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

#include <cstddef>
#include <vector>

#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

struct apr_memcache2_t;
struct apr_memcache2_server_t;
struct apr_pool_t;

namespace net_instaweb {

class Hasher;
class MessageHandler;
class SharedString;
class Statistics;
class Variable;

// Interface to memcached via the apr_memcache2*, as documented in
// http://apr.apache.org/docs/apr-util/1.4/group___a_p_r___util___m_c.html.
//
// While this class derives from CacheInterface, it is a blocking
// implementation, suitable for instantiating underneath an AsyncCache.
class AprMemCache : public CacheInterface {
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

  // Amount of time after a burst of errors to retry memcached operations.
  static const int64 kHealthCheckpointIntervalMs = 30 * Timer::kSecondMs;

  // Maximum number of errors tolerated within kHealthCheckpointIntervalMs,
  // after which AprMemCache will declare itself unhealthy for
  // kHealthCheckpointIntervalMs.
  static const int64 kMaxErrorBurst = 4;

  // servers is a comma-separated list of host[:port] where port defaults
  // to 11211, the memcached default.
  //
  // thread_limit is used to provide apr_memcache2_server_create with
  // a hard maximum number of client connections to open.
  AprMemCache(const StringPiece& servers, int thread_limit, Hasher* hasher,
              Statistics* statistics, Timer* timer, MessageHandler* handler);
  ~AprMemCache();

  static void InitStats(Statistics* statistics);

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

  virtual const char* Name() const { return "AprMemCache"; }
  virtual bool IsBlocking() const { return true; }
  virtual bool IsMachineLocal() const { return is_machine_local_; }

  // Records in statistics that a system error occurred, helping it detect
  // when it's unhealthy if they are too frequent.
  void RecordError();

  // Determines whether memcached is healthy enough to attempt another
  // operation.  Note that even though there may be multiple shards,
  // some of which are healthy and some not, we don't currently track
  // errors on a per-shard basis, so we effectively declare all the
  // memcached instances unhealthy if any of them are.
  virtual bool IsHealthy() const;

  // Close down the connection to the memcached servers.
  virtual void ShutDown();

 private:
  void DecodeValueMatchingKeyAndCallCallback(
      const GoogleString& key, const char* data, size_t data_len,
      const char* calling_method, Callback* callback);

  StringVector hosts_;
  std::vector<int> ports_;
  GoogleString server_spec_;
  bool valid_server_spec_;
  int thread_limit_;
  apr_pool_t* pool_;
  apr_memcache2_t* memcached_;
  std::vector<apr_memcache2_server_t*> servers_;
  Hasher* hasher_;
  Timer* timer_;
  AtomicBool shutdown_;

  Variable* timeouts_;
  Variable* last_error_checkpoint_ms_;
  Variable* error_burst_size_;

  bool is_machine_local_;
  MessageHandler* message_handler_;

  // When memcached is killed, we will generate errors for every cache
  // operation.  To bound the amount of logging we do, we keep track
  // of the last time when we issued a log message for an APR failure.
  // We use a Statistic here for this so that it's shared across
  // Apache processes.
  //
  // Note that we have some messages indicating a potential functional issue on
  // (e.g. key collision) and a variety of places where we print messages
  // because the Apr routine failed.  We are grouping together Apr failures
  // for Get, Put, Delete, and MultiGet.  We might at some point wish to
  // track the last time we sent a message for each of those.
  Variable* last_apr_error_;

  DISALLOW_COPY_AND_ASSIGN(AprMemCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_MEM_CACHE_H_
