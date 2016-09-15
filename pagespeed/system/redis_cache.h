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

#include <stdarg.h>

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
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/thread/thread_synchronizer.h"
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
// See redis_cache.cc for details on different locks that the class has.
//
// TODO(yeputons): consider extracting a common interface with AprMemCache.
// TODO(yeputons): consider making Redis-reported errors treated as failures.
// TODO(yeputons): add redis AUTH command support.
class RedisCache : public CacheInterface {
 public:
  // Uses ThreadSystem to generate several mutexes in constructor only.
  // Does not take ownership of MessageHandler, Timer, and assumes
  // that these pointers are valid throughout full lifetime of RedisCache.
  RedisCache(StringPiece host, int port, ThreadSystem* thread_system,
             MessageHandler* message_handler, Timer* timer,
             int64 reconnection_delay_ms, int64 timeout_us);
  ~RedisCache() override { ShutDown(); }

  GoogleString ServerDescription() const;

  void StartUp(bool connect_now = true);

  // CacheInterface implementations.
  GoogleString Name() const override { return FormatName(); }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override;
  void ShutDown() override;

  static GoogleString FormatName() { return "RedisCache"; }

  // CacheInterface implementations.
  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, SharedString* value) override;
  void Delete(const GoogleString& key) override;

  // Appends detailed server status to a string. Returns true if succeeded. If
  // the the server failed to report status, does not change the string.
  bool GetStatus(GoogleString* status_string);

  // Flushes ALL DATA IN REDIS in blocking mode. Used in tests.
  bool FlushAll();

 private:
  struct RedisReplyDeleter {
    void operator()(redisReply* ptr) {
      if (ptr != nullptr) {  // hiredis 0.11 does not check.
        freeReplyObject(ptr);
      }
    }
  };
  typedef std::unique_ptr<redisReply, RedisReplyDeleter> RedisReply;

  struct RedisContextDeleter {
    void operator()(redisContext* ptr) {
      if (ptr != nullptr) {  // hiredis 0.11 does not check.
        redisFree(ptr);
      }
    }
  };
  typedef std::unique_ptr<redisContext, RedisContextDeleter> RedisContext;

  class Connection {
   public:
    Connection(RedisCache* redis_cache, StringPiece host, int port);

    void StartUp(bool connect_now = true)
        LOCKS_EXCLUDED(redis_mutex_, state_mutex_);
    bool IsHealthy() const LOCKS_EXCLUDED(redis_mutex_, state_mutex_);
    void ShutDown() LOCKS_EXCLUDED(redis_mutex_, state_mutex_);

    AbstractMutex* GetOperationMutex() const LOCK_RETURNED(redis_mutex_) {
      return redis_mutex_.get();
    }

    RedisReply RedisCommand(const char* format, va_list args)
        EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_) LOCKS_EXCLUDED(state_mutex_);

    bool ValidateRedisReply(const RedisReply& reply,
                            std::initializer_list<int> valid_types,
                            const char* command_executed)
        EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_) LOCKS_EXCLUDED(state_mutex_);

   private:
    enum State {
      kShutDown,
      kDisconnected,
      kConnecting,
      kConnected
    };

    bool IsHealthyLockHeld() const EXCLUSIVE_LOCKS_REQUIRED(state_mutex_);
    void UpdateState() EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_, state_mutex_);

    bool EnsureConnection() EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_)
        LOCKS_EXCLUDED(state_mutex_);

    RedisContext TryConnect() LOCKS_EXCLUDED(redis_mutex_, state_mutex_);

    void LogRedisContextError(redisContext* redis, const char* cause);

    const RedisCache* redis_cache_;
    const GoogleString host_;
    const int port_;
    const scoped_ptr<AbstractMutex> redis_mutex_;
    const scoped_ptr<AbstractMutex> state_mutex_;

    RedisContext redis_ GUARDED_BY(redis_mutex_);
    State state_ GUARDED_BY(state_mutex_);
    int64 next_reconnect_at_ms_ GUARDED_BY(state_mutex_);

    DISALLOW_COPY_AND_ASSIGN(Connection);
  };

  // Performs specified command, handling all reconnections, redirections
  // from Redis Cluster, and other stuff. Then it checks that Redis' reply
  // has expected type and returns the reply object. If anything fails,
  // returns nullptr.
  //
  // TODO(yeputons): it's expected that destruction of RedisReply is
  // thread-safe, which is true as of hiredis 0.13. Otherwise it's wrong to
  // return RedisReply here because lock is released on exit.
  // See https://github.com/redis/hiredis/issues/465
  RedisReply RedisCommand(const char* format,
                          std::initializer_list<int> valid_reply_types, ...);

  ThreadSynchronizer* GetThreadSynchronizerForTesting() const {
    return thread_synchronizer_.get();
  }

  const GoogleString main_host_;
  const int main_port_;
  ThreadSystem* thread_system_;
  MessageHandler* message_handler_;
  Timer* timer_;
  const int64 reconnection_delay_ms_;
  const int64 timeout_us_;
  const scoped_ptr<ThreadSynchronizer> thread_synchronizer_;

  const scoped_ptr<Connection> main_connection_;

  friend class RedisCacheTest;
  DISALLOW_COPY_AND_ASSIGN(RedisCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_H_
