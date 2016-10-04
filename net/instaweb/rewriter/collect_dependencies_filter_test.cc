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

#include "net/instaweb/rewriter/public/collect_dependencies_filter.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_cache_failure.h"
#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/dependency_tracker.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/proto_matcher.h"
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
                                  " *  { display: list-item }", 150);

    SetResponseWithDefaultHeaders("b.js",  kContentTypeJavascript,
                                  " var b  = 42", 200);
    SetResponseWithDefaultHeaders("d.js",  kContentTypeJavascript,
                                  " var d  = 32", 250);
    start_time_ms_ = timer()->NowMs();
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
    rewrite_driver()->PropertyCacheSetupDone();
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
  }

  GoogleString FormatRelTimeSec(int delta_sec) {
    return Integer64ToString(start_time_ms_ + delta_sec * Timer::kSecondMs);
  }

  PropertyCache* pcache_;
  PropertyPage* page_;
  int64 start_time_ms_;
  const int64 kYearSec = Timer::kYearMs / Timer::kSecondMs;
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

  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(
      "dependency {"
        "url: 'http://test.com/A.a.css.pagespeed.cf.0.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(100), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}"
      "dependency {"
        "url: 'http://test.com/b.js.pagespeed.jm.0.js'"
        "content_type: DEP_JAVASCRIPT "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(200), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 1"
      "}")));

  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, MediaTopLevel) {
  SetResponseWithDefaultHeaders("e.css", kContentTypeCss,
                                " *  { display: inline-block }", 400);

  rewrite_driver()->AddFilters();

  const char kInput[] =
      "<link rel=stylesheet href=a.css media=\"screen,print\">"
      "<link rel=stylesheet href=c.css media=print>"
      "<link rel=stylesheet href=e.css media=all>";

  ValidateNoChanges("media_top_level", kInput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(
      "dependency {"
        "url: 'http://test.com/a.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(100), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}"
      "dependency {"
        "url: 'http://test.com/e.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(400), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 1"
      "}")));

  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, HandleEmptyResources) {
  // We expect failures to still be hinted (since the browser will try to fetch
  // them), but we need to be careful not to crash. (We used to).

  // Using RememberFailure seems like the easiest way of getting empty
  // Resource objects.
  http_cache()->RememberFailure(
      StrCat(kTestDomain, "e.css"), rewrite_driver()->CacheFragment(),
      kFetchStatusOtherError, message_handler());

  http_cache()->RememberFailure(
      StrCat(kTestDomain, "f.js"), rewrite_driver()->CacheFragment(),
      kFetchStatusOtherError, message_handler());

  const char kInput[] = "<link rel=stylesheet href=e.css>"
                        "<script src=f.js></script>";
  ValidateNoChanges("unoptimized", kInput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(
      "dependency {"
        "url: 'http://test.com/e.css'"
        "content_type: DEP_CSS "
        "order_key: 0"
      "}"
      "dependency {"
        "url: 'http://test.com/f.js'"
        "content_type: DEP_JAVASCRIPT "
        "order_key: 1"
      "}"));

  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, Unoptimized) {
  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<script src=b.js></script>";
  ValidateNoChanges("unoptimized", kInput);

  // Read stuff back in from pcache.
  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(
      "dependency {"
        "url: 'http://test.com/a.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(100), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}"
      "dependency {"
        "url: 'http://test.com/b.js'"
        "content_type: DEP_JAVASCRIPT "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(200), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 1"
      "}")));

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
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(
      "dependency {"
        "url: 'http://test.com/a.css+c.css.pagespeed.cc.0.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(100), " "  // a.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(150), " "  // c.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // a + c
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}")));
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
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(StrCat(
      "dependency {"
        "url: 'http://test.com/A.a.css+c.css,Mcc.0.css.pagespeed.cf.0.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(100), " "  // a.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(150), " "  // c.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // a + c
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // (a + c).cf
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}"),
      "dependency {"
        "url: 'http://test.com/b.js.pagespeed.jm.0.js'"
        "content_type: DEP_JAVASCRIPT "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(200), " "  // b.js
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // b.js.jm
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 2"
      "}"
      "dependency {"
        "url: 'http://test.com/d.js.pagespeed.jm.0.js'"
        "content_type: DEP_JAVASCRIPT "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(250), " "  // d.js
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // d.js.jm
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 3"
      "}")));
  rewrite_driver()->FinishParse();
}

TEST_F(CollectDependenciesFilterTest, Indirect) {
  // Test that we collect indirect dependencies.
  SetResponseWithDefaultHeaders("d.css", kContentTypeCss,
                                "@import \"i1.css\" all;\n"
                                "@import \"i2.css\" print, screen;\n"
                                "@import \"i3.css\" print;         ", 300);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=d.css>";
  const char kOutput[] =
    "<link rel=stylesheet href=A.d.css.pagespeed.cf.0.css>";

  ValidateExpected("basic_res", kInput, kOutput);

  ResetDriver();
  DependencyTracker* tracker = rewrite_driver()->dependency_tracker();
  rewrite_driver()->StartParse(kTestDomain);
  ASSERT_TRUE(tracker->read_in_info() != nullptr);
  EXPECT_THAT(*tracker->read_in_info(), EqualsProto(StrCat(StrCat(
      "dependency {"
        "url: 'http://test.com/A.d.css.pagespeed.cf.0.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(300), " "  // d.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // d.cf
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0"
      "}"),
      "dependency {"
        "url: 'http://test.com/i1.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(300), " "  // d.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // d.cf
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0 "
        "order_key: 1"
      "}"
      "dependency {"
        "url: 'http://test.com/i2.css'"
        "content_type: DEP_CSS "
        "validity_info {"
            "type: CACHED "
            "expiration_time_ms: ", FormatRelTimeSec(300), " "  // d.css
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "validity_info {"
            "type: CACHED "
            "last_modified_time_ms: ", FormatRelTimeSec(0), " ",  // d.cf
            "expiration_time_ms: ", FormatRelTimeSec(kYearSec), " "
            "date_ms: ", FormatRelTimeSec(0),
        "}"
        "order_key: 0 "
        "order_key: 2"
      "}")));
  rewrite_driver()->FinishParse();
}

}  // namespace

}  // namespace net_instaweb
