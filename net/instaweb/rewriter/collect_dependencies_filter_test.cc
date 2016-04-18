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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/dependency_tracker.h"

#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com/";

class CollectDependenciesFilterTest : public RewriteTestBase {
 protected:
  void SetUp() override {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kExperimentHttp2);

    // Setup pcache.
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    const PropertyCache::Cohort* deps_cohort =
        SetupCohort(pcache_, RewriteDriver::kDependenciesCohort);
    server_context()->set_dependencies_cohort(deps_cohort);
    ResetDriver();

    // TODO(morlovich): Once indirect dependency collection is in, make sure to
    // test that it interacts with this correctly.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  " *  { display: block }", 100);
    SetResponseWithDefaultHeaders("c.css", kContentTypeCss,
                                  " *  { display: list-item }", 100);

    SetResponseWithDefaultHeaders("b.js",  kContentTypeJavascript,
                                  " var b  = 42", 200);
    SetResponseWithDefaultHeaders("d.js",  kContentTypeJavascript,
                                  " var d  = 32", 200);
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
  }

  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(CollectDependenciesFilterTest, BasicOperation) {
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<script src=b.js></script>";

  const char kOutput[] =
    "<link rel=stylesheet href=A.a.css.pagespeed.cf.0.css>"
    "<script src=b.js.pagespeed.jm.0.js></script>";

  ValidateExpected("basic_res", kInput, kOutput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  ASSERT_EQ(2, tracker->read_in_info()->dependency_size());
  EXPECT_EQ(StrCat(kTestDomain, "A.a.css.pagespeed.cf.0.css"),
            tracker->read_in_info()->dependency(0).url());
  EXPECT_EQ(DEP_CSS, tracker->read_in_info()->dependency(0).content_type());
  EXPECT_EQ(StrCat(kTestDomain, "b.js.pagespeed.jm.0.js"),
            tracker->read_in_info()->dependency(1).url());
  EXPECT_EQ(DEP_JAVASCRIPT,
            tracker->read_in_info()->dependency(1).content_type());

  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, Inliners) {
  // Currently we don't collect info on inline resources --- the filters
  // themsleves are expected to help --- but we should at least behave
  // sanely on them.
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptInline);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<script src=b.js></script>";

  const char kOutput[] =
    "<style>*{display:block}</style>"
    "<script>var b=42</script>";

  ValidateExpected("inliners", kInput, kOutput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  EXPECT_EQ(tracker->read_in_info(), nullptr);
  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, Combiners) {
  // Because of implementation details of each, we notice output of CombineCss
  // but not of CombineJavascript (as it create a new <script> we don't see).
  // This is not thought to be a problem since we'll near certainly be turning
  // off Combine for H2 anyway.
  options()->EnableFilter(RewriteOptions::kCombineCss);
  options()->EnableFilter(RewriteOptions::kCombineJavascript);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<link rel=stylesheet href=c.css>"
                        "<script src=b.js></script>"
                        "<script src=d.js></script>";
  const char kOutput[] =
      "<link rel=stylesheet href=a.css+c.css.pagespeed.cc.0.css>"
      "<script src=\"b.js+d.js.pagespeed.jc.0.js\"></script>"
      "<script>eval(mod_pagespeed_0);</script>"
      "<script>eval(mod_pagespeed_0);</script>";
  ValidateExpected("combiners", kInput, kOutput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  ASSERT_EQ(1, tracker->read_in_info()->dependency_size());
  EXPECT_EQ(StrCat(kTestDomain, "a.css+c.css.pagespeed.cc.0.css"),
            tracker->read_in_info()->dependency(0).url());
  EXPECT_EQ(DEP_CSS, tracker->read_in_info()->dependency(0).content_type());
  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, Chain) {
  // With lots of filters involved. (Not turning on combine JS here since
  // it would make us lose track of the JS. Ditto for inliners).
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheScripts);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<link rel=stylesheet href=c.css>"
                        "<script src=b.js></script>"
                        "<script src=d.js></script>";
  const char kOutput[] =
      "<link rel=stylesheet href=A.a.css+c.css,Mcc.0.css.pagespeed.cf.0.css>"
      "<script src=b.js.pagespeed.jm.0.js></script>"
      "<script src=d.js.pagespeed.jm.0.js></script>";
  ValidateExpected("combiners", kInput, kOutput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  ASSERT_EQ(3, tracker->read_in_info()->dependency_size());
  EXPECT_EQ(StrCat(kTestDomain, "A.a.css+c.css,Mcc.0.css.pagespeed.cf.0.css"),
            tracker->read_in_info()->dependency(0).url());
  EXPECT_EQ(DEP_CSS, tracker->read_in_info()->dependency(0).content_type());
  EXPECT_EQ(StrCat(kTestDomain, "b.js.pagespeed.jm.0.js"),
            tracker->read_in_info()->dependency(1).url());
  EXPECT_EQ(DEP_JAVASCRIPT,
            tracker->read_in_info()->dependency(1).content_type());
  EXPECT_EQ(StrCat(kTestDomain, "d.js.pagespeed.jm.0.js"),
            tracker->read_in_info()->dependency(2).url());
  EXPECT_EQ(DEP_JAVASCRIPT,
            tracker->read_in_info()->dependency(2).content_type());
  rewrite_driver()->FinishParse();
}

}  // namespace

}  // namespace net_instaweb

