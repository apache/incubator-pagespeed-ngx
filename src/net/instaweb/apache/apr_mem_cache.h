/*
 * Copyright 2012 Google Inc.
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

#ifndef NET_INSTAWEB_APACHE_APR_MEM_CACHE_H_
#define NET_INSTAWEB_APACHE_APR_MEM_CACHE_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct apr_memcache_t;
struct apr_memcache_server_t;

namespace net_instaweb {

class AprMemCacheServers;
class Hasher;
class MessageHandler;
class SharedString;

// Interface to memcached via connectivity layer in
// AprMemCacheServers.  This is an entirely blocking interface, hence
// this is a blocking cache implementation.
//
// A fallback cache is used to store large values that won't fit in
// memcached.  Note that this is not a write-through cache; if a value
// is small enough to be written to memcache, it will not be written
// to the fallback.  Large values will be written to the fallback, and
// only a sentinel is written to the memcache.  When a lookup misses
// in memcache, we do not check it in the fallback.  We only do a lookup
// in the fallback if the memcache lookup gives us our sentinel.
class AprMemCache : public CacheInterface {
 public:
  // Experimentally it seems large values larger than 1M bytes result in
  // a failure, e.g. from load-tests:
  //     [Fri Jul 20 10:29:34 2012] [error] [mod_pagespeed 0.10.0.0-1699 @1522]
  //     AprMemCache::Put error: Internal error on key
  //     http://example.com/image.jpg, value-size 1393146
  // We use a fallback cache (in Apache a FileCache) to handle too-large
  // requests.  We store in memcached a sentinel indicating the value
  // should be found in the fallback cache.
  //
  // We also bound the key-size to 65534 bytes.  Values with larger keys are
  // passed to the fallback cache.
  static const size_t kValueSizeThreshold = 1 * 1000 * 1000;

  AprMemCache(AprMemCacheServers* servers,
              CacheInterface* fallback_cache,
              MessageHandler* handler);
  virtual ~AprMemCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);

  virtual const char* Name() const { return "AprMemCache"; }

  // Get detailed status in a string, returning false if the server
  // failed to return status.
  bool GetStatus(GoogleString* status_string);

 private:
  void DecodeValueMatchingKeyAndCallCallback(
      const GoogleString& key, const char* data, size_t data_len,
      Callback* callback);

  AprMemCacheServers* servers_;
  CacheInterface* fallback_cache_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AprMemCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_MEM_CACHE_H_
