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

  MockTimer mock_timer_;
  HTTPCache http_cache_;
  GoogleMessageHandler message_handler_;
};

// Simple flow of putting in an item, getting it.
TEST_F(HTTPCacheTest, PutGet) {
  SimpleMetaData meta_data_in, meta_data_out;
  meta_data_in.Add("name", "value");
  meta_data_in.Add("Date", kStartDate);
  meta_data_in.Add("Cache-control", "public, max-age=300");
  meta_data_in.set_status_code(HttpStatus::kOK);
  meta_data_in.ComputeCaching();
  http_cache_.Put("mykey", meta_data_in, "content", &message_handler_);
  EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query("mykey"));
  HTTPValue value;
  bool found = http_cache_.Get("mykey", &value, &meta_data_out,
                               &message_handler_);
  ASSERT_TRUE(found);
  ASSERT_TRUE(value.ExtractHeaders(&meta_data_out, &message_handler_));
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
  found = http_cache_.Get("mykey", &value, &meta_data_out, &message_handler_);
  EXPECT_FALSE(found);
}

TEST_F(HTTPCacheTest, Uncacheable) {
  SimpleMetaData meta_data_in, meta_data_out;
  meta_data_in.Add("name", "value");
  meta_data_in.Add("Date", kStartDate);
  meta_data_in.set_status_code(HttpStatus::kOK);
  meta_data_in.ComputeCaching();
  http_cache_.Put("mykey", meta_data_in, "content", &message_handler_);
  EXPECT_EQ(CacheInterface::kNotFound, http_cache_.Query("mykey"));
  HTTPValue value;
  bool found = http_cache_.Get("mykey", &value, &meta_data_out,
                               &message_handler_);
  ASSERT_FALSE(found);
}

}  // namespace net_instaweb
