/*
 * Copyright 2011 Google Inc.
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
//
// NOTE: This interface is actively under development and may be
// changed extensively. Contact us at mod-pagespeed-discuss@googlegroups.com
// if you are interested in using it.

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_RESOURCE_FETCH_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_RESOURCE_FETCH_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;
class Timer;

// Manages a single fetch of a pagespeed rewritten resource.
// Fetch is initialized by calling ResourceFetch::Start()
//
// TODO(sligocki): Rename to ProxyResourceFetch or something else ...
class ResourceFetch : public SharedAsyncFetch {
 public:
  static void Start(ResourceManager* resource_manager,
                    const GoogleUrl& url,
                    AsyncFetch* async_fetch,
                    RewriteOptions* custom_options,
                    const GoogleString& version);

 protected:
  // Protected interface from AsyncFetch.
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

 private:
  explicit ResourceFetch(const GoogleUrl& url,
                         AsyncFetch* async_fetch,
                         MessageHandler* handler,
                         RewriteDriver* driver,
                         Timer* timer,
                         const GoogleString& version);
  virtual ~ResourceFetch();

  GoogleUrl resource_url_;
  MessageHandler* message_handler_;
  RewriteDriver* driver_;
  Timer* timer_;
  const GoogleString& version_;

  int64 start_time_us_;
  int redirect_count_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_RESOURCE_FETCH_H_
