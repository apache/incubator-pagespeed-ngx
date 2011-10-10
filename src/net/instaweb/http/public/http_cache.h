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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_
#define NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Statistics;
class Timer;
class Variable;

// Implements HTTP caching semantics, including cache expiration and
// retention of the originally served cache headers.
class HTTPCache {
 public:
  // Names of statistics variables: exported for tests.
  static const char kCacheTimeUs[];
  static const char kCacheHits[];
  static const char kCacheMisses[];
  static const char kCacheExpirations[];
  static const char kCacheInserts[];

  // Does not take ownership of any inputs.
  HTTPCache(CacheInterface* cache, Timer* timer, Statistics* stats);
  virtual ~HTTPCache();

  // When a lookup is done in the HTTP Cache, it returns one of these values.
  enum FindResult {
    kFound,
    kNotFound,
    // Helps avoid frequent refetching of resources which have error status
    // codes or are not cacheable.
    kRecentFetchFailedOrNotCacheable,
  };

  class Callback {
   public:
    virtual ~Callback();
    virtual void Done(FindResult find_result) = 0;
    // A method that allows client Callbacks to apply invalidation checks.  We
    // first (in http_cache.cc) check whether the entry is expired using normal
    // http semantics, and if it is not expired, then this check is called --
    // thus callbacks can apply any further invalidation semantics it wants on
    // otherwise valid entries. But there's no way for a callback to override
    // when the HTTP semantics say the entry is expired.
    virtual bool IsCacheValid(const ResponseHeaders& headers) {
      return true;
    }

    HTTPValue* http_value() { return &http_value_; }
    ResponseHeaders* response_headers() { return &response_headers_; }

   private:
    HTTPValue http_value_;
    ResponseHeaders response_headers_;
  };

  // Makes the cache ignore all put requests.
  virtual void SetReadOnly();

  // Non-blocking Find.  Calls callback when done.  'handler' must all
  // stay valid until callback->Done() is called.
  virtual void Find(const GoogleString& key, MessageHandler* handler,
                    Callback* callback);

  // Blocking Find.  This method is deprecated for transition to strictly
  // non-blocking cache usage.
  //
  // TODO(jmarantz): remove this when blocking callers of HTTPCache::Find
  // are removed from the codebase.
  virtual HTTPCache::FindResult Find(const GoogleString& key, HTTPValue* value,
                                     ResponseHeaders* headers,
                                     MessageHandler* handler);

  // Note that Put takes a non-const pointer for HTTPValue so it can
  // bump the reference count.
  virtual void Put(const GoogleString& key, HTTPValue* value,
                   MessageHandler* handler);

  // Note that Put takes a non-const pointer for ResponseHeaders* so it
  // can update the caching fields prior to storing.
  virtual void Put(const GoogleString& key, ResponseHeaders* headers,
                   const StringPiece& content, MessageHandler* handler);

  // Deprecated method to make a blocking query for the state of an
  // element in the cache.
  // TODO(jmarantz): remove this interface when blocking callers are removed.
  virtual CacheInterface::KeyState Query(const GoogleString& key);

  // Deletes an element in the cache.
  virtual void Delete(const GoogleString& key);

  virtual void set_force_caching(bool force) { force_caching_ = force; }
  bool force_caching() const { return force_caching_; }
  Timer* timer() const { return timer_; }

  // Tell the HTTP Cache to remember that a particular key is not cacheable.
  // This may be due to the associated URL failing Fetch, or it may be because
  // the URL was fetched but was marked with Cache-Control 'nocache' or
  // Cache-Control 'private'.  In any case we would like to avoid DOSing
  // the origin server or spinning our own wheels trying to re-fetch this
  // resource.
  //
  // The not-cacheable setting will be 'remembered' for 5 minutes -- currently
  // hard-coded in http_cache.cc.
  //
  // TODO(jmarantz): if fetch failed, maybe we should try back soon,
  // but if it is Cache-Control: private, we can probably assume that
  // it still will be in 5 minutes.
  //
  // TODO(sligocki): I think we want to distinguish fetch failure from
  // non-cacheability in general. For example, when caching proxied
  // resources (say an un-rewritten CSS file), we may want to remember
  // that the original resource 404'd and not ping the server again.
  // However, if the resource was not cacheable, we should definitely
  // refetch it and proxy it.
  virtual void RememberFetchFailedOrNotCacheable(const GoogleString& key,
                                                 MessageHandler * handler);

  // Initialize statistics variables for the cache
  static void Initialize(Statistics* statistics);

  // Returns true if the resource is already at the point of expiration,
  // and would never be used if inserted into the cache. Otherwise, returns
  // false and increments the cache expirations statistic
  bool IsAlreadyExpired(const ResponseHeaders& headers);

  Variable* cache_time_us()     { return cache_time_us_; }
  Variable* cache_hits()        { return cache_hits_; }
  Variable* cache_misses()      { return cache_misses_; }
  Variable* cache_expirations() { return cache_expirations_; }
  Variable* cache_inserts()     { return cache_inserts_; }

 private:
  friend class HTTPCacheCallback;

  bool IsCurrentlyValid(const ResponseHeaders& headers, int64 now_ms);
  void PutHelper(const GoogleString& key, int64 now_us,
                 HTTPValue* value, MessageHandler* handler);
  void UpdateStats(FindResult result, int64 delta_us);

  CacheInterface* cache_;  // Owned by the caller.
  Timer* timer_;
  bool force_caching_;
  Variable* cache_time_us_;
  Variable* cache_hits_;
  Variable* cache_misses_;
  Variable* cache_expirations_;
  Variable* cache_inserts_;
  AtomicBool readonly_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_
