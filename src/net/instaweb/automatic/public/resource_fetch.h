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

namespace net_instaweb {

class MessageHandler;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;
class SyncFetcherAdapterCallback;
class Timer;

// Manages a single fetch of a pagespeed rewritten resource.
// Fetch is initialized by calling ResourceFetch::Start()
//
// TODO(sligocki): Rename to PagespeedResourceFetch or something else ...
class ResourceFetch : public SharedAsyncFetch {
 public:
  // Start an async fetch for pagespeed resource. Response will be streamed
  // to async_fetch.
  static void Start(const GoogleUrl& url,
                    RewriteOptions* custom_options,
                    // This is intentionally not set in RewriteOptions because
                    // it is not so much an option as request-specific info
                    // similar to User-Agent (also not an option).
                    bool using_spdy,
                    ResourceManager* resource_manager,
                    AsyncFetch* async_fetch);

  // Fetch a pagespeed resource in a blocking fashion. Response will be
  // streamed back to async_fetch, but this function will not return until
  // fetch has completed.
  //
  // Returns true iff the fetch succeeded and thus response headers and
  // contents were sent to async_fetch.
  static bool BlockingFetch(const GoogleUrl& url,
                            RewriteOptions* custom_options,
                            bool using_spdy,
                            ResourceManager* resource_manager,
                            SyncFetcherAdapterCallback* async_fetch);

 protected:
  // Protected interface from AsyncFetch.
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

 private:
  ResourceFetch(const GoogleUrl& url, RewriteDriver* driver, Timer* timer,
                MessageHandler* handler, AsyncFetch* async_fetch);
  virtual ~ResourceFetch();

  // Same as Start(), but returns the RewriteDriver created. Used by
  // BlockingFetch() to wait for completion.
  static RewriteDriver* StartAndGetDriver(const GoogleUrl& url,
                                          RewriteOptions* custom_options,
                                          bool using_spdy,
                                          ResourceManager* resource_manager,
                                          AsyncFetch* async_fetch);

  GoogleUrl resource_url_;
  RewriteDriver* driver_;
  Timer* timer_;
  MessageHandler* message_handler_;

  int64 start_time_us_;
  int redirect_count_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_RESOURCE_FETCH_H_
