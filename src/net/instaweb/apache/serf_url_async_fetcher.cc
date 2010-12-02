// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(jmarantz): Avoid initiating fetches for resources already in flight.
// The challenge is that we would want to call all the callbacks that indicated
// interest in a particular URL once the callback completed.  Alternatively,
// this could be done in a level above the URL fetcher.

#include "net/instaweb/apache/serf_url_async_fetcher.h"

#include <algorithm>
#include <string>
#include <vector>

#include "apr_atomic.h"
#include "apr_strings.h"
#include "apr_thread_proc.h"
#include "apr_version.h"
#include "base/basictypes.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "third_party/serf/src/serf.h"
#include "third_party/serf/src/serf_bucket_util.h"

// Until this fetcher has some mileage on it, it is useful to keep around
// an easy way to turn on lots of debug messages.  But they do get a bit chatty
// when things are working well.
#define SERF_DEBUG(x)

namespace {
const int kBufferSize = 2048;
const char kFetchMethod[] = "GET";
}  // namespace

namespace net_instaweb {

const char SerfStats::kSerfFetchRequestCount[] = "serf_fetch_request_count";
const char SerfStats::kSerfFetchByteCount[] = "serf_fetch_bytes_count";
const char SerfStats::kSerfFetchTimeDurationMs[] =
    "serf_fetch_time_duration_ms";
const char SerfStats::kSerfFetchCancelCount[] = "serf_fetch_cancel_count";
const char SerfStats::kSerfFetchOutstandingCount[] =
    "serf_fetch_outstanding_count";
const char SerfStats::kSerfFetchTimeoutCount[] = "serf_fetch_timeout_count";

std::string GetAprErrorString(apr_status_t status) {
  char error_str[1024];
  apr_strerror(status, error_str, sizeof(error_str));
  return error_str;
}

// TODO(lsong): Move this to a separate file. Necessary?
class SerfFetch {
 public:
  // TODO(lsong): make use of request_headers.
  SerfFetch(apr_pool_t* pool,
            const std::string& url,
            const MetaData& request_headers,
            MetaData* response_headers,
            Writer* fetched_content_writer,
            MessageHandler* message_handler,
            UrlAsyncFetcher::Callback* callback,
            Timer* timer)
      : fetcher_(NULL),
        timer_(timer),
        str_url_(url),
        response_headers_(response_headers),
        fetched_content_writer_(fetched_content_writer),
        message_handler_(message_handler),
        callback_(callback),
        connection_(NULL),
        byte_received_(0),
        fetch_start_ms_(0),
        fetch_end_ms_(0) {
    request_headers_.CopyFrom(request_headers);
    apr_pool_create(&pool_, pool);
    bucket_alloc_ = serf_bucket_allocator_create(pool_, NULL, NULL);
  }

  ~SerfFetch() {
    if (connection_ != NULL) {
      serf_connection_close(connection_);
    }
    apr_pool_destroy(pool_);
  }

  // Start the fetch. It returns immediately.  This can only be run when
  // locked with fetcher->mutex_.
  bool Start(SerfUrlAsyncFetcher* fetcher);

  const char* str_url() { return str_url_.c_str(); }

  void Cancel() {
    callback_->Done(false);
    delete this;
  }
  int64 TimeDuration() const {
    if ((fetch_start_ms_ != 0) && (fetch_end_ms_ != 0)) {
      return fetch_end_ms_ - fetch_start_ms_;
    } else {
      return 0;
    }
  }
  int64 fetch_start_ms() const { return fetch_start_ms_; }

  size_t byte_received() const { return byte_received_; }
  MessageHandler* message_handler() { return message_handler_; }

 private:

  // Static functions used in callbacks.
  static serf_bucket_t* ConnectionSetup(
      apr_socket_t* socket, void* setup_baton, apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(setup_baton);
    return serf_bucket_socket_create(socket, fetch->bucket_alloc_);
  }

  static void ClosedConnection(serf_connection_t* conn,
                               void* closed_baton,
                               apr_status_t why,
                               apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(closed_baton);
    if (why != APR_SUCCESS) {
      fetch->message_handler_->Warning(
          fetch->str_url_.c_str(), 0, "Connection close (code=%d %s).",
          why, GetAprErrorString(why).c_str());
    }
    // Connection is closed.
    fetch->connection_ = NULL;
  }

  static serf_bucket_t* AcceptResponse(serf_request_t* request,
                                       serf_bucket_t* stream,
                                       void* acceptor_baton,
                                       apr_pool_t* pool) {
    // Get the per-request bucket allocator.
    serf_bucket_alloc_t* bucket_alloc = serf_request_get_alloc(request);
    // Create a barrier so the response doesn't eat us!
    // From the comment in Serf:
    // ### the stream does not have a barrier, this callback should generally
    // ### add a barrier around the stream before incorporating it into a
    // ### response bucket stack.
    // ... i.e. the passed bucket becomes owned rather than
    // ### borrowed.
    serf_bucket_t* bucket = serf_bucket_barrier_create(stream, bucket_alloc);
    return serf_bucket_response_create(bucket, bucket_alloc);
  }

  static apr_status_t HandleResponse(serf_request_t* request,
                                     serf_bucket_t* response,
                                     void* handler_baton,
                                     apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(handler_baton);
    return fetch->HandleResponse(request, response);
  }

  // The handler MUST process data from the response bucket until the
  // bucket's read function states it would block (APR_STATUS_IS_EAGAIN).
  // The handler is invoked only when new data arrives. If no further data
  // arrives, and the handler does not process all available data, then the
  // system can result in a deadlock around the unprocessed, but read, data.
  apr_status_t HandleResponse(serf_request_t* request,
                              serf_bucket_t* response) {
    apr_status_t status = APR_EGENERAL;
    serf_status_line status_line;
    if ((response != NULL) &&
        ((status = serf_bucket_response_status(response, &status_line))
         == APR_SUCCESS)) {
      response_headers_->SetStatusAndReason(
          static_cast<HttpStatus::Code>(status_line.code));
      response_headers_->set_major_version(status_line.version / 1000);
      response_headers_->set_minor_version(status_line.version % 1000);
      const char* data = NULL;
      apr_size_t len = 0;
      while ((status = serf_bucket_read(response, kBufferSize, &data, &len))
             == APR_SUCCESS || APR_STATUS_IS_EOF(status) ||
             APR_STATUS_IS_EAGAIN(status)) {
        byte_received_ += len;
        if (len > 0 &&
            !fetched_content_writer_->Write(
                StringPiece(data, len), message_handler_)) {
          status = APR_EGENERAL;
          break;
        }
        if (status != APR_SUCCESS) {
          break;
        }
      }
      // We could read the headers earlier, but then we have to check if we
      // have received the headers.  At EOF of response, we have the headers
      // already. Read them.
      if (APR_STATUS_IS_EOF(status)) {
        status = ReadHeaders(response);
      }
    }
    if (!APR_STATUS_IS_EAGAIN(status)) {
      bool success = APR_STATUS_IS_EOF(status);
      fetch_end_ms_ = timer_->NowMs();
      callback_->Done(success);
      fetcher_->FetchComplete(this);
    }
    return status;
  }

  apr_status_t ReadHeaders(serf_bucket_t* response) {
    apr_status_t status = APR_SUCCESS;
    serf_bucket_t* headers = serf_bucket_response_get_headers(response);
    const char* data = NULL;
    apr_size_t num_bytes = 0;
    while ((status = serf_bucket_read(headers, kBufferSize, &data, &num_bytes))
           == APR_SUCCESS || APR_STATUS_IS_EOF(status) ||
           APR_STATUS_IS_EAGAIN(status)) {
      if (response_headers_->headers_complete()) {
        status = APR_EGENERAL;
        message_handler_->Info(str_url_.c_str(), 0,
                               "headers complete but more data coming");
      } else {
        StringPiece str_piece(data, num_bytes);
        apr_size_t parsed_len =
            response_headers_->ParseChunk(str_piece, message_handler_);
        if (parsed_len != num_bytes) {
          status = APR_EGENERAL;
          message_handler_->Error(str_url_.c_str(), 0,
                                  "unexpected bytes at end of header");
        }
      }
      if (status != APR_SUCCESS) {
        break;
      }
    }
    if (APR_STATUS_IS_EOF(status)
        && !response_headers_->headers_complete()) {
      message_handler_->Error(str_url_.c_str(), 0,
                              "eof on incomplete headers code=%d %s",
                              status, GetAprErrorString(status).c_str());
      status = APR_EGENERAL;
    }
    return status;
  }

  static apr_status_t SetupRequest(serf_request_t* request,
                                   void* setup_baton,
                                   serf_bucket_t** req_bkt,
                                   serf_response_acceptor_t* acceptor,
                                   void** acceptor_baton,
                                   serf_response_handler_t* handler,
                                   void** handler_baton,
                                   apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(setup_baton);
    const char* url_path = apr_uri_unparse(pool, &fetch->url_,
                                           APR_URI_UNP_OMITSITEPART);
    *req_bkt = serf_request_bucket_request_create(
        request, kFetchMethod,
        url_path, NULL,
        serf_request_get_alloc(request));
    serf_bucket_t* hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

    bool found_user_agent = false;
    // TODO(abliss): get this from the SerfFetch
    const char* user_agent_suffix =
        " mod_pagespeed/" MOD_PAGESPEED_VERSION_STRING "-" LASTCHANGE_STRING;
    for (int i = 0; i < fetch->request_headers_.NumAttributes(); ++i) {
      const char* name = fetch->request_headers_.Name(i);
      const char* value = fetch->request_headers_.Value(i);
      bool add = false;
      if (strcasecmp(name, HttpAttributes::kUserAgent) == 0) {
        found_user_agent = true;
        value = apr_pstrcat(pool, value, user_agent_suffix, NULL);
        add = true;
      } else if (strcasecmp(name, HttpAttributes::kAcceptEncoding) == 0) {
        add = true;
      } else if (strcasecmp(name, HttpAttributes::kReferer) == 0) {
        add = true;
      } else if (strcasecmp(name, HttpAttributes::kHost) == 0) {
        // Note: this will append the specified Host to the one from
        // the URL.  Serf provides no obvious mechanism to replace
        // an attribute value that it added automatically as a default.
        add = true;
      }
      if (add) {
        serf_bucket_headers_setn(hdrs_bkt, name, value);
      }
    }
    if (!found_user_agent) {
      const char* default_user_agent = apr_pstrcat(
          pool, "Serf/" SERF_VERSION_STRING, user_agent_suffix, NULL);
      serf_bucket_headers_setn(hdrs_bkt, HttpAttributes::kUserAgent,
                               default_user_agent);
    }

    // TODO(jmarantz): add accept-encoding:gzip even if not requested by
    // the caller, but then decompress in the output handler.

    *acceptor = SerfFetch::AcceptResponse;
    *acceptor_baton = fetch;
    *handler = SerfFetch::HandleResponse;
    *handler_baton = fetch;
    return APR_SUCCESS;
  }

  bool ParseUrl() {
    apr_status_t status = 0;
    status = apr_uri_parse(pool_, str_url_.c_str(), &url_);
    if (status != APR_SUCCESS) {
      return false;  // Failed to parse URL.
    }

    // TODO(lsong): We do not handle HTTPS for now. HTTPS needs authentication
    // verifying certificates, etc.
    if (strcasecmp(url_.scheme, "https") == 0) {
      return false;
    }
    if (!url_.port) {
      url_.port = apr_uri_port_of_scheme(url_.scheme);
    }
    if (!url_.path) {
      url_.path = apr_pstrdup(pool_, "/");
    }
    return true;
  }

  SerfUrlAsyncFetcher* fetcher_;
  Timer* timer_;
  const std::string str_url_;
  SimpleMetaData request_headers_;
  MetaData* response_headers_;
  Writer* fetched_content_writer_;
  MessageHandler* message_handler_;
  UrlAsyncFetcher::Callback* callback_;

  apr_pool_t* pool_;
  serf_bucket_alloc_t* bucket_alloc_;
  apr_uri_t url_;
  serf_connection_t* connection_;
  size_t byte_received_;
  int64 fetch_start_ms_;
  int64 fetch_end_ms_;


  DISALLOW_COPY_AND_ASSIGN(SerfFetch);
};

class SerfThreadedFetcher : public SerfUrlAsyncFetcher {
 public:
  SerfThreadedFetcher(SerfUrlAsyncFetcher* parent, const char* proxy) :
      SerfUrlAsyncFetcher(parent, proxy),
      initiate_mutex_(pool_),
      terminate_mutex_(pool_),
      thread_done_(false) {
    terminate_mutex_.Lock();
    CHECK_EQ(APR_SUCCESS,
             apr_thread_create(&thread_id_, NULL, SerfThreadFn, this, pool_));
  }

  ~SerfThreadedFetcher() {
    // Although Cancel will be called in the base class destructor, we
    // want to call it here as well, as it will make it easier for the
    // thread to terminate.
    CancelOutstandingFetches();

    // Let the thread terminate naturally by unlocking its mutexes.
    thread_done_ = true;
    mutex_->Unlock();
    LOG(INFO) << "Waiting for threaded serf fetcher to terminate";
    terminate_mutex_.Lock();
    terminate_mutex_.Unlock();
  }

  // Called from mainline to queue up a fetch for the thread.  If the
  // thread is idle then we can unlock it.
  void InitiateFetch(SerfFetch* fetch) {
    ScopedMutex lock(&initiate_mutex_);
    initiate_fetches_.push_back(fetch);
  }

 private:
  static void* SerfThreadFn(apr_thread_t* thread_id, void* context) {
    SerfThreadedFetcher* stc = static_cast<SerfThreadedFetcher*>(context);
    CHECK_EQ(thread_id, stc->thread_id_);
    stc->SerfThread();
    return NULL;
  }

  // Thread-called function to transfer fetches from initiate_fetches_ vector to
  // the active_fetches_ queue.  Doesn't do any work if initiate_fetches_ is
  // empty.
  void TransferFetches() {
    // Use a temp that to minimize the amount of time we hold the
    // initiate_mutex_ lock, so that the parent thread doesn't get
    // blocked trying to initiate fetches.
    FetchVector xfer_fetches;
    {
      ScopedMutex lock(&initiate_mutex_);
      xfer_fetches.swap(initiate_fetches_);
    }

    // Now that we've unblocked the parent thread, we can leisurely
    // queue up the fetches, employing the proper lock for the active_fetches_
    // set.  Actually we expect we wll never have contention on this mutex
    // from the thread.
    if (!xfer_fetches.empty()) {
      int num_started = 0;
      ScopedMutex lock(mutex_);
      for (int i = 0, n = xfer_fetches.size(); i < n; ++i) {
        SerfFetch* fetch = xfer_fetches[i];
        if (fetch->Start(this)) {
          SERF_DEBUG(LOG(INFO) << "Adding threaded fetch to url "
                     << fetch->str_url()
                     << " (" << active_fetches_.size() << ")");
          active_fetches_.push_back(fetch);
          active_fetch_map_[fetch] = --active_fetches_.end();
          ++num_started;
        } else {
          delete fetch;
        }
      }
      if ((num_started != 0) && (outstanding_count_ != NULL)) {
        outstanding_count_->Add(num_started);
      }
    }
  }

  void SerfThread() {
    while (!thread_done_) {
      // If initiate_fetches is empty, we will not do any work.
      TransferFetches();

      const int64 kPollIntervalUs = 500000;
      SERF_DEBUG(LOG(INFO) << "Polling from serf thread (" << this << ")");
      // If active_fetches_ is empty, we will not do any work.
      int num_outstanding_fetches = Poll(kPollIntervalUs);
      SERF_DEBUG(LOG(INFO) << "Finished polling from serf thread ("
                 << this << ")");
      // We don't want to spin busily waiting for new fetches.  We could use a
      // semaphore, but we're not really concerned with latency here, so we can
      // just check every once in a while.
      if (num_outstanding_fetches == 0) {
        sleep(1);
      }
    }
    terminate_mutex_.Unlock();
  }

  apr_thread_t* thread_id_;

  // protects initiate_fetches_
  AprMutex initiate_mutex_;
  // pushed in the main thread; popped in the serf thread.
  std::vector<SerfFetch*> initiate_fetches_;

  // Allows parent to block till thread exits
  AprMutex terminate_mutex_;
  bool thread_done_;

  DISALLOW_COPY_AND_ASSIGN(SerfThreadedFetcher);
};

bool SerfFetch::Start(SerfUrlAsyncFetcher* fetcher) {
  fetch_start_ms_ = timer_->NowMs();
  fetcher_ = fetcher;
  // Parse and validate the URL.
  if (!ParseUrl()) {
    return false;
  }

  apr_status_t status = serf_connection_create2(&connection_,
                                                fetcher_->serf_context(),
                                                url_,
                                                ConnectionSetup, this,
                                                ClosedConnection, this,
                                                pool_);
  if (status != APR_SUCCESS) {
    message_handler_->Error(str_url_.c_str(), 0,
                            "Error status=%d (%s) serf_connection_create2",
                            status, GetAprErrorString(status).c_str());
    return false;
  }
  serf_connection_request_create(connection_, SetupRequest, this);

  // Start the fetch. It will connect to the remote host, send the request,
  // and accept the response, without blocking.
  status = serf_context_run(fetcher_->serf_context(), 0, fetcher_->pool());

  if (status == APR_SUCCESS || APR_STATUS_IS_TIMEUP(status)) {
    return true;
  } else {
    message_handler_->Error(str_url_.c_str(), 0,
                            "serf_context_run error status=%d (%s)",
                            status, GetAprErrorString(status).c_str());
    return false;
  }
}


// Set up the proxy for all the connections in the context. The proxy is in the
// format of hostname:port.
bool SerfUrlAsyncFetcher::SetupProxy(const char* proxy) {
  apr_status_t status = 0;
  if (proxy == NULL || *proxy == '\0') {
    return true;  // No proxy to be set.
  }

  apr_sockaddr_t* proxy_address = NULL;
  apr_port_t proxy_port;
  char* proxy_host;
  char* proxy_scope;
  status = apr_parse_addr_port(&proxy_host, &proxy_scope, &proxy_port, proxy,
                               pool_);
  if (status != APR_SUCCESS || proxy_host == NULL || proxy_port == 0 ||
      (status = apr_sockaddr_info_get(&proxy_address, proxy_host, APR_UNSPEC,
                                      proxy_port, 0, pool_)) != APR_SUCCESS) {
    return false;
  }
  serf_config_proxy(serf_context_, proxy_address);
  return true;
}

SerfUrlAsyncFetcher::SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool,
                                         Statistics* statistics, Timer* timer,
                                         int64 timeout_ms)
    : pool_(pool),
      timer_(timer),
      mutex_(NULL),
      serf_context_(NULL),
      threaded_fetcher_(NULL),
      outstanding_count_(NULL),
      request_count_(NULL),
      byte_count_(NULL),
      time_duration_ms_(NULL),
      cancel_count_(NULL),
      timeout_count_(NULL),
      timeout_ms_(timeout_ms) {
  if (statistics != NULL) {
    request_count_  =
        statistics->GetVariable(SerfStats::kSerfFetchRequestCount);
    byte_count_ = statistics->GetVariable(SerfStats::kSerfFetchByteCount);
    time_duration_ms_ =
        statistics->GetVariable(SerfStats::kSerfFetchTimeDurationMs);
    cancel_count_ = statistics->GetVariable(SerfStats::kSerfFetchCancelCount);
    outstanding_count_ = statistics->GetVariable(
        SerfStats::kSerfFetchOutstandingCount);
    timeout_count_ = statistics->GetVariable(
        SerfStats::kSerfFetchTimeoutCount);
  }
  mutex_ = new AprMutex(pool_);
  serf_context_ = serf_context_create(pool_);
  threaded_fetcher_ = new SerfThreadedFetcher(this, proxy);
  if (!SetupProxy(proxy)) {
    LOG(WARNING) << "Proxy failed: " << proxy;
  }
}

SerfUrlAsyncFetcher::SerfUrlAsyncFetcher(SerfUrlAsyncFetcher* parent,
                                         const char* proxy)
    : pool_(parent->pool_),
      timer_(parent->timer_),
      mutex_(NULL),
      serf_context_(NULL),
      threaded_fetcher_(NULL),
      outstanding_count_(parent->outstanding_count_),
      request_count_(parent->request_count_),
      byte_count_(parent->byte_count_),
      time_duration_ms_(parent->time_duration_ms_),
      cancel_count_(parent->cancel_count_),
      timeout_count_(parent->timeout_count_),
      timeout_ms_(parent->timeout_ms()) {
  mutex_ = new AprMutex(pool_);
  serf_context_ = serf_context_create(pool_);
  threaded_fetcher_ = NULL;
  if (!SetupProxy(proxy)) {
    LOG(WARNING) << "Proxy failed: " << proxy;
  }
}

SerfUrlAsyncFetcher::~SerfUrlAsyncFetcher() {
  CancelOutstandingFetches();
  int orphaned_fetches = active_fetches_.size();
  if (orphaned_fetches != 0) {
    LOG(ERROR) << "SerfFecher destructed with " << orphaned_fetches
               << " orphaned fetches.";
    if (outstanding_count_ != NULL) {
      outstanding_count_->Add(-orphaned_fetches);
    }
    if (cancel_count_ != NULL) {
      cancel_count_->Add(orphaned_fetches);
    }
  }

  STLDeleteElements(&active_fetches_);
  active_fetch_map_.clear();
  if (threaded_fetcher_ != NULL) {
    delete threaded_fetcher_;
  }
  delete mutex_;
}

SerfUrlAsyncFetcher::FetchQueueEntry SerfUrlAsyncFetcher::EraseFetch(
    SerfFetch* fetch) {
  FetchMapEntry map_entry = active_fetch_map_.find(fetch);
  CHECK(map_entry != active_fetch_map_.end())
      << "Active fetch not in map: " << fetch->str_url();
  FetchQueueEntry entry = active_fetches_.erase(map_entry->second);
  active_fetch_map_.erase(map_entry);
  return entry;
}

void SerfUrlAsyncFetcher::CancelOutstandingFetches() {
  // If there are still active requests, cancel them.
  int num_canceled = 0;
  {
    ScopedMutex lock(mutex_);
    while (!active_fetches_.empty()) {
      FetchQueueEntry p = active_fetches_.begin();
      SerfFetch* fetch = *p;
      LOG(WARNING) << "Aborting fetch of " << fetch->str_url();
      EraseFetch(fetch);
      ++num_canceled;
      fetch->Cancel();
    }
  }
  if (num_canceled != 0) {
    if (cancel_count_ != NULL) {
      cancel_count_->Add(num_canceled);
    }
    if (outstanding_count_ != NULL) {
      outstanding_count_->Add(-num_canceled);
    }
  }
}

bool SerfUrlAsyncFetcher::StreamingFetch(const std::string& url,
                                         const MetaData& request_headers,
                                         MetaData* response_headers,
                                         Writer* fetched_content_writer,
                                         MessageHandler* message_handler,
                                         UrlAsyncFetcher::Callback* callback) {
  SerfFetch* fetch = new SerfFetch(
      pool_, url, request_headers, response_headers, fetched_content_writer,
      message_handler, callback, timer_);
  if (request_count_ != NULL) {
    request_count_->Add(1);
  }
  if (callback->EnableThreaded()) {
    message_handler->Message(kInfo, "Initiating async fetch for %s",
                             url.c_str());
    threaded_fetcher_->InitiateFetch(fetch);
  } else {
    message_handler->Message(kInfo, "Initiating blocking fetch for %s",
                             url.c_str());
    bool started = false;
    {
      ScopedMutex mutex(mutex_);
      started = fetch->Start(this);
      if (started) {
        active_fetches_.push_back(fetch);
        active_fetch_map_[fetch] = --active_fetches_.end();
        if (outstanding_count_ != NULL) {
          outstanding_count_->Add(1);
        }
      } else {
        delete fetch;
      }
    }
  }
  return false;
}

void SerfUrlAsyncFetcher::PrintOutstandingFetches(
    MessageHandler* handler) const {
  ScopedMutex mutex(mutex_);
  for (FetchQueue::const_iterator p = active_fetches_.begin(),
           e = active_fetches_.end(); p != e; ++p) {
    SerfFetch* fetch = *p;
    handler->Message(kInfo, "Outstanding fetch: %s",
                     fetch->str_url());
  }
}

// If active_fetches_ is empty, this does no work and returns 0.
int SerfUrlAsyncFetcher::Poll(int64 microseconds) {
  // Run serf polling up to microseconds.
  ScopedMutex mutex(mutex_);
  if (!active_fetches_.empty()) {
    apr_status_t status = serf_context_run(serf_context_, microseconds, pool_);
    STLDeleteElements(&completed_fetches_);
    if (APR_STATUS_IS_TIMEUP(status)) {
      // Remove expired fetches from the front of the queue.
      int64 stale_cutoff = timer_->NowMs() - timeout_ms_;
      FetchQueueEntry p = active_fetches_.begin(), e = active_fetches_.end();
      int timeouts = 0;
      while ((p != e) && ((*p)->fetch_start_ms() < stale_cutoff)) {
        SerfFetch* fetch = *p;
        LOG(WARNING) << "Fetch timed out: " << fetch->str_url();
        timeouts++;
        fetch->Cancel();
        p = EraseFetch(fetch);
      }
      if ((timeouts > 0) && (timeout_count_ != NULL)) {
        timeout_count_->Add(timeouts);
      }
    }
    bool success = ((status == APR_SUCCESS) || APR_STATUS_IS_TIMEUP(status));
    // TODO(jmarantz): provide the success status to the caller if there is a
    // need.
    if (!success && !active_fetches_.empty()) {
      // TODO(jmarantz): I have a new theory that we are getting
      // behind when our self-directed URL fetches queue up multiple
      // requests for the same URL, which might be sending the Serf
      // library into an n^2 situation with its polling, even though
      // we are using an rb_tree to hold the outstanding fetches.  We
      // should fix this by keeping a map from url->SerfFetch, where
      // we'd have to store lists of Callback*, ResponseHeader*, Writer* so
      // all interested parties were updated if and when the fetch finally
      // completed.
      //
      // In the meantime by putting more detail into the log here, we'll
      // know whether we are accumulating outstanding fetches to make the
      // server fall over.
      LOG(ERROR) << "Serf status " << status << " ("
                 << GetAprErrorString(status) << " ) polling for "
                 << active_fetches_.size()
                 << ((threaded_fetcher_ == NULL) ? ": (threaded)"
                     : ": (non-blocking)")
                 << " (" << this << ") for " << microseconds/1.0e6
                 << " seconds";
    }
  }
  return active_fetches_.size();
}

void SerfUrlAsyncFetcher::FetchComplete(SerfFetch* fetch) {
  // We do not have a ScopedMutex in FetchComplete, because it is only
  // called from Poll, which has a ScopedMutex.
  EraseFetch(fetch);
  fetch->message_handler()->Message(kInfo, "Fetch complete: %s",
                                    fetch->str_url());
  completed_fetches_.push_back(fetch);
  if (time_duration_ms_) {
    time_duration_ms_->Add(fetch->TimeDuration());
  }
  if (byte_count_) {
    byte_count_->Add(fetch->byte_received());
  }
  if (outstanding_count_) {
    outstanding_count_->Add(-1);
  }
}

size_t SerfUrlAsyncFetcher::NumActiveFetches() {
  ScopedMutex lock(mutex_);
  return active_fetches_.size();
}

bool SerfUrlAsyncFetcher::WaitForInProgressFetches(
    int64 max_ms, MessageHandler* message_handler, WaitChoice wait_choice) {
  bool ret = true;
  if ((threaded_fetcher_ != NULL) && (wait_choice != kMainlineOnly)) {
    ret &= threaded_fetcher_->WaitForInProgressFetchesHelper(
        max_ms, message_handler);
  }
  if (wait_choice != kThreadedOnly) {
    ret &= WaitForInProgressFetchesHelper(max_ms, message_handler);
  }
  return ret;
}

bool SerfUrlAsyncFetcher::WaitForInProgressFetchesHelper(
    int64 max_ms, MessageHandler* message_handler) {
  int num_active_fetches = NumActiveFetches();
  if (num_active_fetches != 0) {
    int64 now_ms = timer_->NowMs();
    int64 end_ms = now_ms + max_ms;
    while ((now_ms < end_ms) && (num_active_fetches != 0)) {
      int64 remaining_ms = end_ms - now_ms;
      SERF_DEBUG(LOG(INFO) << "Blocking process waiting " << remaining_ms
                 << "ms for " << active_fetches_.size() << " to complete");
      SERF_DEBUG(PrintOutstandingFetches(message_handler));
      Poll(1000 * remaining_ms);
      now_ms = timer_->NowMs();
      num_active_fetches = NumActiveFetches();
    }
    if (!active_fetches_.empty()) {
      message_handler->Message(
          kError, "Serf timeout waiting for %d to complete",
          num_active_fetches);
      return false;
    }
    SERF_DEBUG(LOG(INFO) << "Serf successfully completed outstanding fetches");
  }
  return true;
}
void SerfUrlAsyncFetcher::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(SerfStats::kSerfFetchRequestCount);
    statistics->AddVariable(SerfStats::kSerfFetchByteCount);
    statistics->AddVariable(SerfStats::kSerfFetchTimeDurationMs);
    statistics->AddVariable(SerfStats::kSerfFetchCancelCount);
    statistics->AddVariable(SerfStats::kSerfFetchOutstandingCount);
    statistics->AddVariable(SerfStats::kSerfFetchTimeoutCount);
  }
}

}  // namespace net_instaweb
