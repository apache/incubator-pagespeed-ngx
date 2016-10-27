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

// Unit-test in-memory cache

#include "pagespeed/kernel/cache/in_memory_cache.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/cache/cache_test_base.h"

namespace net_instaweb {

class InMemoryCacheTest : public CacheTestBase {
 protected:
  InMemoryCacheTest() {}

  CacheInterface* Cache() override { return &cache_; }

  InMemoryCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryCacheTest);
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(InMemoryCacheTest, PutGetDelete) {
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  CheckDelete("Name");
  CheckNotFound("Name");
}

TEST_F(InMemoryCacheTest, HandlesStringPieces) {
  SharedString s("Value");
  s.RemovePrefix(1);
  s.RemoveSuffix(1);

  Cache()->Put("Name", s);

  CheckGet("Name", "alu");
}

TEST_F(InMemoryCacheTest, DetachesValueOnPut) {
  SharedString s("Value");
  Cache()->Put("Name", s);

  s.WriteAt(0, "-", 1);

  EXPECT_EQ("-alue", s.Value());
  CheckGet("Name", "Value");
}

TEST_F(InMemoryCacheTest, BasicInvalid) {
  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

TEST_F(InMemoryCacheTest, MultiGet) {
  // This covers CacheInterface's default implementation of MultiGet.
  TestMultiGet();
}

TEST_F(InMemoryCacheTest, DoesNotGetAfterShutdown) {
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  Cache()->ShutDown();
  CheckNotFound("Name");
}

}  // namespace net_instaweb
