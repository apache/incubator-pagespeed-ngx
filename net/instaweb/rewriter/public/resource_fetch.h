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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_FETCH_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_FETCH_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class MessageHandler;
class ServerContext;
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
  // For StartWithDriver().
  enum CleanupMode {
    kAutoCleanupDriver,
    kDontAutoCleanupDriver
  };

  // Start an async fetch for pagespeed resource. Response will be streamed
  // to async_fetch.
  //
  // If custom_options it not NULL, takes ownership of it and and can mutate it.
  static void Start(const GoogleUrl& url,
                    RewriteOptions* custom_options,
                    ServerContext* server_context,
                    AsyncFetch* async_fetch);

  // Same as Start(), but takes the RewriteDriver to use.
  // cleanup_mode determines whether ResourceFetch will call Cleanup()
  // on the driver itself. If it's set to kAutoCleanupDriver, the driver should
  // not be used by the caller after this call. Otherwise, it may be used by
  // the caller, but it's responsible for calling Cleanup() once done with it.
  static void StartWithDriver(const GoogleUrl& url,
                              CleanupMode cleanup_mode,
                              ServerContext* server_context,
                              RewriteDriver* driver,
                              AsyncFetch* async_fetch);

  // Fetch a pagespeed resource in a blocking fashion. Response will be
  // streamed back to async_fetch, but this function will not return until
  // fetch has completed.
  //
  // You'll probably want to use GetDriver to construct the driver passed in
  // to this method, in order to properly apply experiment info encoded into
  // the URL into settings.
  //
  // Returns true iff the fetch succeeded and thus response headers and
  // contents were sent to async_fetch.
  static bool BlockingFetch(const GoogleUrl& url,
                            ServerContext* server_context,
                            RewriteDriver* driver,
                            SyncFetcherAdapterCallback* async_fetch);

  // Creates a rewrite_driver suitable for passing to BlockingFetch
  // (or StartWithDriver) incorporating any experiment settings.
  // If custom_options it not NULL, takes ownership of it and and can mutate it;
  // otherwise the driver will be made using driver_pool with matching options
  // as the pre-experiment starting point.
  //
  // Exactly one of custom_options and driver_pool must be non-NULL.
  static RewriteDriver* GetDriver(const GoogleUrl& url,
                                  RewriteOptions* custom_options,
                                  ServerContext* server_context,
                                  const RequestContextPtr& request_ctx);

 protected:
  // Protected interface from AsyncFetch.
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

 private:
  ResourceFetch(const GoogleUrl& url, CleanupMode cleanup_mode,
                RewriteDriver* driver, Timer* timer,
                MessageHandler* handler, AsyncFetch* async_fetch);
  virtual ~ResourceFetch();

  // If we're running an experiment and the url specifies an experiment spec,
  // set custom_options to use that experiment spec.  If custom_options is NULL
  // one will be allocated and the caller takes ownership of it.
  static void ApplyExperimentOptions(const GoogleUrl& url,
                                     const RequestContextPtr& request_ctx,
                                     ServerContext* server_context,
                                     RewriteOptions** custom_options);

  GoogleUrl resource_url_;
  RewriteDriver* driver_;
  Timer* timer_;
  MessageHandler* message_handler_;

  int64 start_time_ms_;
  int redirect_count_;
  CleanupMode cleanup_mode_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_FETCH_H_
