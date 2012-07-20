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
struct apr_pool_t;

namespace net_instaweb {

class Hasher;
class MessageHandler;
class SharedString;
class SlowWorker;
class ThreadSystem;
class Timer;

// Interface to memcached via libmemcached.  A memcached polling loop
// is run via a thread.
class AprMemCache : public CacheInterface {
 public:
  // servers is a comma-separated list of host[:port] where port defaults
  // to 11211, the memcached default.
  //
  // thread_limit is the maximum number of threads that might be needed,
  // which is the result of calling:
  //     int thread_limit;
  //     ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
  // TODO(jmarantz): consider also accounting for the number of threads
  // that we can create in PSA.
  AprMemCache(const StringPiece& servers, int thread_limit,
              Hasher* hasher, MessageHandler* handler);
  virtual ~AprMemCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

  virtual const char* Name() const { return "AprMemCache"; }

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
  ThreadSystem* thread_system_;
  Timer* timer_;
  Hasher* hasher_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AprMemCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_MEM_CACHE_H_
