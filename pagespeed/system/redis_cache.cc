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

#include "pagespeed/system/redis_cache.h"

#include <sys/time.h>
#include <cstddef>
#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Hiredis is a non-thread-safe C library which we wrap around. We could use a
// single mutex for all operations, but we want to have two properties:
// 1. IsHealthy() never locks for a long time.
//    Why: it's specification of CacheInterface and it's called in a part of
//    rewriting where we need to know quickly whether to bother doing work.
// 2. If there is a connection in progress, at most one thread is locked. All
//    other threads fail current operation and unlock. All new threads return
//    failure immediately.
//    Why: connection happens after previous connection drops, which is caused
//    by network glitches or server failure. Either way, it's reasonable to
//    expect that reconnection may take a very long time, therefore we do not
//    want many threads to wait for it.
//
// Some invariants:
// 1. state_mutex_ can be locked for constant time only (by a single thread).
// 2. Current state of cache is kept in bunch of variables protected by
//    state_mutex_, therefore IsHealthy() can return fast.
// 3. All Redis-related errors are detected and reflected in state_ by
//    UpdateState(), which should be called after operations on redis_.
//    Note that if you have not actually performed operation on redis_, you
//    should not call it.
// 4. Mutexes should be locked in that order: redis_mutex_, state_mutex_.
// 5. Thread that wants to change state variables should have BOTH redis_mutex_
//    and state_mutex_. It should happen automatically: if thread does not hold
//    redis_mutex_, it cannot do anything with cache and cannot change state.
// 6. redis_mutex_ can be locked for non-constant time only if it's for
//    operation on healthy redis_. Ideologically, redis_mutex_ is used for
//    'queueing' Redis commands.
// 7. If reconnection is required, redis_mutex_ is unlocked so other threads can
//    unlock, see state_ == kConnecting and fail their operations. The mutex is
//    locked back after re-connection is completed.

RedisCache::RedisCache(StringPiece host, int port, ThreadSystem* thread_system,
                       MessageHandler* message_handler, Timer* timer,
                       int64 reconnection_delay_ms, int64 timeout_us)
    : main_host_(host.as_string()),
      main_port_(port),
      thread_system_(thread_system),
      message_handler_(message_handler),
      timer_(timer),
      reconnection_delay_ms_(reconnection_delay_ms),
      timeout_us_(timeout_us),
      thread_synchronizer_(new ThreadSynchronizer(thread_system)),
      main_connection_(new Connection(this, main_host_, main_port_)) {}

GoogleString RedisCache::ServerDescription() const {
  return StrCat(main_host_, ":", IntegerToString(main_port_));
}

void RedisCache::StartUp(bool connect_now) {
  main_connection_->StartUp(connect_now);
}

bool RedisCache::IsHealthy() const {
  return main_connection_->IsHealthy();
}

void RedisCache::ShutDown() {
  main_connection_->ShutDown();
}

void RedisCache::Get(const GoogleString& key, Callback* callback) {
  KeyState keyState = CacheInterface::kNotFound;
  RedisReply reply =
      RedisCommand("GET %b", {REDIS_REPLY_STRING, REDIS_REPLY_NIL}, key.data(),
                   key.length());

  if (reply) {
    if (reply->type == REDIS_REPLY_STRING) {
      // The only type of values that we store in Redis is string.
      *callback->value() = SharedString(StringPiece(reply->str, reply->len));
      keyState = CacheInterface::kAvailable;
    } else {
      // REDIS_REPLY_NIL means 'key not found', do nothing.
    }
  }
  ValidateAndReportResult(key, keyState, callback);
}

void RedisCache::Put(const GoogleString& key, SharedString* value) {
  RedisReply reply = RedisCommand(
      "SET %b %b",
      {REDIS_REPLY_STATUS},
      key.data(), key.length(),
      value->data(), static_cast<size_t>(value->size()));

  if (!reply) {
    return;
  }

  GoogleString answer(reply->str, reply->len);
  if (answer == "OK") {
    // Success, nothing to do.
  } else {
    LOG(DFATAL) << "Unexpected status from redis as answer to SET: " << answer;
    message_handler_->Message(
        kError, "Unexpected status from redis as answer to SET: %s",
        answer.c_str());
  }
}

void RedisCache::Delete(const GoogleString& key) {
  // Redis returns amount of keys deleted (probably, zero), no need in check
  // that amount; all other errors are handled by RedisCommand.
  RedisCommand("DEL %b", {REDIS_REPLY_INTEGER}, key.data(), key.length());
}

bool RedisCache::GetStatus(GoogleString* buffer) {
  RedisReply reply = RedisCommand("INFO", {REDIS_REPLY_STRING});
  if (!reply) {
    return false;
  }

  StrAppend(buffer, "Statistics for Redis (", ServerDescription(), "):\n");
  StrAppend(buffer, reply->str);
  return true;
}

bool RedisCache::FlushAll() {
  return RedisCommand("FLUSHALL", {REDIS_REPLY_STATUS}) != nullptr;
}

RedisCache::RedisReply RedisCache::RedisCommand(
    const char* format, std::initializer_list<int> valid_reply_types, ...) {
  ScopedMutex lock(main_connection_->GetOperationMutex());

  va_list args;
  va_start(args, valid_reply_types);
  RedisReply reply = main_connection_->RedisCommand(format, args);
  va_end(args);

  GoogleString command = format;
  command = command.substr(0, command.find_first_of(' '));
  if (main_connection_->ValidateRedisReply(reply, valid_reply_types,
                                           command.c_str())) {
    return reply;
  } else {
    return nullptr;
  }
}

RedisCache::Connection::Connection(RedisCache* redis_cache, StringPiece host,
                                   int port)
    : redis_cache_(redis_cache),
      host_(host.as_string()),
      port_(port),
      redis_mutex_(redis_cache_->thread_system_->NewMutex()),
      state_mutex_(redis_cache_->thread_system_->NewMutex()),
      redis_(nullptr),
      state_(kShutDown),
      next_reconnect_at_ms_(redis_cache_->timer_->NowMs()) {}

void RedisCache::Connection::StartUp(bool connect_now) {
  ScopedMutex lock1(redis_mutex_.get());
  {
    ScopedMutex lock2(state_mutex_.get());
    CHECK_EQ(state_, kShutDown);
    state_ = kDisconnected;
  }
  if (connect_now) {
    EnsureConnection();
  }
}

bool RedisCache::Connection::IsHealthy() const {
  ScopedMutex lock(state_mutex_.get());
  return IsHealthyLockHeld();
}

void RedisCache::Connection::ShutDown() {
  ScopedMutex lock1(redis_mutex_.get());
  ScopedMutex lock2(state_mutex_.get());
  if (state_ == kShutDown) {
    return;
  }
  // As we were able to grab redis_mutex_, there is no operation in progress,
  // maybe except for connection. EnsureConnection() handles the possibility
  // that a shutdown happens while it has released its lock and is waiting
  // for TryConnect().
  //
  // TODO(yeputons): be careful when adding async requests: ShutDown can be
  // called while there are some unfinished requests, they should return.
  redis_.reset();
  state_ = kShutDown;
}

bool RedisCache::Connection::EnsureConnection() {
  {
    ScopedMutex lock(state_mutex_.get());
    if (state_ == kConnected) {
      return true;
    }
    DCHECK(!redis_);
    // IsHealthyLockHeld() knows whether reconnection is sensible.
    if (!IsHealthyLockHeld()) {
      return false;
    }
    redis_.reset();
    state_ = kConnecting;
  }

  // redis_mutex_ is held on entry to EnsureConnection().
  redis_mutex_->Unlock();
  RedisContext loc_redis = TryConnect();
  redis_mutex_->Lock();

  ScopedMutex lock(state_mutex_.get());
  if (state_ == kConnecting) {
    CHECK(!redis_);
    redis_ = std::move(loc_redis);

    next_reconnect_at_ms_ = redis_cache_->timer_->NowMs();
    if (!redis_) {
      // It's better to wait some time before next attempt.
      next_reconnect_at_ms_ += redis_cache_->reconnection_delay_ms_;
    }
    UpdateState();
  } else {
    // Looks like cache was shut down. It also possible that it was started up
    // back (though it's prohibited by CacheInterface), but in that case we
    // will just fail this old operation, no big deal.
    DCHECK_EQ(state_, kShutDown);
  }
  return state_ == kConnected;
}

RedisCache::RedisContext RedisCache::Connection::TryConnect() {
  struct timeval timeout;
  timeout.tv_sec = redis_cache_->timeout_us_ / Timer::kSecondUs;
  timeout.tv_usec = redis_cache_->timeout_us_ % Timer::kSecondUs;

  RedisContext loc_redis(
      redisConnectWithTimeout(host_.c_str(), port_, timeout));
  redis_cache_->thread_synchronizer_->Signal("RedisConnect.After.Signal");
  redis_cache_->thread_synchronizer_->Wait("RedisConnect.After.Wait");

  if (loc_redis == nullptr) {
    redis_cache_->message_handler_->Message(kError,
                                            "Cannot allocate redis context");
  } else if (loc_redis->err) {
    LogRedisContextError(loc_redis.get(), "Error while connecting to redis");
  } else if (redisSetTimeout(loc_redis.get(), timeout) != REDIS_OK) {
    LogRedisContextError(loc_redis.get(),
                         "Error while setting timeout on redis context");
  } else {
    return loc_redis;
  }
  return nullptr;
}

bool RedisCache::Connection::IsHealthyLockHeld() const {
  switch (state_) {
    case kShutDown:
      return false;
    case kDisconnected:
      // Reconnection can happen during cache request onnly. We want some thread
      // to make a cache request, so we return `true` if cache is eligible for
      // reconnection.
      return next_reconnect_at_ms_ <= redis_cache_->timer_->NowMs();
    case kConnecting:
      return false;
    case kConnected:
      return true;
  }
  // gcc thinks following lines are reachable.
  LOG(FATAL) << "Invalid state_ in IsHealthyLockHeld()";
  return false;
}

void RedisCache::Connection::UpdateState() {
  // Quoting hireds documentation: "once an error is returned the context cannot
  // be reused and you should set up a new connection".
  if (redis_ != nullptr && redis_->err == 0) {
    state_ = kConnected;
  } else {
    state_ = kDisconnected;
    redis_.reset();
  }
}

RedisCache::RedisReply RedisCache::Connection::RedisCommand(const char* format,
                                                            va_list args) {
  if (!EnsureConnection()) {
    return nullptr;
  }

  void* result = redisvCommand(redis_.get(), format, args);
  redis_cache_->thread_synchronizer_->Signal("RedisCommand.After.Signal");
  redis_cache_->thread_synchronizer_->Wait("RedisCommand.After.Wait");

  return RedisReply(static_cast<redisReply*>(result));
}

void RedisCache::Connection::LogRedisContextError(redisContext* context,
                                      const char* cause) {
  if (context == nullptr) {
    // Can happen if EnsureConnection() failed to allocate context
    redis_cache_->message_handler_->Message(
        kError, "%s: unknown error (redis context is not available)", cause);
  } else {
    redis_cache_->message_handler_->Message(kError, "%s: err flags is %d, %s",
                              cause, context->err, context->errstr);
  }
}

bool RedisCache::Connection::ValidateRedisReply(const RedisReply& reply,
                                    std::initializer_list<int> valid_types,
                                    const char* command_executed) {
  {
    ScopedMutex lock(state_mutex_.get());
    if (state_ != kConnected) {
      // We should not have made a request to Redis at all.
      // Thus, reply is non-existent and there are no errors to log.
      DCHECK(!reply);
      return false;
    }
  }
  bool valid = false;
  if (reply == nullptr) {
    LogRedisContextError(redis_.get(), command_executed);
  } else if (reply->type == REDIS_REPLY_ERROR) {
    GoogleString error(reply->str, reply->len);
    LOG(DFATAL) << command_executed << ": redis returned error: " << error;
    redis_cache_->message_handler_->Message(kError,
                                            "%s: redis returned error: %s",
                                            command_executed, error.c_str());
  } else {
    for (int type : valid_types) {
      if (reply->type == type) {
        valid = true;
      }
    }
    if (!valid) {
      LOG(DFATAL) << command_executed
                  << ": unexpected reply type from redis: " << reply->type;
      redis_cache_->message_handler_->Message(kError,
                                "%s: unexpected reply type from redis: %d",
                                command_executed, reply->type);
    }
  }
  ScopedMutex lock(state_mutex_.get());
  UpdateState();
  return valid;
}

}  // namespace net_instaweb
