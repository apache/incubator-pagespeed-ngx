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

// Unit-test the redis interface.

#include "pagespeed/system/redis_cache.h"

#include <cstddef>

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_test_base.h"

namespace net_instaweb {

class RedisCacheTest : public CacheTestBase {
 protected:
  RedisCacheTest() {}

  bool InitRedisOrSkip() {
    const char* portString = getenv("REDIS_PORT");
    int port;
    if (portString == nullptr || !StringToInt(portString, &port)) {
      LOG(ERROR) << "RedisCache tests are skipped because env var "
                 << "$REDIS_PORT is not set to an integer. Set that "
                 << "to the port number where redis is running to "
                 << "enable the tests. See install/run_program_with_redis.sh";
      return false;
    }

    cache_.reset(new RedisCache("localhost", port, &handler_));
    return cache_->Connect();
  }

  CacheInterface* Cache() override { return cache_.get(); }

 private:
  scoped_ptr<RedisCache> cache_;
  GoogleMessageHandler handler_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(RedisCacheTest, PutGetDelete) {
  if (!InitRedisOrSkip()) {
    return;
  }
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  CheckDelete("Name");
  CheckNotFound("Name");
}

TEST_F(RedisCacheTest, MultiGet) {
  if (!InitRedisOrSkip()) {
    return;
  }
  TestMultiGet();  // test from CacheTestBase is just fine
}

TEST_F(RedisCacheTest, BasicInvalid) {
  if (!InitRedisOrSkip()) {
    return;
  }

  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

}  // namespace net_instaweb
