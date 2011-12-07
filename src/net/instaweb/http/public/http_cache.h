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

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Hasher;
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

  // The prefix to be added to Etags.
  static const char kEtagPrefix[];

  // Does not take ownership of any inputs.
  HTTPCache(CacheInterface* cache, Timer* timer, Hasher* hasher,
            Statistics* stats);
  virtual ~HTTPCache();

  // When a lookup is done in the HTTP Cache, it returns one of these values.
  enum FindResult {
    kFound,
    kNotFound,
    // Helps avoid frequent refetching of resources which have error status
    // codes or are not cacheable.
    kRecentFetchFailedOrNotCacheable,
  };

  // Class to handle an asynchronous cache lookup response.
  //
  // TODO(jmarantz): consider inheriting from AsyncFetch with an implementation
  // of Write/Flush/HeadersComplete -- we'd have to make Done take true/false so
  // this would impact callers.
  class Callback {
   public:
    Callback()
        : response_headers_(NULL),
          owns_response_headers_(false) {
    }
    virtual ~Callback();
    virtual void Done(FindResult find_result) = 0;
    // A method that allows client Callbacks to apply invalidation checks.  We
    // first (in http_cache.cc) check whether the entry is expired using normal
    // http semantics, and if it is not expired, then this check is called --
    // thus callbacks can apply any further invalidation semantics it wants on
    // otherwise valid entries. But there's no way for a callback to override
    // when the HTTP semantics say the entry is expired.
    //
    // See also OptionsAwareHTTPCacheCallback in rewrite_driver.h for an
    // implementation you probably want to use.
    virtual bool IsCacheValid(const ResponseHeaders& headers) = 0;

    // TODO(jmarantz): specify the dataflow between http_value and
    // response_headers.
    HTTPValue* http_value() { return &http_value_; }
    ResponseHeaders* response_headers() {
      if (response_headers_ == NULL) {
        response_headers_ = new ResponseHeaders;
        owns_response_headers_ = true;
      }
      return response_headers_;
    }
    const ResponseHeaders* response_headers() const {
      return const_cast<Callback*>(this)->response_headers();
    }
    void set_response_headers(ResponseHeaders* headers) {
      DCHECK(!owns_response_headers_);
      if (owns_response_headers_) {
        delete response_headers_;
      }
      response_headers_ = headers;
      owns_response_headers_ = false;
    }

   private:
    HTTPValue http_value_;
    ResponseHeaders* response_headers_;
    bool owns_response_headers_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
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

  // Tell the HTTP Cache to remember that a particular key is not cacheable
  // because the URL was marked with Cache-Control 'nocache' or Cache-Control
  // 'private'. We would like to avoid DOSing the origin server or spinning our
  // own wheels trying to re-fetch this resource.
  //
  // The not-cacheable setting will be 'remembered' for
  // remember_not_cacheable_ttl_seconds_.
  virtual void RememberNotCacheable(const GoogleString& key,
                                    MessageHandler * handler);

  // Tell the HTTP Cache to remember that a particular key is not cacheable
  // because the associated URL failing Fetch.
  //
  // The not-cacheable setting will be 'remembered' for
  // remember_fetch_failed_ttl_seconds_.
  virtual void RememberFetchFailed(const GoogleString& key,
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

  int64 remember_not_cacheable_ttl_seconds() {
    return remember_not_cacheable_ttl_seconds_;
  }

  virtual void set_remember_not_cacheable_ttl_seconds(int64 value) {
    DCHECK(value >= 0);
    if (value >= 0) {
      remember_not_cacheable_ttl_seconds_ = value;
    }
  }

  int64 remember_fetch_failed_ttl_seconds() {
    return remember_fetch_failed_ttl_seconds_;
  }

  virtual void set_remember_fetch_failed_ttl_seconds(int64 value) {
    DCHECK(value >= 0);
    if (value >= 0) {
      remember_fetch_failed_ttl_seconds_ = value;
    }
  }

 protected:
  virtual void PutInternal(const GoogleString& key, int64 start_us,
                           HTTPValue* value);

 private:
  friend class HTTPCacheCallback;
  friend class WriteThroughHTTPCache;

  bool IsCurrentlyValid(const ResponseHeaders& headers, int64 now_ms);
  // Requires either content or value to be non-NULL.
  // Applies changes to headers. If the headers are actually changed or if value
  // is NULL then it builds and returns a new HTTPValue. If content is NULL
  // then content is extracted from value.
  HTTPValue* ApplyHeaderChangesForPut(
      const GoogleString& key, int64 start_us, const StringPiece* content,
      ResponseHeaders* headers, HTTPValue* value, MessageHandler* handler);
  void UpdateStats(FindResult result, int64 delta_us);
  void RememberFetchFailedorNotCacheableHelper(
      const GoogleString& key, MessageHandler* handler, HttpStatus::Code code,
      int64 ttl_sec);

  CacheInterface* cache_;  // Owned by the caller.
  Timer* timer_;
  Hasher* hasher_;
  bool force_caching_;
  Variable* cache_time_us_;
  Variable* cache_hits_;
  Variable* cache_misses_;
  Variable* cache_expirations_;
  Variable* cache_inserts_;
  int64 remember_not_cacheable_ttl_seconds_;
  int64 remember_fetch_failed_ttl_seconds_;
  AtomicBool readonly_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_
