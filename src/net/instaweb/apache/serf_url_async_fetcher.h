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
#include "base/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

struct apr_pool_t;
struct serf_context_t;
struct apr_thread_mutex_t;

namespace net_instaweb {

class AprMutex;
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
  static const char kSerfFetchOutstandingCount[];
};

class SerfUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // TODO(abliss): we currently ignore timeout_ms.
  SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool,
                      Statistics* statistics, Timer* timer, int64 timeout_ms);
  SerfUrlAsyncFetcher(SerfUrlAsyncFetcher* parent, const char* proxy);
  virtual ~SerfUrlAsyncFetcher();
  static void Initialize(Statistics* statistics);
  virtual bool StreamingFetch(const std::string& url,
                              const MetaData& request_headers,
                              MetaData* response_headers,
                              Writer* fetched_content_writer,
                              MessageHandler* message_handler,
                              UrlAsyncFetcher::Callback* callback);

  // Poll the active fetches, returning the number of fetches
  // still outstanding.
  int Poll(int64 microseconds);

  enum WaitChoice {
    kThreadedOnly,
    kMainlineOnly,
    kThreadedAndMainline
  };

  bool WaitForInProgressFetches(int64 max_milliseconds,
                                MessageHandler* message_handler,
                                WaitChoice wait_choice);

  // Remove the completed fetch from the active fetch set, and put it into a
  // completed fetch list to be cleaned up.
  void FetchComplete(SerfFetch* fetch);
  apr_pool_t* pool() const { return pool_; }
  serf_context_t* serf_context() const { return serf_context_; }

  void PrintOutstandingFetches(MessageHandler* handler) const;
  virtual int64 timeout_ms() { return timeout_ms_; }

 protected:
  bool SetupProxy(const char* proxy);
  size_t NumActiveFetches();
  void CancelOutstandingFetches();
  bool WaitForInProgressFetchesHelper(int64 max_ms,
                                      MessageHandler* message_handler);

  apr_pool_t* pool_;
  Timer* timer_;

  // protects serf_context_ and active_fetches_
  AprMutex* mutex_;
  serf_context_t* serf_context_;
  typedef std::set<SerfFetch*> FetchSet;
  FetchSet active_fetches_;

  typedef std::vector<SerfFetch*> FetchVector;
  FetchVector completed_fetches_;
  SerfThreadedFetcher* threaded_fetcher_;

  // This is protected because it's updated along with active_fetches_,
  // which happens in subclass SerfThreadedFetcher as well as this class.
  Variable* outstanding_count_;

 private:
  Variable* request_count_;
  Variable* byte_count_;
  Variable* time_duration_ms_;
  Variable* cancel_count_;
  const int64 timeout_ms_;

  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_SERF_URL_ASYNC_FETCHER_H_
