/*
 * Copyright 2010 Google Inc.
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

// Unit-test the memcache interface.

#include "net/instaweb/apache/apr_mem_cache.h"

#include <cstddef>

#include "apr_pools.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace {

const char kPortString[] = "6765";

}  // namespace

namespace net_instaweb {

class AprMemCacheTest : public CacheTestBase {
 protected:
  bool ConnectToMemcached(bool use_md5_hasher) {
    apr_initialize();
    GoogleString servers = StrCat("localhost:", kPortString);
    Hasher* hasher = &mock_hasher_;
    if (use_md5_hasher) {
      hasher = &md5_hasher_;
    }
    cache_.reset(new AprMemCache(servers, 5, hasher, &handler_));

    // apr_memcache actually lazy-connects to memcached, it seems, so
    // if we fail the Connect call then something is truly broken.  To
    // make sure memcached is actually up, we have to make an API
    // call, such as GetStatus.
    GoogleString buf;
    if (!cache_->Connect() || !cache_->GetStatus(&buf)) {
      LOG(ERROR) << "please start 'memcached -p 6765";
      return false;
    }
    return true;
  }
  virtual CacheInterface* Cache() { return cache_.get(); }

  GoogleMessageHandler handler_;
  MD5Hasher md5_hasher_;
  MockHasher mock_hasher_;
  scoped_ptr<AprMemCache> cache_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(AprMemCacheTest, PutGetDelete) {
  ASSERT_TRUE(ConnectToMemcached(true));

  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  cache_->Delete("Name");
  CheckNotFound("Name");
}

TEST_F(AprMemCacheTest, MultiGet) {
  ASSERT_TRUE(ConnectToMemcached(true));
  TestMultiGet();
}

TEST_F(AprMemCacheTest, BasicInvalid) {
  ASSERT_TRUE(ConnectToMemcached(true));

  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

TEST_F(AprMemCacheTest, SizeTest) {
  ASSERT_TRUE(ConnectToMemcached(true));

  for (int x = 0; x < 10; ++x) {
    for (int i = 128; i < (1<<20); i += i) {
      GoogleString value(i, 'a');
      GoogleString key = StrCat("big", IntegerToString(i));
      CheckPut(key.c_str(), value.c_str());
      CheckGet(key.c_str(), value.c_str());
    }
  }
}

TEST_F(AprMemCacheTest, StatsTest) {
  ASSERT_TRUE(ConnectToMemcached(true));
  GoogleString buf;
  ASSERT_TRUE(cache_->GetStatus(&buf));
  EXPECT_TRUE(buf.find("memcached server localhost:") != GoogleString::npos);
  EXPECT_TRUE(buf.find(" pid ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\nbytes_read: ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\ncurr_connections: ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\ntotal_items: ") != GoogleString::npos);
}

TEST_F(AprMemCacheTest, HashCollision) {
  ASSERT_TRUE(ConnectToMemcached(false));
  CheckPut("N1", "V1");
  CheckGet("N1", "V1");

  // Since we are using a mock hasher, which always returns "0", the
  // put on "N2" will overwrite "N1" in memcached due to hash
  // collision.
  CheckPut("N2", "V2");
  CheckGet("N2", "V2");
  CheckNotFound("N1");
}

}  // namespace net_instaweb
