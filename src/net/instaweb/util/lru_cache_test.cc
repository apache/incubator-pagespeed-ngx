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

// Unit-test the lru cache

#include "net/instaweb/util/public/lru_cache.h"

#include <cstddef>
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {
const size_t kMaxSize = 100;
}

namespace net_instaweb {

class LRUCacheTest : public CacheTestBase {
 protected:
  LRUCacheTest()
      : cache_(kMaxSize) {
  }

  virtual CacheInterface* Cache() { return &cache_; }
  virtual void PostOpCleanup() { cache_.SanityCheck(); }

  LRUCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LRUCacheTest);
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(LRUCacheTest, PutGetDelete) {
  EXPECT_EQ(static_cast<size_t>(0), cache_.size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), cache_.num_elements());
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  EXPECT_EQ(static_cast<size_t>(9), cache_.size_bytes());  // "Name" + "Value"
  EXPECT_EQ(static_cast<size_t>(1), cache_.num_elements());
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");
  EXPECT_EQ(static_cast<size_t>(12),
            cache_.size_bytes());  // "Name" + "NewValue"
  EXPECT_EQ(static_cast<size_t>(1), cache_.num_elements());

  cache_.Delete("Name");
  cache_.SanityCheck();
  SharedString value_buffer;
  CheckNotFound("Name");
  EXPECT_EQ(static_cast<size_t>(0), cache_.size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), cache_.num_elements());
}

// Test eviction.  We happen to know that the cache does not account for
// STL overhead -- it's just counting key/value size.  Exploit that to
// understand when objects fall off the end.
TEST_F(LRUCacheTest, LeastRecentlyUsed) {
  // Fill the cache.
  GoogleString keys[10], values[10];
  const char key_pattern[]      = "name%d";
  const char value_pattern[]    = "valu%d";
  const int key_plus_value_size = 10;  // strlen("name7") + strlen("valu7")
  const size_t num_elements        = kMaxSize / key_plus_value_size;
  for (int i = 0; i < 10; ++i) {
    SStringPrintf(&keys[i], key_pattern, i);
    SStringPrintf(&values[i], value_pattern, i);
    CheckPut(keys[i], values[i]);
  }
  EXPECT_EQ(kMaxSize, cache_.size_bytes());
  EXPECT_EQ(num_elements, cache_.num_elements());

  // Ensure we can see those.
  for (int i = 0; i < 10; ++i) {
    CheckGet(keys[i], values[i]);
  }

  // Now if we insert a new entry totaling 10 bytes, that should work,
  // but we will lose name0 due to LRU semantics.  We should still have name1,
  // and by Get-ing name1 it we will make it the MRU.
  CheckPut("nameA", "valuA");
  CheckGet("nameA", "valuA");
  CheckNotFound("name0");
  CheckGet("name1", "valu1");

  // So now when we put in nameB,valuB we will lose name2 but keep name1,
  // which got bumped up to the MRU when we checked it above.
  CheckPut("nameB", "valuB");
  CheckGet("nameB", "valuB");
  CheckGet("name1", "valu1");
  CheckNotFound("name2");

  // Now insert something 1 byte too big, spelling out "value" this time.
  // We will now lose name3 and name4.  We should still have name5-name9,
  // plus name1, nameA, and nameB.
  CheckPut("nameC", "valueC");
  CheckNotFound("name3");
  CheckNotFound("name4");
  CheckGet("nameA", "valuA");
  CheckGet("nameB", "valuB");
  CheckGet("nameC", "valueC");
  CheckGet("name1", "valu1");
  for (int i = 5; i < 10; ++i) {
    CheckGet(keys[i], values[i]);
  }

  // Now the oldest item is "nameA".  Freshen it by re-inserting it, tickling
  // the code-path in lru_cache.cc that special-cases handling of re-inserting
  // the same value.
  CheckPut("nameA", "valuA");
  CheckPut("nameD", "valuD");
  // nameB should be evicted, the others should be retained.
  CheckNotFound("nameB");
  CheckGet("nameA", "valuA");
  CheckGet("nameC", "valueC");
  CheckGet("name1", "valu1");
  for (int i = 5; i < 10; ++i) {
    CheckGet(keys[i], values[i]);
  }
}

TEST_F(LRUCacheTest, BasicInvalid) {
  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

TEST_F(LRUCacheTest, MultiGet) {
  // This covers CacheInterface's default implementation of MultiGet.
  TestMultiGet();
}

}  // namespace net_instaweb
