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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/collect_subresources_filter.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {
namespace {
const int64 kOriginTtlMs = 12 * Timer::kMinuteMs;
const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
}

class CollectSubresourcesFilterTest : public ResourceManagerTestBase {
 public:
  CollectSubresourcesFilterTest() {}

 protected:
  void InitResources() {
    SetResponseWithDefaultHeaders("http://test.com/a.css", kContentTypeCss,
                                  " a ", kOriginTtlMs);

    SetResponseWithDefaultHeaders("http://test.com/b.js",
                                  kContentTypeJavascript, kJsData,
                                  kOriginTtlMs);
  }

  virtual void SetUp() {
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kFlushSubresources);
    options()->EnableExtendCacheFilters();
    options()->ComputeSignature(hasher());
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
    rewrite_driver()->set_user_agent(UserAgentStrings::kChromeUserAgent);
    SetupWriter();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectSubresourcesFilterTest);
};

TEST_F(CollectSubresourcesFilterTest, CollectSubresourcesFilter) {
  InitResources();
  GoogleString html_ip =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body>";

  Parse("not_flushed_early", html_ip);
  // CollectSubresourcesFilter should have populated the flush_early_info
  // proto with the appropriate subresources.
  EXPECT_EQ(2, rewrite_driver()->flush_early_info()->resources_size());
  EXPECT_EQ("http://test.com/a.css.pagespeed.ce.0.css",
            rewrite_driver()->flush_early_info()->resources(0));
  EXPECT_EQ("http://test.com/b.js.pagespeed.ce.0.js",
            rewrite_driver()->flush_early_info()->resources(1));
}

}  // namespace net_instaweb
