/**
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

// Unit-test the lru cache

#include "net/instaweb/util/public/http_cache.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace {
// Set the cache size large enough so nothing gets evicted during this test.
const int kMaxSize = 10000;
const char kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
}

namespace net_instaweb {

class HTTPCacheTest : public testing::Test {
 protected:
  static int64 ParseDate(const char* start_date) {
    int64 time_ms;
    MetaData::ParseTime(start_date, &time_ms);
    return time_ms;
  }

  HTTPCacheTest() : mock_timer_(ParseDate(kStartDate)),
                    http_cache_(new LRUCache(kMaxSize), &mock_timer_) {
  }

  void InitHeaders(MetaData* headers, const char* cache_control) {
    headers->Add("name", "value");
    headers->Add("Date", kStartDate);
    if (cache_control != NULL) {
      headers->Add("Cache-control", cache_control);
    }
    headers->SetStatusAndReason(HttpStatus::kOK);
    headers->ComputeCaching();
  }

  MockTimer mock_timer_;
  HTTPCache http_cache_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPCacheTest);
};

// Simple flow of putting in an item, getting it.
TEST_F(HTTPCacheTest, PutGet) {
  SimpleMetaData meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "public, max-age=300");
  http_cache_.Put("mykey", meta_data_in, "content", &message_handler_);
  EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query("mykey"));
  HTTPValue value;
  HTTPCache::FindResult found = http_cache_.Find(
      "mykey", &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  CharStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(std::string("value"), std::string(values[0]));
  EXPECT_EQ("content", contents);

  // Now advance time 301 seconds and the we should no longer
  // be able to fetch this resource out of the cache.
  mock_timer_.advance_ms(301 * 1000);
  found = http_cache_.Find("mykey", &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

// Verifies that the cache will 'remember' that a fetch should not be
// cached for 5 minutes.
TEST_F(HTTPCacheTest, RememberNotCacheable) {
  SimpleMetaData meta_data_out;
  http_cache_.RememberNotCacheable("mykey", &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch,
            http_cache_.Find("mykey", &value, &meta_data_out,
                             &message_handler_));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.advance_ms(301 * 1000);
  EXPECT_EQ(HTTPCache::kNotFound,
            http_cache_.Find("mykey", &value, &meta_data_out,
                             &message_handler_));
}

TEST_F(HTTPCacheTest, Uncacheable) {
  SimpleMetaData meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, NULL);
  http_cache_.Put("mykey", meta_data_in, "content", &message_handler_);
  EXPECT_EQ(CacheInterface::kNotFound, http_cache_.Query("mykey"));
  HTTPValue value;
  HTTPCache::FindResult found = http_cache_.Find(
      "mykey", &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

TEST_F(HTTPCacheTest, UncacheablePrivate) {
  SimpleMetaData meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "private, max-age=300");
  http_cache_.Put("mykey", meta_data_in, "content", &message_handler_);
  EXPECT_EQ(CacheInterface::kNotFound, http_cache_.Query("mykey"));
  HTTPValue value;
  HTTPCache::FindResult found = http_cache_.Find(
      "mykey", &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

}  // namespace net_instaweb
