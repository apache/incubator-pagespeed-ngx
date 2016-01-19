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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_

#include <set>
#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Fake UrlAsyncFetcher which waits to call underlying fetcher until
// you explicitly call CallCallbacks().
class WaitUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  WaitUrlAsyncFetcher(UrlAsyncFetcher* url_fetcher,
                      AbstractMutex* mutex)
      : url_fetcher_(url_fetcher),
        pass_through_mode_(false),
        mutex_(mutex) {
  }
  virtual ~WaitUrlAsyncFetcher();

  // Initiate fetches that will finish when CallCallbacks is called.
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* handler,
                     AsyncFetch* fetch);

  // Call all callbacks from previously initiated fetches.
  void CallCallbacks();

  // Sets a mode where no waiting occurs -- fetches propagate immediately.
  // The previous mode is returned.  When turning pass-through mode on,
  // any pending callbacks are called.
  bool SetPassThroughMode(bool pass_through_mode);

  // Makes the given URL be fetched immediately.
  void DoNotDelay(const GoogleString& url);

 private:
  class DelayedFetch;

  bool CallCallbacksAndSwitchModesHelper(bool new_mode);

  UrlAsyncFetcher* url_fetcher_;
  std::set<GoogleString> do_not_delay_urls_;
  std::vector<DelayedFetch*> delayed_fetches_;
  bool pass_through_mode_;
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(WaitUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_
