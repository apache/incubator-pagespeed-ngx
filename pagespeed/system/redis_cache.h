/*
 * Copyright 2016 Google Inc.
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

// Author: yeputons@google.com (Egor Suvorov)

#ifndef PAGESPEED_SYSTEM_REDIS_CACHE_H_
#define PAGESPEED_SYSTEM_REDIS_CACHE_H_

#include <memory>
#include <initializer_list>
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "third_party/hiredis/src/hiredis.h"

namespace net_instaweb {

// Interface to Redis using hiredis library. This implementation is blocking and
// thread-safe. One should call StartUp() before making any requests and
// ShutDown() after finishing work. Connecting will be initiated on StartUp()
// call. If it fails there or is dropped after any request, the following
// reconnection strategy is used:
// 1. If an operation fails because of communication or protocol error, try
//    reconnecting on the next Get/Put/Delete (without delay).
// 2. If (re-)connection attempt is unsuccessfull, try again on next
//    Get/Put/Delete operation, but not until at least reconnection_delay_ms_
//    have passed from the previous attempt.
// That ensures that we do not try to connect to unreachable server a lot, but
// still allows us to reconnect quickly in case of network glitches.
//
// TODO(yeputons): consider extracting a common interface with AprMemCache.
// TODO(yeputons): consider making Redis-reported errors treated as failures.
// TODO(yeputons): add redis AUTH command support.
class RedisCache : public CacheInterface {
 public:
  // Takes ownership of mutex, it should not be used outside RedisCache
  // afterwards. Does not take ownership of MessageHandler or Timer, and assumes
  // that these pointers are valid throughout full lifetime of RedisCache.
  RedisCache(StringPiece host, int port, AbstractMutex* mutex,
             MessageHandler* message_handler, Timer* timer,
             int64 reconnection_delay_ms, int64 timeout_us);
  ~RedisCache() override { ShutDown(); }

  GoogleString ServerDescription() const;

  void StartUp() LOCKS_EXCLUDED(mutex_);

  // CacheInterface implementations.
  GoogleString Name() const override { return FormatName(); }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override LOCKS_EXCLUDED(mutex_);
  void ShutDown() override LOCKS_EXCLUDED(mutex_);

  static GoogleString FormatName() { return "RedisCache"; }

  // CacheInterface implementations.
  void Get(const GoogleString& key, Callback* callback) override
      LOCKS_EXCLUDED(mutex_);
  void Put(const GoogleString& key, SharedString* value) override
      LOCKS_EXCLUDED(mutex_);
  void Delete(const GoogleString& key) override LOCKS_EXCLUDED(mutex_);

  // Appends detailed server status to a string. Returns true if succeeded. If
  // the the server failed to report status, does not change the string.
  bool GetStatus(GoogleString* status_string) LOCKS_EXCLUDED(mutex_);

  // Flushes ALL DATA IN REDIS in blocking mode. Used in tests.
  bool FlushAll() LOCKS_EXCLUDED(mutex_);

 private:
  struct RedisReplyDeleter {
    void operator()(redisReply* ptr) {
      freeReplyObject(ptr);
    }
  };
  typedef std::unique_ptr<redisReply, RedisReplyDeleter> RedisReply;

  bool Reconnect() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsHealthyLockHeld() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void FreeRedisContext() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  RedisReply RedisCommand(const char* format, ...)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void LogRedisContextError(const char* cause)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool ValidateRedisReply(const RedisReply& reply,
                          std::initializer_list<int> valid_types,
                          const char* command_executed)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const GoogleString host_;
  const int port_;
  const scoped_ptr<AbstractMutex> mutex_;
  MessageHandler* message_handler_;
  Timer* timer_;
  const int64 reconnection_delay_ms_;
  const int64 timeout_us_;

  redisContext *redis_ GUARDED_BY(mutex_);
  int64 next_reconnect_at_ms_ GUARDED_BY(mutex_);
  bool is_started_up_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(RedisCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_H_
