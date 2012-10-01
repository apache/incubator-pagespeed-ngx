/*
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/wait_url_async_fetcher.h"

#include <vector>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class WaitUrlAsyncFetcher::DelayedFetch {
 public:
  DelayedFetch(UrlFetcher* base_fetcher, const GoogleString& url,
               MessageHandler* handler, AsyncFetch* base_fetch)
      : base_fetcher_(base_fetcher), url_(url), handler_(handler),
        base_fetch_(base_fetch) {
  }

  void FetchNow() {
    bool status = base_fetcher_->StreamingFetchUrl(
        url_, *base_fetch_->request_headers(), base_fetch_->response_headers(),
        base_fetch_, handler_);
    base_fetch_->Done(status);
  }

 private:
  UrlFetcher* base_fetcher_;

  GoogleString url_;
  MessageHandler* handler_;
  AsyncFetch* base_fetch_;

  DISALLOW_COPY_AND_ASSIGN(DelayedFetch);
};

WaitUrlAsyncFetcher::~WaitUrlAsyncFetcher() {}

bool WaitUrlAsyncFetcher::Fetch(const GoogleString& url,
                                MessageHandler* handler,
                                AsyncFetch* base_fetch) {
  DelayedFetch* delayed_fetch =
      new DelayedFetch(url_fetcher_, url, handler, base_fetch);
  {
    ScopedMutex lock(mutex_.get());
    if (!pass_through_mode_) {
      // Don't call the blocking fetcher until CallCallbacks.
      delayed_fetches_.push_back(delayed_fetch);
      return false;
    }
  }
  // pass_through_mode_ == true
  delayed_fetch->FetchNow();
  delete delayed_fetch;
  return true;
}

bool WaitUrlAsyncFetcher::CallCallbacksAndSwitchModesHelper(bool new_mode) {
  bool prev_mode = false;
  std::vector<DelayedFetch*> fetches;
  {
    // Don't hold the mutex while we call our callbacks.  Transfer
    // the to a local vector and release the mutex as quickly as possible.
    ScopedMutex lock(mutex_.get());
    fetches.swap(delayed_fetches_);
    prev_mode = pass_through_mode_;
    pass_through_mode_ = new_mode;
  }
  for (int i = 0, n = fetches.size(); i < n; ++i) {
    fetches[i]->FetchNow();
  }
  STLDeleteElements(&fetches);
  return prev_mode;
}

void WaitUrlAsyncFetcher::CallCallbacks() {
  DCHECK(!pass_through_mode_);
  CallCallbacksAndSwitchModesHelper(false);
}

bool WaitUrlAsyncFetcher::SetPassThroughMode(bool new_mode) {
  bool old_mode = false;
  if (new_mode) {
    // This is structured so that we only need to grab the mutex once.
    old_mode = CallCallbacksAndSwitchModesHelper(true);
  } else {
    // We are turning pass-through-mode back off
    ScopedMutex lock(mutex_.get());
    old_mode = pass_through_mode_;
    pass_through_mode_ = false;
  }
  return old_mode;
}


}  // namespace net_instaweb
