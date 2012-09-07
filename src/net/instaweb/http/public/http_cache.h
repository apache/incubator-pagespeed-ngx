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
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CacheInterface;
class Hasher;
class MessageHandler;
class RequestHeaders;
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
  static const char kCacheDeletes[];

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
    kRecentFetchFailed,
    kRecentFetchNotCacheable,
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
          owns_response_headers_(false),
          logging_info_(NULL),
          owns_logging_info_(false) {
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
    virtual bool IsCacheValid(const GoogleString& key,
                              const ResponseHeaders& headers) = 0;

    // A method that allows client Callbacks to check if the response in cache
    // is fresh enough, in addition to it being valid.  This is used while
    // freshening  resources to check that the response in cache is not only
    // valid, but is also not going to expire anytime soon.
    // Note that if the response in cache is valid but not fresh, the HTTPCache
    // calls Callback::Done with find_result = kNotFound and fills in
    // fallback_http_value() with the cached response.
    virtual bool IsFresh(const ResponseHeaders& headers) { return true; }

    // Overrides the cache ttl of the cached response with the given value. Note
    // that this has no effect if the returned value is negative or less than
    // the cache ttl of the stored value.
    virtual int64 OverrideCacheTtlMs(const GoogleString& key) { return -1; }

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
    HTTPValue* fallback_http_value() { return &fallback_http_value_; }

    // Sets the LoggingInfo to the specified pointer.  The caller must
    // guarantee that the pointed-to LoggingInfo remains valid as long as the
    // HTTPCache is running.
    void set_logging_info(LoggingInfo* logging_info);
    virtual LoggingInfo* logging_info();
    virtual void SetTimingMs(int64 timing_value_ms);

   private:
    HTTPValue http_value_;
    // Stale value that can be used in case a fetch fails. Note that Find()
    // may fill in a stale value here but it will still return kNotFound.
    HTTPValue fallback_http_value_;
    ResponseHeaders* response_headers_;
    bool owns_response_headers_;
    LoggingInfo* logging_info_;
    bool owns_logging_info_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
  };

  // Makes the cache ignore put requests that do not record successes.
  virtual void SetIgnoreFailurePuts();

  // Non-blocking Find.  Calls callback when done.  'handler' must all
  // stay valid until callback->Done() is called.
  virtual void Find(const GoogleString& key, MessageHandler* handler,
                    Callback* callback);

  // Note that Put takes a non-const pointer for HTTPValue so it can
  // bump the reference count.
  virtual void Put(const GoogleString& key, HTTPValue* value,
                   MessageHandler* handler);

  // Note that Put takes a non-const pointer for ResponseHeaders* so it
  // can update the caching fields prior to storing.
  // If you call this method, you must be certain that the outgoing
  // request was not sent with Authorization:.
  virtual void Put(const GoogleString& key, ResponseHeaders* headers,
                   const StringPiece& content, MessageHandler* handler);

  // Deletes an element in the cache.
  virtual void Delete(const GoogleString& key);

  virtual void set_force_caching(bool force) { force_caching_ = force; }
  bool force_caching() const { return force_caching_; }
  Timer* timer() const { return timer_; }

  // Tell the HTTP Cache to remember that a particular key is not cacheable
  // because the URL was marked with Cache-Control 'nocache' or Cache-Control
  // 'private'. We would like to avoid DOSing the origin server or spinning our
  // own wheels trying to re-fetch this resource.
  // The not-cacheable setting will be 'remembered' for
  // remember_not_cacheable_ttl_seconds_.
  // Note that we remember whether the response was originally a "200 OK" so
  // that we can check if the cache TTL can be overridden.
  virtual void RememberNotCacheable(const GoogleString& key,
                                    bool is_200_status_code,
                                    MessageHandler* handler);

  // Tell the HTTP Cache to remember that a particular key is not cacheable
  // because the associated URL failing Fetch.
  //
  // The not-cacheable setting will be 'remembered' for
  // remember_fetch_failed_ttl_seconds_.
  virtual void RememberFetchFailed(const GoogleString& key,
                                   MessageHandler* handler);

  // Tell the HTTP Cache to remember that we had to give up on doing a
  // background fetch due to load. This will remember it for
  // remember_fetch_load_shed_ttl_seconds_.
  virtual void RememberFetchDropped(const GoogleString& key,
                                    MessageHandler* handler);

  // Indicates if the response is within the cacheable size limit. Clients of
  // HTTPCache must check if they will be eventually able to cache their entries
  // before buffering them in memory. If the content length header is not found
  // then consider it as cacheable. This could be a chunked response.
  bool IsCacheableContentLength(ResponseHeaders* headers) const;
  // Indicates if the response body is within the cacheable size limit. If the
  // response headers do not have content length header, then the clients of
  // HTTPCache must check if the received response body is of cacheable size
  // before buffering them in memory.
  bool IsCacheableBodySize(int64 body_size) const;

  // Initialize statistics variables for the cache
  static void InitStats(Statistics* statistics);

  // Returns true if the resource is already at the point of expiration
  // (or not cacheable by us), and would never be used if inserted into the
  // cache. Otherwise, returns false. If the entry was rejected because of
  // expiration but would otherwise have been cacheable, this also increments
  // the cache expirations statistic.
  //
  // request_headers is used to check for resources requested with
  // authorization. It is OK to pass NULL if you're certain that the fetch
  // was done without authorization headers.
  bool IsAlreadyExpired(const RequestHeaders* request_headers,
                        const ResponseHeaders& headers);

  Variable* cache_time_us()     { return cache_time_us_; }
  Variable* cache_hits()        { return cache_hits_; }
  Variable* cache_misses()      { return cache_misses_; }
  Variable* cache_expirations() { return cache_expirations_; }
  Variable* cache_inserts()     { return cache_inserts_; }
  Variable* cache_deletes()     { return cache_deletes_; }

  int64 remember_not_cacheable_ttl_seconds() {
    return remember_not_cacheable_ttl_seconds_;
  }

  virtual void set_remember_not_cacheable_ttl_seconds(int64 value) {
    DCHECK_LE(0, value);
    if (value >= 0) {
      remember_not_cacheable_ttl_seconds_ = value;
    }
  }

  int64 remember_fetch_failed_ttl_seconds() {
    return remember_fetch_failed_ttl_seconds_;
  }

  virtual void set_remember_fetch_failed_ttl_seconds(int64 value) {
    DCHECK_LE(0, value);
    if (value >= 0) {
      remember_fetch_failed_ttl_seconds_ = value;
    }
  }

  int64 remember_fetch_dropped_ttl_seconds() {
    return remember_fetch_dropped_ttl_seconds_;
  }

  virtual void set_remember_fetch_dropped_ttl_seconds(int64 value) {
    DCHECK_LE(0, value);
    if (value >= 0) {
      remember_fetch_dropped_ttl_seconds_ = value;
    }
  }

  int max_cacheable_response_content_length() {
    return max_cacheable_response_content_length_;
  }

  virtual void set_max_cacheable_response_content_length(int64 value);

  virtual const char* Name() const { return name_.c_str(); }

 protected:
  virtual void PutInternal(const GoogleString& key, int64 start_us,
                           HTTPValue* value);

 private:
  friend class HTTPCacheCallback;
  friend class WriteThroughHTTPCache;

  bool IsCurrentlyValid(const RequestHeaders* request_headers,
                        const ResponseHeaders& headers, int64 now_ms);
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
  Variable* cache_deletes_;
  GoogleString name_;
  int64 remember_not_cacheable_ttl_seconds_;
  int64 remember_fetch_failed_ttl_seconds_;
  int64 remember_fetch_dropped_ttl_seconds_;
  int64 max_cacheable_response_content_length_;
  AtomicBool ignore_failure_puts_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_
