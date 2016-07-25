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

#include <cstddef>
#include <cstdarg>

#include "base/logging.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

RedisCache::RedisCache(const StringPiece& host, int port, AbstractMutex* mutex,
                       MessageHandler* message_handler)
    : host_(host.as_string()),
      port_(port),
      redis_(nullptr),
      mutex_(mutex),
      message_handler_(message_handler) {}

bool RedisCache::Connect() {
  ScopedMutex lock(mutex_.get());
  FreeRedisContext();

  redis_ = redisConnect(host_.c_str(), port_);
  if (redis_ == nullptr) {
    message_handler_->Message(kError, "Cannot allocate redis context");
    return false;
  }
  if (redis_->err) {
    LogRedisContextError("Error while connecting to redis");
    redisFree(redis_);
    redis_ = nullptr;
    return false;
  }
  return true;
}

bool RedisCache::IsHealthy() const {
  ScopedMutex lock(mutex_.get());
  return IsHealthyLockHeld();
}

bool RedisCache::IsHealthyLockHeld() const {
  // Quoting hireds documentation: "once an error is returned the context cannot
  // be reused and you should set up a new connection"
  return redis_ != nullptr && redis_->err == 0;
}

void RedisCache::ShutDown() {
  ScopedMutex lock(mutex_.get());
  FreeRedisContext();
}

void RedisCache::FreeRedisContext() {
  // TODO(yeputons): be careful when adding async requests: ShutDown can be
  // called while there are some unfinished requests, they should return.
  redisFree(redis_);  // handles nullptr correctly
  redis_ = nullptr;
}

void RedisCache::Get(const GoogleString& key, Callback* callback) {
  mutex_->Lock();
  if (!IsHealthyLockHeld()) {
    mutex_->Unlock();
    // TODO(yeputons): return kNetworkError instead?
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    return;
  }

  RedisReply reply = redisCommand("GET %b", key.data(), key.length());
  KeyState keyState = CacheInterface::kNotFound;
  bool reply_valid =
      ValidateRedisReply(reply, {REDIS_REPLY_STRING, REDIS_REPLY_NIL}, "GET");
  mutex_->Unlock();
  if (reply_valid) {
    if (reply->type == REDIS_REPLY_STRING) {
      // The only type of values that we store in Redis is string
      *callback->value() = SharedString(StringPiece(reply->str, reply->len));
      keyState = CacheInterface::kAvailable;
    } else {
      // REDIS_REPLY_NIL means 'key not found', do nothing
    }
  }
  ValidateAndReportResult(key, keyState, callback);
}

void RedisCache::Put(const GoogleString& key, SharedString* value) {
  ScopedMutex lock(mutex_.get());
  if (!IsHealthyLockHeld()) {
    return;
  }

  RedisReply reply = redisCommand(
      "SET %b %b",
      key.data(), key.length(),
      value->data(), static_cast<size_t>(value->size()));
  if (!ValidateRedisReply(reply, {REDIS_REPLY_STATUS}, "SET")) {
    return;
  }
  GoogleString answer(reply->str, reply->len);
  if (answer == "OK") {
    // success, nothing to do
  } else {
    LOG(DFATAL) << "Unexpected status from redis as answer to SET: " << answer;
    message_handler_->Message(
        kError, "Unexpected status from redis as answer to SET: %s",
        answer.c_str());
  }
}

void RedisCache::Delete(const GoogleString& key) {
  ScopedMutex lock(mutex_.get());
  if (!IsHealthyLockHeld()) {
    return;
  }

  RedisReply reply = redisCommand("DEL %b", key.data(), key.length());
  // Redis returns amount of keys deleted (probably, zero), no need in check
  // that amount; all other errors are handled by ValidateRedisReply
  ValidateRedisReply(reply, {REDIS_REPLY_INTEGER}, "DEL");
}

bool RedisCache::FlushAll() {
  ScopedMutex lock(mutex_.get());
  if (!IsHealthyLockHeld()) {
    return false;
  }

  RedisReply reply = redisCommand("FLUSHALL");
  return ValidateRedisReply(reply, {REDIS_REPLY_STATUS}, "FLUSHALL");
}

RedisCache::RedisReply RedisCache::redisCommand(const char* format, ...) {
  CHECK(redis_ != nullptr);
  va_list args;
  va_start(args, format);
  void* result = redisvCommand(redis_, format, args);
  va_end(args);
  return RedisReply(static_cast<redisReply*>(result));
}

void RedisCache::LogRedisContextError(const char* cause) {
  message_handler_->Message(kError, "%s: err flags is %d, %s",
                            cause, redis_->err, redis_->errstr);
}

bool RedisCache::ValidateRedisReply(const RedisReply& reply,
                                    std::initializer_list<int> valid_types,
                                    const char* command_executed) {
  if (reply == nullptr) {
    LogRedisContextError(command_executed);
    return false;
  }
  if (reply->type == REDIS_REPLY_ERROR) {
    GoogleString error(reply->str, reply->len);
    LOG(DFATAL) << command_executed << ": redis returned error: " << error;
    message_handler_->Message(kError, "%s: redis returned error: %s",
                              command_executed, error.c_str());
    return false;
  }
  for (int type : valid_types) {
    if (reply->type == type) {
      return true;
    }
  }
  LOG(DFATAL) << command_executed
              << ": unexpected reply type from redis: " << reply->type;
  message_handler_->Message(kError, "%s: unexpected reply type from redis: %d",
                            command_executed, reply->type);
  return false;
}

}  // namespace net_instaweb
