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
#include "base/logging.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

RedisCache::RedisCache(const StringPiece& host, int port)
    : host_(host.as_string()), port_(port), redis_(nullptr) {}

bool RedisCache::Connect() {
  ShutDown();

  redis_ = redisConnect(host_.c_str(), port_);
  if (redis_ == nullptr) {
    LOG(ERROR) << "Cannot allocate redis context";
    return false;
  }
  if (redis_->err) {
    LOG(ERROR) << "Error while connecting to Redis: " << redis_->errstr;
    redisFree(redis_);
    return false;
  }
  return true;
}

bool RedisCache::IsHealthy() const {
  // Quoting hireds documentation: "once an error is returned the context cannot
  // be reused and you should set up a new connection"
  return redis_ != nullptr && redis_->err == 0;
}

void RedisCache::ShutDown() {
  if (redis_ == nullptr) {
    return;
  }
  redisFree(redis_);
  redis_ = nullptr;
}

void RedisCache::Get(const GoogleString& key, Callback* callback) {
  if (!IsHealthy()) {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    return;
  }

  redisReply* reply = static_cast<redisReply*>(
      redisCommand(redis_, "GET %b", key.data(), key.length()));
  KeyState keyState = CacheInterface::kNotFound;
  if (reply != nullptr) {
    if (reply->type == REDIS_REPLY_STRING) {
      // The only type of values that we store in Redis is string
      *callback->value() = SharedString(StringPiece(reply->str, reply->len));
      keyState = CacheInterface::kAvailable;
    } else if (reply->type == REDIS_REPLY_NIL) {
      // key not found, do nothing
    } else if (reply->type == REDIS_REPLY_ERROR) {
      LOG(ERROR) << "Redis GET returned error: "
                 << StringPiece(reply->str, reply->len);
    } else {
      LOG(ERROR) << "Unexpected reply type from redis GET: " << reply->type;
    }
    freeReplyObject(reply);
  }
  ValidateAndReportResult(key, keyState, callback);
}

void RedisCache::Put(const GoogleString& key, SharedString* value) {
  if (!IsHealthy()) {
    return;
  }

  // TODO(yeputons): check for server's response and log errors
  redisCommand(redis_, "SET %b %b",
               key.data(), key.length(),
               value->data(), static_cast<size_t>(value->size()));
}

void RedisCache::Delete(const GoogleString& key) {
  if (!IsHealthy()) {
    return;
  }

  // TODO(yeputons): check for server's response and log errors
  redisCommand(redis_, "DEL %b", key.data(), key.length());
}

}  // namespace net_instaweb
