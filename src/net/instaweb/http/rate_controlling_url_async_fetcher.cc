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

#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"

#include <cstddef>
#include <queue>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

namespace {

// Keeps track of the objects required while deferring a fetch.
struct DeferredFetch {
  DeferredFetch(const GoogleString& in_url,
                AsyncFetch* in_fetch,
                MessageHandler* in_handler)
      : url(in_url),
        fetch(in_fetch),
        handler(in_handler) {}

  GoogleString url;
  AsyncFetch* fetch;
  MessageHandler* handler;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeferredFetch);
};

}  // namespace

const char RateControllingUrlAsyncFetcher::kQueuedFetchCount[] =
    "queued-fetch-count";
const char RateControllingUrlAsyncFetcher::kDroppedFetchCount[] =
    "dropped-fetch-count";
const char RateControllingUrlAsyncFetcher::kCurrentGlobalFetchQueueSize[] =
    "current-fetch-queue-size";

// Keeps track of all the pending and enqueued fetches for a given host.
class RateControllingUrlAsyncFetcher::HostFetchInfo
  : public RefCounted<RateControllingUrlAsyncFetcher::HostFetchInfo> {
 public:
  // Takes ownership of the mutex passed in.
  HostFetchInfo(const GoogleString& host,
                int per_host_outgoing_request_threshold,
                int per_host_queued_request_threshold,
                AbstractMutex* mutex)
      : host_(host),
        num_outbound_fetches_(0),
        per_host_outgoing_request_threshold_(
            per_host_outgoing_request_threshold),
        per_host_queued_request_threshold_(per_host_queued_request_threshold),
        mutex_(mutex) {}

  ~HostFetchInfo() {}

  // Returns the number of outbound fetches for the given host.
  int num_outbound_fetches() {
    ScopedMutex lock(mutex_.get());
    return num_outbound_fetches_;
  }

  // Checks if the number of outbound fetches is less than the threshold. If so,
  // increments the number of outbound fetches and returns true. Returns false
  // otherwise.
  bool IncrementIfCanTriggerFetch() {
    ScopedMutex lock(mutex_.get());
    if (num_outbound_fetches_ < per_host_outgoing_request_threshold_) {
      ++num_outbound_fetches_;
      return true;
    }
    return false;
  }

  // Decreases the number of outbound fetches by 1.
  void decrement_num_outbound_fetches() {
    ScopedMutex lock(mutex_.get());
    DCHECK_GT(num_outbound_fetches_, 0);
    --num_outbound_fetches_;
  }

  // Increases the number of outbound fetches by 1.
  void increment_num_outbound_fetches() {
    ScopedMutex lock(mutex_.get());
    DCHECK_GE(num_outbound_fetches_, 0);
    ++num_outbound_fetches_;
  }
  // Pushes the fetch to the back of the queue.
  bool EnqueueFetchIfWithinThreshold(const GoogleString& url,
                                     MessageHandler* handler,
                                     AsyncFetch* fetch) {
    ScopedMutex lock(mutex_.get());
    if (fetch_queue_.size() <
        static_cast<size_t>(per_host_queued_request_threshold_)) {
      fetch_queue_.push(new DeferredFetch(url, fetch, handler));
      return true;
    }
    return false;
  }

  // Gets the next fetch from the queue. Returns NULL if the queue is empty.
  DeferredFetch* PopNextFetchAndIncrementCountIfWithinThreshold() {
    ScopedMutex lock(mutex_.get());
    if (fetch_queue_.empty() ||
        num_outbound_fetches_ >= per_host_outgoing_request_threshold_) {
      return NULL;
    }
    DeferredFetch* fetch = fetch_queue_.front();
    fetch_queue_.pop();
    ++num_outbound_fetches_;
    return fetch;
  }

  // Returns the host associated with this HostFetchInfo object.
  const GoogleString& host() { return host_; }

 private:
  GoogleString host_;
  int num_outbound_fetches_;
  const int per_host_outgoing_request_threshold_;
  const int per_host_queued_request_threshold_;
  scoped_ptr<AbstractMutex> mutex_;
  std::queue<DeferredFetch*> fetch_queue_;

  DISALLOW_COPY_AND_ASSIGN(HostFetchInfo);
};

// Wrapper fetch that updates the count of outgoing fetches for the host when
// completed. It also triggers a fetch for any other pending requests for the
// domain.
class RateControllingUrlAsyncFetcher::CustomFetch : public SharedAsyncFetch {
 public:
  CustomFetch(const HostFetchInfoPtr& fetch_info,
              AsyncFetch* fetch,
              RateControllingUrlAsyncFetcher* fetcher)
      : SharedAsyncFetch(fetch),
        fetch_info_(fetch_info),
        fetcher_(fetcher) {}

  virtual void HandleDone(bool success) {
    base_fetch()->Done(success);
    fetch_info_->decrement_num_outbound_fetches();
    // Check if there is any fetch queued up for this host and the number of
    // outstanding fetches for the host is less than the threshold.
    DeferredFetch* deferred_fetch =
        fetch_info_->PopNextFetchAndIncrementCountIfWithinThreshold();
    if (deferred_fetch != NULL) {
      DCHECK_GT(fetcher_->current_global_fetch_queue_size_->Get(), 0);
      fetcher_->current_global_fetch_queue_size_->Add(-1);
      // Trigger a fetch for the queued up request.
      CustomFetch* wrapper_fetch = new CustomFetch(
          fetch_info_, deferred_fetch->fetch, fetcher_);
      fetcher_->base_fetcher_->Fetch(deferred_fetch->url,
                                     deferred_fetch->handler,
                                     wrapper_fetch);
      delete deferred_fetch;
    } else {
      fetcher_->DeleteFetchInfoIfPossible(fetch_info_);
    }
    delete this;
  }

 private:
  HostFetchInfoPtr fetch_info_;
  RateControllingUrlAsyncFetcher* fetcher_;
  DISALLOW_COPY_AND_ASSIGN(CustomFetch);
};

RateControllingUrlAsyncFetcher::RateControllingUrlAsyncFetcher(
    UrlAsyncFetcher* fetcher,
    int max_global_queue_size,
    int per_host_outgoing_request_threshold,
    int per_host_queued_request_threshold,
    ThreadSystem* thread_system,
    Statistics* statistics)
    :  base_fetcher_(fetcher),
       max_global_queue_size_(max_global_queue_size),
       per_host_outgoing_request_threshold_(
           per_host_outgoing_request_threshold),
       per_host_queued_request_threshold_(per_host_queued_request_threshold),
       thread_system_(thread_system),
       mutex_(thread_system->NewMutex()) {
  CHECK_GT(max_global_queue_size, 0);
  CHECK_GT(per_host_outgoing_request_threshold, 0);
  CHECK_GT(per_host_queued_request_threshold, 0);
  CHECK_GE(max_global_queue_size, per_host_queued_request_threshold);
  queued_fetch_count_ = statistics->GetTimedVariable(kQueuedFetchCount);
  dropped_fetch_count_ = statistics->GetTimedVariable(kDroppedFetchCount);
  current_global_fetch_queue_size_ = statistics->GetVariable(
      kCurrentGlobalFetchQueueSize);
}

RateControllingUrlAsyncFetcher::~RateControllingUrlAsyncFetcher() {
}

bool RateControllingUrlAsyncFetcher::Fetch(const GoogleString& url,
                                           MessageHandler* message_handler,
                                           AsyncFetch* fetch) {
  GoogleUrl gurl(url);
  GoogleString host;
  if (gurl.is_valid()) {
    host = gurl.Host().as_string();
    LowerString(&host);
  } else {
    // TODO(nikhilmadan): We should ideally just be dropping this fetch, but for
    // now we just hand it off to the base fetcher.
    return base_fetcher_->Fetch(url, message_handler, fetch);
  }

  HostFetchInfoPtr fetch_info_ptr;
  // Lookup the map for the fetch info associated with the given host. Note that
  // it would have been nice to avoid acquiring the mutex for user-facing
  // requests, but we need to lookup the fetch info in order to update the
  // number of outgoing requests.
  {
    ScopedMutex lock(mutex_.get());
    HostFetchInfoMap::iterator iter = fetch_info_map_.find(host);
    if (iter != fetch_info_map_.end()) {
      fetch_info_ptr = *iter->second;
    } else {
      // Insert a new entry if there wasn't one already.
      HostFetchInfoPtr* new_fetch_info_ptr = new HostFetchInfoPtr(
          new HostFetchInfo(host, per_host_outgoing_request_threshold_,
                            per_host_queued_request_threshold_,
                            thread_system_->NewMutex()));
      fetch_info_ptr = *new_fetch_info_ptr;
      fetch_info_map_[host] = new_fetch_info_ptr;
    }
  }

  if (!fetch->IsBackgroundFetch() ||
      fetch_info_ptr->IncrementIfCanTriggerFetch()) {
    // If this is a user-facing fetch or the number of outgoing fetches is
    // within the per-host threshold, trigger the fetch immediately.
    if (!fetch->IsBackgroundFetch()) {
      // Increment the count if the request is not a background fetch.
      fetch_info_ptr->increment_num_outbound_fetches();
    }
    CustomFetch* wrapper_fetch = new CustomFetch(fetch_info_ptr, fetch, this);
    return base_fetcher_->Fetch(url, message_handler, wrapper_fetch);
  } else if (current_global_fetch_queue_size_->Get() < max_global_queue_size_ &&
             fetch_info_ptr->EnqueueFetchIfWithinThreshold(
                 url, message_handler, fetch)) {
    // If the number of globally queued up fetches is within the threshold and
    // the number of queued requests for this host is less than the threshold,
    // push it to the back of the per-host queue.
    current_global_fetch_queue_size_->Add(1);
    queued_fetch_count_->IncBy(1);
    return false;
  }

  dropped_fetch_count_->IncBy(1);
  message_handler->Message(kInfo, "Dropping request for %s", url.c_str());
  fetch->Done(false);
  return true;
}

void RateControllingUrlAsyncFetcher::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCurrentGlobalFetchQueueSize);
  statistics->AddTimedVariable(kQueuedFetchCount,
                               UrlAsyncFetcher::kStatisticsGroup);
  statistics->AddTimedVariable(kDroppedFetchCount,
                               UrlAsyncFetcher::kStatisticsGroup);
}

void RateControllingUrlAsyncFetcher::DeleteFetchInfoIfPossible(
    const HostFetchInfoPtr& fetch_info) {
  ScopedMutex lock(mutex_.get());
  DCHECK_GE(fetch_info->num_outbound_fetches(), 0);
  if (fetch_info->num_outbound_fetches() > 0) {
    return;
  }

  HostFetchInfoMap::iterator iter = fetch_info_map_.find(fetch_info->host());
  if (iter != fetch_info_map_.end()) {
    delete iter->second;
    fetch_info_map_.erase(iter);
  }
}

}  // namespace net_instaweb
