/*
 * Copyright 2012 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_

#include <map>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AbstractMutex;
class AsyncFetch;
class MessageHandler;
class Statistics;
class ThreadSystem;
class TimedVariable;
class Variable;

// Fetcher which limits the number of outgoing fetches per domain. If the
// fetch is for a user-facing request, this sends the request out anyway and
// updates the count for number of outgoing fetches.
// For non-user facing requests, this checks that the number of outgoing fetches
// for this domain is less than the limit. If less than the limit, it sends
// the fetch out and updates the count. If greater than the per-domain limit,
// and if the global queue size is within the limit, it queues the request up.
// However, if the global queue size is above the limit, it drops the request.
// If a request is dropped, the response will have HttpAttributes::kXPsaLoadShed
// set on the response headers.
class RateControllingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  static const char kQueuedFetchCount[];
  static const char kDroppedFetchCount[];
  static const char kCurrentGlobalFetchQueueSize[];

  RateControllingUrlAsyncFetcher(UrlAsyncFetcher* fetcher,
                                 int max_global_queue_size,
                                 int per_host_outgoing_request_threshold,
                                 int per_host_queued_request_threshold,
                                 ThreadSystem* thread_system,
                                 Statistics* statistics);

  virtual ~RateControllingUrlAsyncFetcher();

  virtual bool SupportsHttps() const {
    return base_fetcher_->SupportsHttps();
  }

  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Initializes statistics variables associated with this class.
  static void InitStats(Statistics* statistics);

 private:
  class HostFetchInfo;
  class CustomFetch;
  friend class CustomFetch;

  typedef RefCountedPtr<HostFetchInfo> HostFetchInfoPtr;

  typedef std::map<GoogleString, HostFetchInfoPtr*> HostFetchInfoMap;

  // Delete the fetch info from fetch_info_map_ if possible.
  void DeleteFetchInfoIfPossible(const HostFetchInfoPtr& fetch_info);

  // The base fetcher used while fetching.
  UrlAsyncFetcher* base_fetcher_;
  // The maximum permissible size of the global queue.
  const int max_global_queue_size_;
  // The maximum number of outgoing requests allowed per host.
  const int per_host_outgoing_request_threshold_;
  // The maximum number of queued requests allowed per host.
  const int per_host_queued_request_threshold_;
  ThreadSystem* thread_system_;

  // Map containing per-host information tracking outgoing and queued fetches.
  HostFetchInfoMap fetch_info_map_;
  scoped_ptr<AbstractMutex> mutex_;

  TimedVariable* queued_fetch_count_;
  TimedVariable* dropped_fetch_count_;
  // Using a variable here, since we want to be able to track this in the server
  // statistics.
  Variable* current_global_fetch_queue_size_;

  DISALLOW_COPY_AND_ASSIGN(RateControllingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_
