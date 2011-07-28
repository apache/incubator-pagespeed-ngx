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

#ifndef NET_INSTAWEB_APACHE_SERF_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_APACHE_SERF_URL_ASYNC_FETCHER_H_

#include <set>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"

struct apr_pool_t;
struct serf_context_t;
struct apr_thread_mutex_t;

namespace net_instaweb {

class Statistics;
class SerfFetch;
class SerfThreadedFetcher;
class Timer;
class Variable;

struct SerfStats {
  static const char kSerfFetchRequestCount[];
  static const char kSerfFetchByteCount[];
  static const char kSerfFetchTimeDurationMs[];
  static const char kSerfFetchCancelCount[];
  static const char kSerfFetchActiveCount[];
  static const char kSerfFetchTimeoutCount[];
};

class SerfUrlAsyncFetcher : public UrlPollableAsyncFetcher {
 public:
  SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool,
                      ThreadSystem* thread_system,
                      Statistics* statistics, Timer* timer, int64 timeout_ms);
  SerfUrlAsyncFetcher(SerfUrlAsyncFetcher* parent, const char* proxy);
  virtual ~SerfUrlAsyncFetcher();
  static void Initialize(Statistics* statistics);
  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* fetched_content_writer,
                              MessageHandler* message_handler,
                              UrlAsyncFetcher::Callback* callback);

  virtual int Poll(int64 max_wait_ms);

  enum WaitChoice {
    kThreadedOnly,
    kMainlineOnly,
    kThreadedAndMainline
  };

  bool WaitForActiveFetches(int64 max_milliseconds,
                            MessageHandler* message_handler,
                            WaitChoice wait_choice);

  // Remove the completed fetch from the active fetch set, and put it into a
  // completed fetch list to be cleaned up.
  void FetchComplete(SerfFetch* fetch);
  apr_pool_t* pool() const { return pool_; }
  serf_context_t* serf_context() const { return serf_context_; }

  void PrintActiveFetches(MessageHandler* handler) const;
  virtual int64 timeout_ms() { return timeout_ms_; }
  ThreadSystem* thread_system() { return thread_system_; }

 protected:
  typedef Pool<SerfFetch> SerfFetchPool;

  void Init(apr_pool_t* parent_pool, const char* proxy);
  bool SetupProxy(const char* proxy);

  // AnyPendingFetches is accurate only at the time of call; this is
  // used conservatively during shutdown.  It counts fetches that have been
  // requested by some thread, and can include fetches for which no action
  // has yet been taken (ie fetches that are not active).
  virtual bool AnyPendingFetches();
  // ApproximateNumActiveFetches can under- or over-count and is used only for
  // error reporting.
  int ApproximateNumActiveFetches();
  void CancelActiveFetches();
  bool WaitForActiveFetchesHelper(int64 max_ms,
                                  MessageHandler* message_handler);

  // This cleans up the serf resources for fetches that errored out.
  // Must be called only immediately after running the serf event loop.
  // Must be called with mutex_ held.
  void CleanupFetchesWithErrors();

  apr_pool_t* pool_;
  ThreadSystem* thread_system_;
  Timer* timer_;

  // mutex_ protects serf_context_ and active_fetches_.
  ThreadSystem::CondvarCapableMutex* mutex_;
  serf_context_t* serf_context_;
  SerfFetchPool active_fetches_;

  typedef std::vector<SerfFetch*> FetchVector;
  SerfFetchPool completed_fetches_;
  SerfThreadedFetcher* threaded_fetcher_;

  // This is protected because it's updated along with active_fetches_,
  // which happens in subclass SerfThreadedFetcher as well as this class.
  Variable* active_count_;

 private:
  Variable* request_count_;
  Variable* byte_count_;
  Variable* time_duration_ms_;
  Variable* cancel_count_;
  Variable* timeout_count_;
  const int64 timeout_ms_;

  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_SERF_URL_ASYNC_FETCHER_H_
