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
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com/";

class PushPreloadFilterTest : public RewriteTestBase {
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

    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  " *  { display: block }", 100);
    SetResponseWithDefaultHeaders("b.js",  kContentTypeJavascript,
                                  " var b  = 42", 200);
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

TEST_F(PushPreloadFilterTest, WeirdTiming) {
  // Event buffering causes us to clear mutable_response_headers()
  // at first flush window even if we haven't yet even delivered StartDocument.
  // At the very least, that shouldn't cause us to crash.
  // TODO(morlovich): Discuss what the API should be with Josh.
  rewrite_driver()->AddFilters();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->Flush();
  rewrite_driver()->FinishParse();
}

TEST_F(PushPreloadFilterTest, BasicOperation) {
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver()->AddFilters();

  const char kInput[] = "<link rel=stylesheet href=a.css>"
                        "<script src=b.js></script>";

  const char kOutput[] =
    "<link rel=stylesheet href=A.a.css.pagespeed.cf.0.css>"
    "<script src=b.js.pagespeed.jm.0.js></script>";

  ValidateExpected("basic_res", kInput, kOutput);

  // Now that we've collected stuff, see if it produces proper headers.
  ResetDriver();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<!doctype html><html>");
  rewrite_driver()->Flush();   // Run filters
  ConstStringStarVector links;

  EXPECT_TRUE(rewrite_driver()->response_headers()->Lookup(
                  HttpAttributes::kLink, &links));
  rewrite_driver()->FinishParse();

  ASSERT_EQ(2, links.size());
  EXPECT_STREQ("</A.a.css.pagespeed.cf.0.css>; rel=\"preload\"; as=style",
               *links[0]);
  EXPECT_STREQ("</b.js.pagespeed.jm.0.js>; rel=\"preload\"; as=script",
               *links[1]);
}

}  // namespace

}  // namespace net_instaweb

