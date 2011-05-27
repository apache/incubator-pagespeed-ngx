/* Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

namespace net_instaweb {

class SpeedTest : public ResourceManagerTestBase {
};

// This measures the speed of the HTML parsing & filter dispatch mechanism.
TEST_F(SpeedTest, FilterSpeedTest) {
#ifdef NDEBUG
  // Enables all filters.
  options()->SetRewriteLevel(RewriteOptions::kAllFilters);
  rewrite_driver_.AddFilters();

  GoogleString html;
  for (int i = 0; i < 1000; ++i) {
    html += "<div id='x' class='y'> x y z </div>";
  }

  AprTimer timer;
  int64 start_us = timer.NowUs();

  for (int i = 0; i < 1000; ++i) {
    rewrite_driver_.StartParse("http://example.com/index.html");
    rewrite_driver_.ParseText("<html><head></head><body>");
    rewrite_driver_.Flush();
    rewrite_driver_.ParseText(html);
    rewrite_driver_.Flush();
    rewrite_driver_.ParseText("</body></html>");
    rewrite_driver_.FinishParse();
  }

  int64 end_us = timer.NowUs();
  LOG(INFO) << "1000 3-flush parses took " << (end_us - start_us) << "us";
#else
  LOG(INFO) << "Speed test skipped in debug mode";
#endif
}

}  // namespace net_instaweb
