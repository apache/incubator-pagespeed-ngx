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
//         jmarantz@google.com (Joshua Marantz)
//
// Resources are created by a RewriteDriver.  Input resources are
// read from URLs or the file system.  Output resources are constructed
// programatically, usually by transforming one or more existing
// resources.  Both input and output resources inherit from this class
// so they can be used interchangeably in successive rewrite passes.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CACHEABLE_RESOURCE_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CACHEABLE_RESOURCE_BASE_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

struct ContentType;
class HTTPCache;
class MessageHandler;
class RewriteOptions;
class Timer;

class CacheableResourceBase : public Resource {
 public:
  // Links the stale fallback value that can be used in case a fetch fails.
  // All subclasses of this use the HTTP cache.
  virtual bool UseHttpCache() const { return true; }

 protected:
  class LoadHttpCacheCallback;

  CacheableResourceBase(ServerContext* server_context, const ContentType* type)
      : Resource(server_context, type) {}
  virtual ~CacheableResourceBase();

  virtual void RefreshIfImminentlyExpiring();

  // Implementation of loading. This checks the cache, and calls
  // LoadAndSaveToCache if appropriate.
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback);

  // Should be overridden by subclasses to actually fetch the resource and write
  // it to the cache. LoadAndCallback is implemented in terms of this
  virtual void LoadAndSaveToCache(NotCacheablePolicy not_cacheable_policy,
                                  AsyncCallback* callback,
                                  MessageHandler* message_handler) = 0;

  // Obtain rewrite options for this. Used in cache invalidation.
  virtual const RewriteOptions* rewrite_options() const = 0;

 private:
  HTTPCache* http_cache() const { return server_context()->http_cache(); }
  Timer* timer() const { return server_context()->timer(); }
  MessageHandler* message_handler() const {
    return server_context()->message_handler();
  }

  DISALLOW_COPY_AND_ASSIGN(CacheableResourceBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHEABLE_RESOURCE_BASE_H_
