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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/property_cache_util.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/http.pb.h"

namespace net_instaweb {

namespace {

const char kTestProp[] = "test_property";
const char kRequestUrl[] = "http://www.example.com/";

// Unit tests for higher-level property cache utils. These use
// NameValue from http.proto as the stored proto type.
class PropertyCacheUtilTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    dom_cohort_ = SetupCohort(pcache_, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(dom_cohort_);
    ResetDriver();
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
  }

  const PropertyCache::Cohort* dom_cohort_;
  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(PropertyCacheUtilTest, WriteRead) {
  NameValue to_write;
  to_write.set_name("name");
  to_write.set_value("value");
  PropertyCacheUpdateResult write_status =
      UpdateInPropertyCache(
          to_write, rewrite_driver(), dom_cohort_, kTestProp, false);
  EXPECT_EQ(kPropertyCacheUpdateOk, write_status);

  PropertyCacheDecodeResult read_status;
  scoped_ptr<NameValue> result(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp,
          -1 /*no ttl check*/, &read_status));
  EXPECT_EQ(kPropertyCacheDecodeOk, read_status);
  ASSERT_TRUE(result.get() != NULL);
  EXPECT_STREQ("name", result->name());
  EXPECT_STREQ("value", result->value());
}

TEST_F(PropertyCacheUtilTest, WritePersistence) {
  NameValue to_write;
  to_write.set_name("name");
  to_write.set_value("value");
  PropertyCacheUpdateResult write_status =
      UpdateInPropertyCache(to_write, rewrite_driver(),
                            dom_cohort_, kTestProp,
                            false /* don't write out cohort*/);
  EXPECT_EQ(kPropertyCacheUpdateOk, write_status);

  ResetDriver();

  // We did not actually commit the cohort to cache, and reset the driver,
  // so the read should fail.

  PropertyCacheDecodeResult read_status;
  scoped_ptr<NameValue> result(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp,
          -1 /*no ttl check*/, &read_status));
  EXPECT_EQ(kPropertyCacheDecodeNotFound, read_status);
  EXPECT_TRUE(result.get() == NULL);

  // Now write again, but ask the routine to write out.
  write_status = UpdateInPropertyCache(
      to_write, rewrite_driver(), dom_cohort_, kTestProp,
      true /* do write out cohort */);
  EXPECT_EQ(kPropertyCacheUpdateOk, write_status);

  // Reset the driver, and re-read: should succeed.
  ResetDriver();
  result.reset(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp,
          -1 /*no ttl check*/, &read_status));
  EXPECT_EQ(kPropertyCacheDecodeOk, read_status);
  ASSERT_TRUE(result.get() != NULL);
  EXPECT_STREQ("name", result->name());
  EXPECT_STREQ("value", result->value());
}

TEST_F(PropertyCacheUtilTest, DecodeExpired) {
  NameValue to_write;
  to_write.set_name("name");
  to_write.set_value("value");
  PropertyCacheUpdateResult write_status =
      UpdateInPropertyCache(to_write, rewrite_driver(),
                            dom_cohort_, kTestProp, false);
  EXPECT_EQ(kPropertyCacheUpdateOk, write_status);

  AdvanceTimeMs(200);

  PropertyCacheDecodeResult read_status;
  scoped_ptr<NameValue> result(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp,
          100 /* ttl check */, &read_status));
  EXPECT_EQ(kPropertyCacheDecodeExpired, read_status);
  ASSERT_TRUE(result.get() == NULL);
}

TEST_F(PropertyCacheUtilTest, DecodeMissing) {
  PropertyCacheDecodeResult status;
  scoped_ptr<NameValue> result(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp, -1, &status));
  EXPECT_TRUE(result.get() == NULL);
  EXPECT_EQ(kPropertyCacheDecodeNotFound, status);
}

TEST_F(PropertyCacheUtilTest, DecodeError) {
  // Write something that probably doesn't decode as NameValue proto.
  rewrite_driver()->UpdatePropertyValueInDomCohort(
      rewrite_driver()->property_page(), kTestProp, "@(#(@(#@(");
  PropertyCacheDecodeResult status;
  scoped_ptr<NameValue> result(
      DecodeFromPropertyCache<NameValue>(
          rewrite_driver(), dom_cohort_, kTestProp, -1, &status));
  EXPECT_TRUE(result.get() == NULL);
  EXPECT_EQ(kPropertyCacheDecodeParseError, status);
}

}  // namespace

}  // namespace net_instaweb
