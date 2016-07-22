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

#include "pagespeed/kernel/cache/cache_key_prepender.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/in_memory_cache.h"

namespace {
const char kKeyPrefix[] = "Prefix_";
}  // namespace

namespace net_instaweb {

class CacheKeyPrependerTest : public CacheTestBase {
 protected:
  CacheKeyPrependerTest()
      : backend_cache_(), cache_(kKeyPrefix, &backend_cache_) {}

  CacheInterface* Cache() override { return &cache_; }

  InMemoryCache backend_cache_;
  CacheKeyPrepender cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheKeyPrependerTest);
};

TEST_F(CacheKeyPrependerTest, Get) {
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "Name"), "Value");

  CheckGet("Name", "Value");
}

TEST_F(CacheKeyPrependerTest, GetNotFound) {
  CheckPut(cache_.Backend(), "Name", "Value");

  // 'Name' should become 'Prefix_Name' and it's not in backend cache.
  CheckNotFound("Name");
}

TEST_F(CacheKeyPrependerTest, Put) {
  CheckPut("Name", "Value");

  CheckGet(cache_.Backend(), StrCat(kKeyPrefix, "Name"), "Value");
}

TEST_F(CacheKeyPrependerTest, Delete) {
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "Name"), "Value");

  CheckDelete("Name");

  CheckNotFound("Name");
}

TEST_F(CacheKeyPrependerTest, MultiGet) {
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "n0"), "v0");
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "n1"), "v1");
  Callback *n0 = AddCallback();
  Callback *not_found = AddCallback();
  Callback *n1 = AddCallback();

  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");

  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(not_found);
  WaitAndCheck(n1, "v1");
}

TEST_F(CacheKeyPrependerTest, BasicInvalid) {
  // Check that we honor callback veto on validity.
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "nameA"), "valueA");
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "nameB"), "valueB");
  set_invalid_key("nameA");

  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

TEST_F(CacheKeyPrependerTest, MultiGetInvalid) {
  // Check that we honor callback veto on validity in MultiGet.
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "n0"), "v0");
  CheckPut(cache_.Backend(), StrCat(kKeyPrefix, "n1"), "v1");
  set_invalid_key("n0");  // should be called before we create any callbacks
  Callback *n0 = AddCallback();
  Callback *not_found = AddCallback();
  Callback *n1 = AddCallback();

  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");

  WaitAndCheckNotFound(n0);
  WaitAndCheckNotFound(not_found);
  WaitAndCheck(n1, "v1");
}

}  // namespace net_instaweb
