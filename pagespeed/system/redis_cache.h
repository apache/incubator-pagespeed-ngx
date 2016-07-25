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
#include "pagespeed/kernel/cache/cache_interface.h"
#include "third_party/hiredis/src/hiredis.h"

namespace net_instaweb {

// Interface to Redis using hiredis library
// Details are changing rapidly.
// Right now this implementation uses sync API of hiredis and is blocking.
// This class is thread-safe.
// TODO(yeputons): use async API
// TODO(yeputons): add auto-reconnection
// TODO(yeputons): add statistics
// TODO(yeputons): consider making Redis-reported errors treated as failures
class RedisCache : public CacheInterface {
 public:
  // Takes ownership of mutex. This mutex protects inner calls to hiredis only.
  // Does not take ownership of MessageHandler, and assumes the pointer is valid
  // throughout full lifetime of RedisCache
  RedisCache(const StringPiece& host, int port,
             AbstractMutex* mutex,
             MessageHandler* message_handler);
  ~RedisCache() override { ShutDown(); }

  bool Connect() LOCKS_EXCLUDED(mutex_);
  // TODO(yeputons): add connect&authenticate method (redis AUTH command)
  // TODO(yeputons): add ConnectWithTimeout method

  // CacheInterface implementations
  void Get(const GoogleString& key, Callback* callback) override
      LOCKS_EXCLUDED(mutex_);
  void Put(const GoogleString& key, SharedString* value) override
      LOCKS_EXCLUDED(mutex_);
  void Delete(const GoogleString& key) override LOCKS_EXCLUDED(mutex_);

  // CacheInterface implementations
  GoogleString Name() const override { return "RedisCache"; }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override LOCKS_EXCLUDED(mutex_);
  void ShutDown() override LOCKS_EXCLUDED(mutex_);

  // Flushes ALL DATA IN REDIS in blocking mode. Used in tests
  bool FlushAll();

 private:
  const GoogleString host_;
  const int port_;
  redisContext* redis_ GUARDED_BY(mutex_);
  scoped_ptr<AbstractMutex> mutex_;

  bool IsHealthyLockHeld() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void FreeRedisContext() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  MessageHandler *message_handler_;

  struct RedisReplyDeleter {
    void operator()(redisReply* ptr) {
      freeReplyObject(ptr);
    }
  };
  typedef std::unique_ptr<redisReply, RedisReplyDeleter> RedisReply;

  RedisReply redisCommand(const char* format, ...)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void LogRedisContextError(const char* cause) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool ValidateRedisReply(const RedisReply& reply,
                          std::initializer_list<int> valid_types,
                          const char* command_executed)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  DISALLOW_COPY_AND_ASSIGN(RedisCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_H_
