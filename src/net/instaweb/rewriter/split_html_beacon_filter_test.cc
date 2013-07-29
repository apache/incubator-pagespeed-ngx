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

// Author: jud@google.com (Jud Porter)

#include "net/instaweb/rewriter/public/split_html_beacon_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

class SplitHtmlBeaconFilterTest : public RewriteTestBase {
 protected:
  SplitHtmlBeaconFilterTest() {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
    SplitHtmlBeaconFilter::InitStats(statistics());
    // Enabling the filter and options that will turn on beacon injection.
    factory()->set_use_beacon_results_in_filters(true);
    options()->EnableFilter(RewriteOptions::kSplitHtml);
    rewrite_driver()->AddFilters();
  }

  GoogleString BeaconScript() {
    GoogleString script =
        StrCat("<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
               server_context()->static_asset_manager()->GetAsset(
                   StaticAssetManager::kSplitHtmlBeaconJs, options()));
    StrAppend(&script, "\npagespeed.splitHtmlBeaconInit('",
              options()->beacon_url().http, "', '", kTestDomain, "', '0', '",
              ExpectedNonce(), "');");
    StrAppend(&script, "</script>");
    return script;
  }

  GoogleString ExpectedNonce() {
    // TODO(jud): Return an actual nonce here once the beacon has nonces
    // implemented.
    return "";
  }
};

TEST_F(SplitHtmlBeaconFilterTest, ScriptInjection) {
  ValidateExpectedUrl(kTestDomain, "<head></head><body></body>",
                      StrCat("<head></head><body>", BeaconScript(), "</body>"));
}

}  // namespace net_instaweb
