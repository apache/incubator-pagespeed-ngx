/*
 * Copyright 2013 Google Inc.
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

#include "pagespeed/kernel/cache/compressed_cache.h"

#include <cstddef>

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_random.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

namespace {
const size_t kMaxSize = 10*kStackBufferSize;
}

class CompressedCacheTest : public CacheTestBase {
 protected:
  CompressedCacheTest()
      : lru_cache_(new LRUCache(kMaxSize)),
        thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        random_(thread_system_->NewMutex()) {
    CompressedCache::InitStats(&stats_);
    compressed_cache_.reset(new CompressedCache(lru_cache_.get(), &stats_));
  }

  // Get the raw compressed buffer out directly out of the LRU cache.
  GoogleString GetRawValue(const GoogleString& key) {
    Callback* callback = InitiateGet(lru_cache_.get(), key);
    callback->Wait();
    EXPECT_TRUE(callback->called());
    GoogleString ret;
    callback->value()->Value().CopyToString(&ret);
    PostOpCleanup();
    return ret;
  }

  virtual CacheInterface* Cache() { return compressed_cache_.get(); }

  GoogleMessageHandler handler_;
  scoped_ptr<LRUCache> lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<CompressedCache> compressed_cache_;
  SimpleRandom random_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(CompressedCacheTest, PutGetDelete) {
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  EXPECT_LT(0, lru_cache_->size_bytes());

  Cache()->Delete("Name");
  CheckNotFound("Name");

  EXPECT_EQ(0, lru_cache_->size_bytes());
  EXPECT_EQ(0, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, SizeTest) {
  GoogleString value(3 * kStackBufferSize, 'a');
  CheckPut("Name", value);
  CheckGet("Name", value);
  EXPECT_GT(100, lru_cache_->size_bytes());
  EXPECT_GT(100, compressed_cache_->CompressedSize());
  EXPECT_EQ(static_cast<int64>(value.size()),
            compressed_cache_->OriginalSize());
  EXPECT_EQ(0, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, LargeDataHighEntropy) {
  // The internals of the deflater work using kStackBufferSize, so we
  // want to make sure that we test with strings large enough to cover
  // the corner cases at the boundaries.  Note that in SizeTest above,
  // the input spills over kStackBufferSize, but the output does't
  // because that long string of 'a' compresses very well.
  GoogleString value = random_.GenerateHighEntropyString(5 * kStackBufferSize);
  CheckPut("Name", value);
  CheckGet("Name", value);
  EXPECT_LT(2*kStackBufferSize, lru_cache_->size_bytes());
  EXPECT_EQ(0, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, EmptyValue) {
  CheckPut("key", "");
  CheckGet("key", "");
  EXPECT_EQ(0, compressed_cache_->CorruptPayloads());
}

// Test a few patterns of corruption.  We do this by messing with the
// compressed bytes directly in the lru_cache_.

TEST_F(CompressedCacheTest, PhysicallyEmptyValue) {
  CheckPut(lru_cache_.get(), "key", "");

  // The physical value must have a signature written by
  // compressed_cache.cc, otherwise it reports a miss due to
  // corruption.
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, TotalGarbage) {
  CheckPut(lru_cache_.get(), "key", "garbage");
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, CrapAtEnd) {
  CheckPut("key", "garbage");
  GoogleString raw_value = GetRawValue("key");
  // Appending 'crap' to the raw value means we can no longer
  // decompress, so we expect a miss and a corruption count.
  StrAppend(&raw_value, "crap");
  lru_cache_->PutSwappingString("key", &raw_value);
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, CrapAtBeginning) {
  CheckPut("key", "garbage");
  GoogleString raw_value = GetRawValue("key");
  raw_value.insert(0, "crap");
  lru_cache_->PutSwappingString("key", &raw_value);
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, InsertInMiddle) {
  // Make sure that the corruption is detected multiple stack-buffers
  // into the compressed string.
  GoogleString value = random_.GenerateHighEntropyString(5 * kStackBufferSize);
  CheckPut("key", value);
  GoogleString raw_value = GetRawValue("key");
  raw_value.insert(raw_value.size() / 2, "crap");
  lru_cache_->PutSwappingString("key", &raw_value);
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

TEST_F(CompressedCacheTest, RemoveOneByteFromMiddle) {
  GoogleString value = random_.GenerateHighEntropyString(5 * kStackBufferSize);
  CheckPut("key", value);
  GoogleString raw_value = GetRawValue("key");
  raw_value.erase(raw_value.size() / 2, 1);
  lru_cache_->PutSwappingString("key", &raw_value);
  CheckNotFound("key");
  EXPECT_EQ(1, compressed_cache_->CorruptPayloads());
}

}  // namespace net_instaweb
