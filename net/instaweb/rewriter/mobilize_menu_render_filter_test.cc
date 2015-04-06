/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/mobilize_menu_render_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/opt/http/mock_property_page.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kPageUrl[] = "http://test.com/page.html";

// Much simplified version of kActualMenu1 the same in MobilizeMenuFilterTest
const char kContent[] =
    "<nav data-mobile-role=navigational>"
    "<ul>"
    " <li><a href='/submenu1'>Submenu1</a>"
    "  <ul>"
    "   <li><a href='/a'>A</a></li>"
    "   <li><a href='/b'>B</a><li>"
    "   <li><a href='/c'>C</a></li>"
    "  </ul>"
    " </li>"
    " <li><a href='/submenu2'>Submenu2</a>"
    "  <ul>"
    "   <li><a href='/d'>D</a></li>"
    "   <li><a href='/e'>E</a></li>"
    "   <li><a href='/f'>F</a></li>"
    "  </ul>"
    " </li>"
    "</ul>"
    "</nav>";

class MobilizeMenuRenderFilterTest : public RewriteTestBase {
 protected:
  MobilizeMenuRenderFilterTest() : pcache_(NULL), page_(NULL) {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_.reset(new MobilizeMenuRenderFilter(rewrite_driver()));
    options()->ClearSignatureForTesting();
    options()->set_mob_always(true);
    server_context()->ComputeSignature(options());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_.release());

    SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, kContent, 100);

    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(pcache_, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(dom_cohort);
    ClearDriverAndSetUpPCache();
  }

  void ClearDriverAndSetUpPCache() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kPageUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
  }

  virtual bool AddHtmlTags() const { return false; }

  scoped_ptr<MobilizeMenuRenderFilter> filter_;
  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(MobilizeMenuRenderFilterTest, BasicOperation) {
  const char kMenu[] =
    "<!--Computed menu:entries {\n"
    "  name: \"Submenu1\"\n"
    "  submenu {\n"
    "    entries {\n"
    "      name: \"A\"\n"
    "      url: \"/a\"\n"
    "    }\n"
    "    entries {\n"
    "      name: \"B\"\n"
    "      url: \"/b\"\n"
    "    }\n"
    "    entries {\n"
    "      name: \"C\"\n"
    "      url: \"/c\"\n"
    "    }\n"
    "    entries {\n"
    "      name: \"Submenu1\"\n"
    "      url: \"/submenu1\"\n"
    "    }\n"
    "  }\n"
    "}\n"
    "entries {\n"
    "  name: \"Submenu2\"\n"
    "  submenu {\n"
    "    entries {\n"
    "      name: \"D\"\n"
    "      url: \"/d\"\n"
    "    }\n    entries {\n"
    "      name: \"E\"\n"
    "      url: \"/e\"\n"
    "    }\n"
    "    entries {\n"
    "      name: \"F\"\n"
    "      url: \"/f\"\n"
    "    }\n"
    "    entries {\n"
    "      name: \"Submenu2\"\n"
    "      url: \"/submenu2\"\n"
    "    }\n"
    "  }\n"
    "}\n"
    "-->";
  // computes.
  ValidateExpected("page", kContent, StrCat(kContent, kMenu));

  // Does the same thing from pcache.
  SetFetchResponse404(kPageUrl);
  ValidateExpected("page", kContent, StrCat(kContent, kMenu));
}

TEST_F(MobilizeMenuRenderFilterTest, HandleFailure) {
  // Note that Done(false) makes computation fail, 404 doesn't.
  SetFetchFailOnUnexpected(false);
  ValidateExpected("not_page", kContent,
                   StrCat(kContent, "<!--No computed menu-->"));
}

}  // namespace

}  // namespace net_instaweb
