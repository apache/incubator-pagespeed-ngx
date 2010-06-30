/**
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/delay_controller.h"

#include <stdio.h>
#include <algorithm>
#include <set>
#include <vector>
#include "base/callback.h"
#include "base/logging.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

const int DelayController::kNoTransactionsPending = -1;

/*
 * Our coarse model of the network, from the perspective of a browser,
 * incorporates a model of:
 *    the client's overall bandwidth
 *    the total number of connections
 *    the number of connections per domain
 *    total bandwidth
 *    overhead per packet
 *    congestion window (http://en.wikipedia.org/wiki/Congestion_window)
 *
 * These constraints imply that the latency of a request is affected by
 * the presence of other outstanding requests, and by new requests made
 * while it's in transit.
 *
 * In other words, every time a new request comes in, the existing
 * requests must be re-evaluated to determine what their likely order is.
 * We handle this as lazily as possible.  We only need to compute the
 * next wakeup time, and can re-evaluate that every time we get a new
 * request.
 *
 * One advantage we have in our modeling environment is that the actual
 * request is completed up front, so we know what the byte-count will be
 * when we initiate a request.
 *
 * TODO(jmarantz): at this point we ignore packet loss.
 * TODO(jmarantz): Due to limitations in webkit_headless, we are not
 *      currently modeling incremental delivery of bytes.  The client
 *      gets nothing until the transfer is complete, then the client
 *      gets all the bytes.
 */

// Keeps track of an open connnection to a domain, in particular
// its congestion window (cwnd) growth
class DelayController::Connection {
 public:
  explicit Connection(DelayController* dc)
      : delay_controller_(dc),
        cwnd_size_(dc->initial_cwnd_size()) {
  }

  // Grow the congestion window if it hasn't been saturatd
  void GrowCwnd() {
    if (!IsSaturated()) {
      cwnd_size_ *= 2;
    }
  }

  int cwnd_size() const { return cwnd_size_; }

 private:
  // Computes the maximum number of bytes that can be transferred in
  // one RTT.
  int BytesPerRtt() const {
    return delay_controller_->rtt_ms() *
        delay_controller_->bandwidth_bytes_per_ms();
  }

  // Determines whether the connection is saturated.
  bool IsSaturated() const {
    return cwnd_size_ * delay_controller_->packet_size_bytes() >= BytesPerRtt();
  }

  DelayController* delay_controller_;
  int cwnd_size_;
};

// Keeps the current connection-count for a domain.
class DelayController::Domain {
 public:
  explicit Domain(const std::string& name, DelayController* dc)
      : name_(name),
        active_count_(0),
        byte_count_(0),
        requests_(0),
        delay_controller_(dc) {
    for (int i = 0; i < dc->max_domain_requests(); ++i) {
      free_connections_.push_back(new Connection(dc));
    }
  }

  ~Domain() {
    STLDeleteContainerPointers(free_connections_.begin(),
                               free_connections_.end());
  }

  void UpdateVCD(int64 now_ms) {
  }

  // Re-use connections previously warmed up for this fetch, so we
  // take advantage of keep-alive and congestion-window growth.
  //
  // TOOD(jmarantz): observe keep-alive headers on transfers.
  Connection* StartTransfer(int64 now_ms) {
    CHECK(!free_connections_.empty());
    Connection* connection = free_connections_.back();
    free_connections_.pop_back();
    UpdateVCD(now_ms);
    return connection;
  }

  void add_bytes(int n) {
    byte_count_ += n;
    ++requests_;
  }

  void FinishTransfer(int64 now_ms, Connection* connection) {
    CHECK(active_count() > 0);
    free_connections_.push_back(connection);
    UpdateVCD(now_ms);
  }

  void PrintStats(FileSystem::OutputFile* file, MessageHandler* handler) const {
    file->Write(StringPrintf("Domain %s,%d,%d\n", name_.c_str(), byte_count_,
                             requests_).c_str(), handler);
    for (StringSet::const_iterator p = urls_.begin(), e = urls_.end();
         p != e; ++e) {
      file->Write(StringPrintf("Url: %s\n", p->c_str()).c_str(), handler);
    }
  }

  int active_count() const {
    return delay_controller_->max_domain_requests() - free_connections_.size();
  }

  void ClearStats() {
    byte_count_ = 0;
    requests_ = 0;
    urls_.clear();
  }

 private:
  std::string name_;
  int active_count_;
  int byte_count_;
  int requests_;
  std::vector<Connection*> free_connections_;
  DelayController* delay_controller_;

  typedef std::set<std::string> StringSet;
  StringSet urls_;
};

// Tracks a request as it transfers over, taking into account RTT and
// network bandwidth.
class DelayController::Request {
 public:
  Request(const std::string& url, DelayController* dc,
          int size_bytes, Closure* callback,
          Domain* domain)
      : delay_controller_(dc),
        url_(url),
        size_bytes_(size_bytes),
        completed_bytes_(0),
        completed_rtt_ms_(0),
        remaining_packet_ms_(0),
        previous_update_ms_(0),
        delta_ms_(0),
        callback_(callback),
        domain_(domain),
        connection_(NULL),
        in_payload_(false) {
  }

  bool IsPayloadReady() const {
    // Guaranteed by AdvanceRtt
    CHECK(completed_rtt_ms_ <= delay_controller_->rtt_ms());
    return (completed_rtt_ms_ == delay_controller_->rtt_ms());
  }

  int NextRttLatency() {
    return delay_controller_->rtt_ms() - completed_rtt_ms_;
  }

  int PacketLatency(int bytes) const {
    return CeilDivide(bytes, delay_controller_->bandwidth_bytes_per_ms());
  }

  // Advance the RTT of a request.  When we get to the end of the RTT,
  // compute the amount of time it will require to send the next cwnd,
  // based on the connection's cwnd growth size and the byte-size of the
  // transfer.
  void AdvanceRtt(int64 now_ms) {
    if (ComputeDelta(now_ms)) {
      if (!IsPayloadReady()) {
        completed_rtt_ms_ = std::min(completed_rtt_ms_ + delta_ms_,
                                     delay_controller_->rtt_ms());
        if (IsPayloadReady()) {
          ComputeNextPayload();
        }
      }
    }
  }

  void ComputeNextPayload() {
    remaining_packet_ms_ = PacketLatency(TransferSizeBytes());
  }

  int remaining_packet_ms() const { return remaining_packet_ms_; }

  int TransferSizeBytes() const {
    int bytes_remaining = size_bytes_ - completed_bytes_;
    int cwnd_bytes = delay_controller_->packet_size_bytes() *
      connection_->cwnd_size();
    int transfer_size = std::min(bytes_remaining, cwnd_bytes);
    int max_transfer_size_bytes = delay_controller_->rtt_ms() *
        delay_controller_->bandwidth_bytes_per_ms();
    return std::min(transfer_size, max_transfer_size_bytes);
  }

  bool IsExecutable(int max_domain_requests) {
    return (domain_->active_count() < max_domain_requests);
  }

  void Start(int64 now_ms) {
    CHECK(connection_ == NULL);
    connection_ = domain_->StartTransfer(now_ms);
    domain_->add_bytes(size_bytes_);
    previous_update_ms_ = now_ms;
  }

  bool ComputeDelta(int64 now_ms) {
    bool ret = false;
    if (now_ms > previous_update_ms_) {
      delta_ms_ = now_ms - previous_update_ms_;
      previous_update_ms_ = now_ms;
      ret = true;
    }
    return ret;
  }

  bool AdvancePayload(int64 now_ms) {
    bool ret = false;

    // Every time we update, we get a new client-side bandwidth (bytes_per_ms)
    // and so the latency may change.
    in_payload_ = true;
    if (ComputeDelta(now_ms)) {
      remaining_packet_ms_ -= delta_ms_;
      CHECK(remaining_packet_ms_ >= 0);
      if (remaining_packet_ms_ == 0) {
        int bytes = TransferSizeBytes();
        completed_bytes_ += bytes;

        // When the payload completes, we "credit" that in the next RTT.  As
        // the CWND grows, eventually the RTT will be completely hidden in
        // the payload transfer.
        completed_rtt_ms_ = std::min(PacketLatency(bytes),
                                     delay_controller_->rtt_ms());
        connection_->GrowCwnd();
        if (completed_bytes_ == size_bytes_) {
          domain_->FinishTransfer(now_ms, connection_);
          connection_ = NULL;
          callback_->Run();
          ret = true;
        } else if (IsPayloadReady()) {
          // This indicates that the link is saturated, and we are ready
          // to immediately embark on the next packet without waiting for an
          // rtt.
          ComputeNextPayload();
          in_payload_ = false;  // Yields payload to another request.
        }
      }
    }
    return ret;
  }

  bool in_payload() const { return in_payload_; }

 private:
  static int CeilDivide(int numer, int denom) {
    return (numer + (denom - 1)) / denom;
  }

  DelayController* delay_controller_;
  std::string url_;
  int size_bytes_;
  int completed_bytes_;
  int completed_rtt_ms_;
  int remaining_packet_ms_;
  int64 previous_update_ms_;
  int delta_ms_;
  Closure* callback_;
  Domain* domain_;
  Connection* connection_;
  bool in_payload_;
};

DelayController::DelayController(Timer* timer)
    : max_requests_(0),
      max_domain_requests_(0),
      initial_cwnd_size_(2),
      packet_size_bytes_(1500),
      rtt_ms_(0),
      bandwidth_kbytes_per_sec_(0),
      vcd_start_ms_(0),
      timer_(timer),
      next_wakeup_time_ms_(kNoTransactionsPending),
      vcd_recording_(false),
      prev_num_active_(0) {
}

DelayController::~DelayController() {
  Clear();
}

int64 DelayController::NowMs() {
  int64 now_ms = timer_->NowMs();
  if (vcd_start_ms_ == 0) {
    vcd_start_ms_ = now_ms;
  }
  return now_ms;
}

void DelayController::StartTransaction(
    int byte_count, StringPiece url, Closure* callback) {
  int64 now_ms = NowMs();
  CHECK(byte_count > 0);  // There should always be headers.

  // Settle the network up until the current time before adding in the
  // new transaction.  The new transaction may affect the bandwidth for
  // active transactions, and it shouldn't do so until we have brought
  // all transactions up-to-date.
  Settle(now_ms);

  GURL gurl(url.as_string().c_str());
  std::string host = gurl.host();
  Domain* domain = domain_map_[host];
  if (domain == NULL) {
    domain = new Domain(host, this);
    domain_map_[host] = domain;
  }
  Request* request = new Request(
      url.as_string(), this, byte_count, callback, domain);
  if ((active_requests_.size() < static_cast<size_t>(max_requests_)) &&
      request->IsExecutable(max_domain_requests_)) {
    request->Start(now_ms);
    active_requests_.push_back(request);
    Settle(now_ms);
  } else {
    pending_requests_.push_back(request);
  }
  ApplyNextChange(now_ms);
}

// Find a task whose domain connections are not saturated, and execute it.
DelayController::Request* DelayController::FindExecutableTask() {
  // TODO(jmarantz): this can take a long time if we have a lot of
  // requests queued up for a single domain.  Consider moving the
  // pending requests list into the Domain object, and keeping the
  // Domains sorted by number of active transactions.
  Request* executable_request = NULL;
  if (active_requests_.size() < static_cast<size_t>(max_requests_)) {
    for (RequestListIter iter = pending_requests_.begin();
         (executable_request == NULL) && (iter != pending_requests_.end());
         ++iter) {
      Request* request = *iter;
      if (request->IsExecutable(max_domain_requests_)) {
        iter = pending_requests_.erase(iter);
        executable_request = request;
      }
    }
  }
  return executable_request;
}

void DelayController::ApplyNextChange(int64 now_ms) {
  int min_latency_ms = kNoTransactionsPending;
  bool update_pending = false;

  if (!active_requests_.empty()) {
    // In our model, only one request transfers bytes at a time, but all
    // active request can work off through pending RTT.
    //
    // active_requests_ == is the set of requests with open connections
    // current == the request, if any, that is currently trasnfering payload.
    //
    // It's possible that all active requests are currently in the middle of
    // their RTT, so no payloads are active.
    Request* current = active_requests_.front();
    if (current->IsPayloadReady()) {
      active_requests_.pop_front();
      if (current->AdvancePayload(now_ms)) {
        delete current;
        current = NULL;
        Request* request = FindExecutableTask();
        if (request != NULL) {
          active_requests_.push_back(request);
          request->Start(now_ms);
          update_pending = true;
        }
      } else if (current->in_payload() && current->IsPayloadReady()) {
        // We woke up the simulation in the middle of current's cwnd, so
        // leave it in front to continue the current transfer.
        active_requests_.push_front(current);
        min_latency_ms = current->remaining_packet_ms();
      } else {
        // 'current' is now stuck at a new RTT.  Rotate it to the back
        // of the active queue, and let a new request start transferring
        // bytes while 'current' works through its RTT.
        active_requests_.push_back(current);
        current = NULL;
      }
    } else {
      current = NULL;
    }

    // Work through RTT for all requests that are not current, and
    // find the delta before the next interesting event occurs,
    // e.g. the end of the next payload's cwnd, or the next time a
    // request's RTT finishes.
    //
    // TODO(jmarantz): consider using a priority queue and updating the
    // tasks less frequently.
    for (RequestListIter iter = active_requests_.begin();
         (current == NULL) && (iter != active_requests_.end()); ++iter) {
      Request* request = *iter;
      request->AdvanceRtt(now_ms);
      if (request->IsPayloadReady()) {
        current = request;
        // If this request has finished its RTT and is ready to transfer another
        // chunk, then move it to the front of the queue if necessary.
        if (iter != active_requests_.begin()) {
          iter = active_requests_.erase(iter);
          active_requests_.push_front(request);
        }
        min_latency_ms = request->remaining_packet_ms();
      } else {
        int64 rtt_latency_ms = request->NextRttLatency();
        if ((min_latency_ms == kNoTransactionsPending) ||
            (rtt_latency_ms < min_latency_ms)) {
          min_latency_ms = rtt_latency_ms;
        }
      }
    }
  }


  if (min_latency_ms == kNoTransactionsPending) {
    next_wakeup_time_ms_ = kNoTransactionsPending;
  } else {
    next_wakeup_time_ms_ = now_ms + min_latency_ms;
  }
}

void DelayController::Settle(int64 now_ms) {
  // If time has advanced past the next completion event, then
  // walk time forward to that event.  When we retire the transaction
  // we can update the bandwidth and re-evaluate the latencies of the
  // other in-flight transactions.
  while ((next_wakeup_time_ms_ != kNoTransactionsPending) &&
         (now_ms >= next_wakeup_time_ms_)) {
    int64 prev_wakeup_time_ms = next_wakeup_time_ms_;
    ApplyNextChange(prev_wakeup_time_ms);
    CHECK(prev_wakeup_time_ms != next_wakeup_time_ms_);  // ensure progress
  }
}

void DelayController::Wakeup() {
  int64 now_ms = NowMs();
  Settle(now_ms);
}

void DelayController::SetBrowser(Browser browser) {
  // TODO(jmarantz): separate browser-based params from network/machine-based
  // params.
  switch (browser) {
    case kUnitDelay:
      max_requests_ = 10000;
      max_domain_requests_ = 10000;
      packet_size_bytes_ = 1000000;
      rtt_ms_ = 1;
      bandwidth_kbytes_per_sec_ = 10000000;
      break;
    default:
      // TODO(jmarantz): this is for chrome.  Use browserscope.org for other
      // browsers.
      max_requests_ = 53;
      max_domain_requests_ = 6;
      rtt_ms_ = 50;
      bandwidth_kbytes_per_sec_ = 500;
      break;
  }
}




void DelayController::Clear() {
  STLDeleteContainerPairSecondPointers(domain_map_.begin(), domain_map_.end());
  domain_map_.clear();
  STLDeleteContainerPointers(pending_requests_.begin(),
                             pending_requests_.end());
  pending_requests_.clear();
  STLDeleteContainerPointers(active_requests_.begin(), active_requests_.end());
  active_requests_.clear();
  vcd_start_ms_ = 0;
  next_wakeup_time_ms_ = kNoTransactionsPending;
  vcd_recording_ = false;
  prev_num_active_ = 0;
}

void DelayController::PrintStats(FileSystem::OutputFile* file,
                                 MessageHandler* handler) const {
  for (DomainMap::const_iterator p = domain_map_.begin(), e = domain_map_.end();
       p != e; ++p) {
    p->second->PrintStats(file, handler);
  }
}

void DelayController::ClearStats()  {
  for (DomainMap::iterator p = domain_map_.begin(), e = domain_map_.end();
       p != e; ++p) {
    p->second->ClearStats();
  }
}

}  // namespace net_instaweb
