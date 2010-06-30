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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DELAY_CONTROLLER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DELAY_CONTROLLER_H_

#include <list>
#include <map>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
class Closure {
 public:
  virtual ~Closure() { }
  virtual void Run() = 0;
};

namespace net_instaweb {

class MessageHandler;

// Class to model a browser's connection to the internet, including overall
// bandwidth, and per-connection limits.
//
// Notation convention:
//   ms = milliseconds
//   us = microseconds
//   ns = nanoseconds
//   percent_x100 = percentage points, scaled up by a factor of 100: 10000=1.0
class DelayController {
 public:
  enum Browser {
    kChrome4,
    kIE8,
    kFirefox36,
    kSafari40,
    kUnitDelay
  };

  static const int kNoTransactionsPending;

  explicit DelayController(Timer* timer);
  ~DelayController();

  // Collect transaction timing in value-change dumps
  void EnableVCD();
  void StartVCD();
  bool WriteVCDFile(const std::string& filename);

  void SetBrowser(Browser browser);

  // Starts a simulated network transaction.  When the transaction is
  // allowed to complete, a callback is called.
  void StartTransaction(int byte_count, StringPiece url, Closure* callback);

  // Process any queued transactions based on the current time.
  void Wakeup();

  // Returns the next time the delay controller should be woken up, in
  // order to process further transactions.  A return value of
  // kNoTransactionsPending indicates that no transactions are pending.
  int64 next_wakeup_time_ms() {return next_wakeup_time_ms_; }

  // Set various parameters that could be used to control network
  // simulation behavior, at some point in the future.  At the moment
  // these are ignored, and we schedule every transaction with a constant
  // delay.
  void set_max_requests(int max_requests) { max_requests_ = max_requests; }
  void set_max_domain_requests(int r) { max_domain_requests_ = r; }
  void set_packet_size_bytes(int c) { packet_size_bytes_ = c; }
  void set_initial_cwnd_size(int num_packets) {
    initial_cwnd_size_ = num_packets;
  }
  void set_rtt_ms(int r) { rtt_ms_ = r; }
  void set_bandwidth_kbytes_per_sec(int b) { bandwidth_kbytes_per_sec_ = b; }

  void Clear();
  void PrintStats(FileSystem::OutputFile* file, MessageHandler* handler) const;
  void ClearStats();

  int max_requests() const { return max_requests_; }
  int max_domain_requests() const { return max_domain_requests_; }
  int initial_cwnd_size() const { return initial_cwnd_size_; }
  int packet_size_bytes() const { return packet_size_bytes_; }
  int rtt_ms() const { return rtt_ms_; }
  int bandwidth_kbytes_per_sec() const { return bandwidth_kbytes_per_sec_; }
  int bandwidth_bytes_per_ms() const { return bandwidth_kbytes_per_sec_; }
  int64 vcd_start_ms() const { return vcd_start_ms_; }
  bool vcd_recording() const { return vcd_recording_; }

 private:
  class Connection;
  class Domain;
  class Request;
  class RequestOrder;

  void ApplyNextChange(int64 now_ms);
  Request* FindExecutableTask();
  void UpdateActiveTasks(int64 now_ms);
  void Settle(int64 now_ms);
  int64 NowMs();

  // Network configuration parameters
  int max_requests_;
  int max_domain_requests_;
  int initial_cwnd_size_;
  int packet_size_bytes_;
  int rtt_ms_;
  int bandwidth_kbytes_per_sec_;  // this is also bytes_per_ms.

  // VCD files are inconvenient to look at with absolute time, so
  // capture our first start-time and base all our timestamps as
  // offsets from that.
  int64 vcd_start_ms_;

  // Current state of our network system
  Timer* timer_;
  int64 next_wakeup_time_ms_;

  // Pending requests are kept in a separate queue per domain.
  typedef std::map<std::string, Domain*> DomainMap;
  DomainMap domain_map_;

  // We also have a list of pending requests across all domains,
  // although due to connection-domain limits we may not initiate the requests
  // in the exact order they were made.
  typedef std::list<Request*> RequestList;
  typedef RequestList::iterator RequestListIter;
  RequestList pending_requests_;
  RequestList active_requests_;
  bool vcd_recording_;
  int prev_num_active_;  // helps determine whether to add a new 'active' event
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DELAY_CONTROLLER_H_
