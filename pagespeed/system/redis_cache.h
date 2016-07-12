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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "third_party/hiredis/src/hiredis.h"

namespace net_instaweb {

// Interface to Redis using hiredis library
// Details are changing rapidly.
// Right now this implementation uses sync API of hiredis and is, therefore,
// blocking. No auto-reconnection, logging or statistics.
class RedisCache : public CacheInterface {
 public:
  RedisCache(const StringPiece& host, int port);
  ~RedisCache() override { ShutDown(); }

  bool Connect();
  // TODO(yeputons): add connect&authenticate method (redis AUTH command)
  // TODO(yeputons): add ConnectWithTimeout method

  // CacheInterface implementations
  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, SharedString* value) override;
  void Delete(const GoogleString& key) override;

  // CacheInterface implementations
  GoogleString Name() const override { return "RedisCache"; }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override;
  void ShutDown() override;

 private:
  GoogleString host_;
  int port_;
  redisContext* redis_;

  DISALLOW_COPY_AND_ASSIGN(RedisCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_H_
