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

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/cache_interface.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CacheInterface;
class HTTPValue;
class MessageHandler;
class ResponseHeaders;
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

  // Takes over ownership of the cache.
  HTTPCache(CacheInterface* cache, Timer* timer)
      : cache_(cache),
        timer_(timer),
        force_caching_(false),
        cache_time_us_(NULL),
        cache_hits_(NULL),
        cache_misses_(NULL),
        cache_expirations_(NULL),
        cache_inserts_(NULL) {
  }

  ~HTTPCache();

  // When a lookup is done in the HTTP Cache, it returns one of these
  // values.  2 of these are obvious, one is used to help avoid
  // frequently re-fetching the same content that failed to fetch, or
  // was fetched but was not cacheable.
  //
  // TODO(jmarantz): consider merging these 3 into the 3 status codes defined
  // CacheInterface, making 4 distinct codes.  That would be a little clearer,
  // but would require that all callers of Find handle kInTransit which no
  // cache implementations currently generate.
  enum FindResult {
    kFound,
    kRecentFetchFailedDoNotRefetch,
    kNotFound
  };

  FindResult Find(const std::string& key, HTTPValue* value,
                  ResponseHeaders* headers,
                  MessageHandler* handler);

  // Note that Put takes a non-const pointer for HTTPValue so it can
  // bump the reference count.
  void Put(const std::string& key, HTTPValue* value, MessageHandler* handler);

  // Note that Put takes a non-const pointer for ResponseHeaders* so it
  // can update the caching fields prior to storing.
  void Put(const std::string& key, ResponseHeaders* headers,
           const StringPiece& content, MessageHandler* handler);

  CacheInterface::KeyState Query(const std::string& key);
  void Delete(const std::string& key);

  void set_force_caching(bool force) { force_caching_ = force; }
  bool force_caching() const { return force_caching_; }
  Timer* timer() const { return timer_; }

  // Initializes statistics for the cache (time, hits, misses, expirations)
  void SetStatistics(Statistics* stats);

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
  void RememberNotCacheable(const std::string& key, MessageHandler * handler);

  // Initialize statistics variables for the cache
  static void Initialize(Statistics* statistics);

  // Returns true if the resource is already at the point of expiration,
  // and would never be used if inserted into the cache. Otherwise, returns
  // false and increments the cache expirations statistic
  bool IsAlreadyExpired(const ResponseHeaders& headers);

 private:
  bool IsCurrentlyValid(const ResponseHeaders& headers, int64 now_ms);
  void PutHelper(const std::string& key, int64 now_us,
                 HTTPValue* value, MessageHandler* handler);

  scoped_ptr<CacheInterface> cache_;
  Timer* timer_;
  bool force_caching_;
  Variable* cache_time_us_;
  Variable* cache_hits_;
  Variable* cache_misses_;
  Variable* cache_expirations_;
  Variable* cache_inserts_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_H_
