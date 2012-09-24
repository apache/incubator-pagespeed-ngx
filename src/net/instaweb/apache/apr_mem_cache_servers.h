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

#ifndef NET_INSTAWEB_APACHE_APR_MEM_CACHE_SERVERS_H_
#define NET_INSTAWEB_APACHE_APR_MEM_CACHE_SERVERS_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct apr_memcache_t;
struct apr_memcache_server_t;
struct apr_pool_t;

namespace net_instaweb {

class Hasher;
class MessageHandler;
class SharedString;

// Interface to memcached via the apr_memcache*, as documented in
// http://apr.apache.org/docs/apr-util/1.4/group___a_p_r___util___m_c.html.
//
// While this class derives from CacheInterface, it is a blocking
// implementation, suitable for instantiating underneath an AsyncCache.
//
// TODO(jmarantz): rename to AprMemCache and rename cc/h to apr_mem_cache.*.
class AprMemCacheServers : public CacheInterface {
 public:
  // Experimentally it seems large values larger than 1M bytes result in
  // a failure, e.g. from load-tests:
  //     [Fri Jul 20 10:29:34 2012] [error] [mod_pagespeed 0.10.0.0-1699 @1522]
  //     AprMemCache::Put error: Internal error on key
  //     http://example.com/image.jpg, value-size 1393146
  // External to this class, we use a fallback cache (in Apache a FileCache) to
  // handle too-large requests.  This is managed by class FallbackCache in
  // ../util.
  static const size_t kValueSizeThreshold = 1 * 1000 * 1000;

  // servers is a comma-separated list of host[:port] where port defaults
  // to 11211, the memcached default.
  //
  // thread_limit is used to provide apr_memcache_server_create with
  // a hard maximum number of client connections to open.
  AprMemCacheServers(const StringPiece& servers, int thread_limit,
                     Hasher* hasher, MessageHandler* handler);
  ~AprMemCacheServers();

  const GoogleString& server_spec() const { return server_spec_; }

  // As mentioned above, Get and MultiGet are blocking in this implementation.
  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);

  // Connects to the server, returning whether the connnection was
  // successful or not.
  bool Connect();

  bool valid_server_spec() const { return valid_server_spec_; }

  // Get detailed status in a string, returning false if the server
  // failed to return status.
  bool GetStatus(GoogleString* status_string);

  virtual const char* Name() const { return "AprMemCacheServers"; }

 private:
  void DecodeValueMatchingKeyAndCallCallback(
      const GoogleString& key, const char* data, size_t data_len,
      Callback* callback);

  StringVector hosts_;
  std::vector<int> ports_;
  GoogleString server_spec_;
  bool valid_server_spec_;
  int thread_limit_;
  apr_pool_t* pool_;
  apr_memcache_t* memcached_;
  std::vector<apr_memcache_server_t*> servers_;
  Hasher* hasher_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AprMemCacheServers);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_MEM_CACHE_SERVERS_H_
