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
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct ContentType;
class HTTPCache;
class HTTPValue;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class RewriteDriver;
class RewriteOptions;
class Statistics;
class Timer;
class Variable;

class CacheableResourceBase : public Resource {
 public:
  // All the public methods here implement the Resource API.

  // All subclasses of this use the HTTP cache.
  virtual bool UseHttpCache() const { return true; }

  virtual bool IsValidAndCacheable() const;

  // This checks the cache, and fetches the resource if appropriate.
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback);

  virtual void Freshen(FreshenCallback* callback, MessageHandler* handler);
  virtual void RefreshIfImminentlyExpiring();
  virtual GoogleString url() const { return url_; }
  virtual GoogleString cache_key() const { return cache_key_; }

 protected:
  // Note: InitStats(stat_prefix) must have been called before.
  CacheableResourceBase(StringPiece stat_prefix,
                        StringPiece url,
                        StringPiece cache_key,
                        const ContentType* type,
                        RewriteDriver* rewrite_driver);
  virtual ~CacheableResourceBase();

  static void InitStats(StringPiece stat_prefix, Statistics* statistics);

  // Permits the subclass to alter request headers or request context used for
  // a fetch. Default implementation does nothing.
  virtual void PrepareRequest(const RequestContextPtr& request_context,
                              RequestHeaders* headers);

  // Permits the subclass to alter the response headers returned from a
  // fetch before the entry gets added to the cache.
  // Default implementation does nothing.
  // Note: ComputeCaching hasn't been called yet at time this is invoked.
  virtual void PrepareResponseHeaders(ResponseHeaders* headers);

  HTTPCache* http_cache() const { return server_context()->http_cache(); }
  RewriteDriver* rewrite_driver() const { return rewrite_driver_; }
  const RewriteOptions* rewrite_options() const;

  // Returns a bool indicating whether we should skip triggering a background
  // fetch.
  bool ShouldSkipBackgroundFetch() const;

 private:
  class FreshenHttpCacheCallback;
  class LoadHttpCacheCallback;
  class FetchCallbackBase;
  class FreshenFetchCallback;
  class LoadFetchCallback;
  friend class CacheableResourceBaseTest;

  // Extends callback->input_info() validity timeframe if the new state of the
  // resource, as represented by headers and value, is consistent with what's
  // recorded in input_info. Returns true if this extension was successful.
  bool UpdateInputInfoForFreshen(const ResponseHeaders& headers,
                                 const HTTPValue& value,
                                 Resource::FreshenCallback* callback);

  // Implementation for IsValidAndCacheable, and also lets us check the headers
  // before updating the resource
  bool IsValidAndCacheableImpl(const ResponseHeaders& headers) const;

  Timer* timer() const { return server_context()->timer(); }
  MessageHandler* message_handler() const {
    return server_context()->message_handler();
  }

  GoogleString url_;
  GoogleString cache_key_;

  RewriteDriver* rewrite_driver_;
  Variable* hits_;
  Variable* recent_fetch_failures_;
  Variable* recent_uncacheables_miss_;
  Variable* recent_uncacheables_failure_;
  Variable* misses_;

  DISALLOW_COPY_AND_ASSIGN(CacheableResourceBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHEABLE_RESOURCE_BASE_H_
