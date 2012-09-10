/*
 * Copyright 2012 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/rewriter/public/rewritten_content_scanning_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class AbstractMutex;

namespace {

const char kRequestUrl[] = "http://www.test.com";

class MockPage : public PropertyPage {
 public:
  explicit MockPage(AbstractMutex* mutex)
      : PropertyPage(mutex, kRequestUrl) {}
  virtual ~MockPage() {}
  virtual void Done(bool valid) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPage);
};

}  // namespace

class RewrittenContentScanningFilterTest : public RewriteTestBase {
 public:
  RewrittenContentScanningFilterTest() {}

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    rewrite_driver()->AddOwnedPostRenderFilter(
        new RewrittenContentScanningFilter(rewrite_driver_));
    resource_manager()->page_property_cache()->AddCohort(
        RewriteDriver::kDomCohort);
    resource_manager()->page_property_cache()->set_enabled(true);
    MockPage* page = new MockPage(factory_->thread_system()->NewMutex());
    rewrite_driver()->set_property_page(page);
    resource_manager()->page_property_cache()->Read(page);
    url_namer_.reset(new TestUrlNamer);
    resource_manager()->set_url_namer(url_namer_.get());
    url_namer_->SetProxyMode(true);
  }

  StringPiece GetNumRewrittenResources() {
    const PropertyCache::Cohort* cohort =
        resource_manager()->page_property_cache()->GetCohort(
            RewriteDriver::kDomCohort);
    PropertyValue* value = rewrite_driver()->property_page()->GetProperty(
        cohort,
        RewrittenContentScanningFilter::kNumProxiedRewrittenResourcesProperty);
    EXPECT_TRUE(value->has_value());
    return value->value();
  }

 private:
  scoped_ptr<TestUrlNamer> url_namer_;
  DISALLOW_COPY_AND_ASSIGN(RewrittenContentScanningFilterTest);
};

TEST_F(RewrittenContentScanningFilterTest, NoRewrittenResource) {
  const char kInputHtml[] =
      "<html>"
      "<head></head>"
      "<body>"
      "<img src=\"1.jpeg\"/>"
      "<script src=\"1.js\"/>"
      "</body>"
      "</html>";
  Parse("no_rewritten_resource", kInputHtml);
  EXPECT_EQ(GetNumRewrittenResources(), "0");
}

TEST_F(RewrittenContentScanningFilterTest, CountRewrittenResource) {
  const char kInputHtml[] =
      "<html>"
      "<head></head>"
      "<body>"
      "<link rel=\"stylesheet\" "
      "href=\"http://cdn.com/d.css.pagespeed.cf.0.css\"/>"
      "<script src=\"http://cdn.com/c.js.pagespeed.jm.0.js\"/>"
      "<script src=\"http://d.com/c.js.pagespeed.jm.0.js\"/>"
      "</body>"
      "</html>";
  Parse("count_rewritten_resource", kInputHtml);
  EXPECT_EQ(GetNumRewrittenResources(), "2");
}

}  // namespace net_instaweb
