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
// This is an entirely blocking interface.  Note that it does not
// implement CacheInterface: it's intended solely for use by
// apr_mem_cache.cc, so that a single memcached configuration can be
// combined with other caches without opening up redundant TCP/IP
// connections or making extra threads.
class AprMemCacheServers {
 public:
  // servers is a comma-separated list of host[:port] where port defaults
  // to 11211, the memcached default.
  //
  // thread_limit is used to provide apr_memcache_server_create with
  // a hard maximum number of client connections to open.
  AprMemCacheServers(const StringPiece& servers, int thread_limit,
                     Hasher* hasher, MessageHandler* handler);
  ~AprMemCacheServers();

  // Typedefs to facilitate returning values from a MultiGet.  The string data
  // is owned by the apr_pool_t* passed into Get and MultiGet.
  typedef std::pair<CacheInterface::KeyState, StringPiece> Result;
  typedef std::vector<Result> ResultVector;

  // Blocking get for a single value in one of the memcached servers.
  // Returns true for success, false for failure or not-found, placing
  // the result in *result, and allocating the result in data_pool.
  bool Get(const GoogleString& key, apr_pool_t* data_pool, StringPiece* result);

  // Sets the value of a cache item on one of the memcached servers.
  void Set(const GoogleString& key, const GoogleString& encoded_value);

  // Deletes an item from one of the memcached servers.
  void Delete(const GoogleString& key);

  // Performs a blocking multi-get, depositing the results in *results,
  // which will be sized the same as request.  If the call to memcached fails
  // completely, then false is returned and every one of the requested keys
  // should be considered a failure.
  bool MultiGet(CacheInterface::MultiGetRequest* request,
                apr_pool_t* data_pool,
                ResultVector* results);

  // Connects to the server, returning whether the connnection was
  // successful or not.
  bool Connect();

  bool valid_server_spec() const { return valid_server_spec_; }

  // Get detailed status in a string, returning false if the server
  // failed to return status.
  bool GetStatus(GoogleString* status_string);

 private:
  StringVector hosts_;
  std::vector<int> ports_;
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
