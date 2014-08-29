/*
 * Copyright 2014 Google Inc.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/mobilize_label_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

const char kTestDataDir[] = "/net/instaweb/rewriter/testdata/";
const char kOriginal[] = "mobilize_test.html";

class MobilizeLabelFilterTest : public RewriteTestBase {
 protected:
  MobilizeLabelFilterTest() {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_.reset(new MobilizeLabelFilter(rewrite_driver()));
    html_parse()->AddFilter(filter_.get());
    Statistics* stats = statistics();
    pages_labeled_ =
        stats->GetVariable(MobilizeLabelFilter::kPagesLabeled);
    pages_role_added_ =
        stats->GetVariable(MobilizeLabelFilter::kPagesRoleAdded);
    navigational_roles_ =
        stats->GetVariable(MobilizeLabelFilter::kNavigationalRoles);
    header_roles_ =
        stats->GetVariable(MobilizeLabelFilter::kHeaderRoles);
    content_roles_ =
        stats->GetVariable(MobilizeLabelFilter::kContentRoles);
    marginal_roles_ =
        stats->GetVariable(MobilizeLabelFilter::kMarginalRoles);
  }

  scoped_ptr<MobilizeLabelFilter> filter_;
  Variable* pages_labeled_;
  Variable* pages_role_added_;
  Variable* navigational_roles_;
  Variable* header_roles_;
  Variable* content_roles_;
  Variable* marginal_roles_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobilizeLabelFilterTest);
};

TEST_F(MobilizeLabelFilterTest, AlreadyLabeled) {
  StdioFileSystem filesystem;
  GoogleString labeled_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginal);
  GoogleString labeled_contents;
  ASSERT_TRUE(filesystem.ReadFile(
      labeled_filename.c_str(), &labeled_contents, message_handler()));
  ValidateNoChanges("already_labeled", labeled_contents);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
}

TEST_F(MobilizeLabelFilterTest, Html5TagsInHead) {
  const char kInputHtml[] =
      "<head>"
      "<menu>Should not be labeled</menu>"
      "<header><h1>Also unlabeled</h1></header>"
      "<article>Still untouched</article>"
      "<footer>Also untouched</footer>"
      "</head>";
  ValidateNoChanges("html5_tags_in_head", kInputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
}

TEST_F(MobilizeLabelFilterTest, Html5TagsInBody) {
  const char kInputHtml[] =
      "<head></head><body>"
      "<nav>Labeled"
      "  <menu>unlabeled</menu></nav>"
      "<menu>Labeled</menu>"
      "<header><h1>Labeled</h1></header>"
      "<div id='body'>"
      "  <main>labeled"
      "    <article><section>unlabeled</section></article></main>"
      "  <article>also labeled</article>"
      "  <section>this too"
      "    <aside>but not this, right?</aside></section>"
      "</div>"
      "<aside>Labeled</aside>"
      "<footer>labeled"
      "  <menu>unlabeled</menu></footer>"
      "</body>";
  const char kOutputHtml[] =
      "<head></head><body>"
      "<nav data-mobile-role=\"navigational\">Labeled"
      "  <menu>unlabeled</menu></nav>"
      "<menu data-mobile-role=\"navigational\">Labeled</menu>"
      "<header data-mobile-role=\"header\"><h1>Labeled</h1></header>"
      "<div id='body'>"
      "  <main data-mobile-role=\"content\">labeled"
      "    <article><section>unlabeled</section></article></main>"
      "  <article data-mobile-role=\"content\">also labeled</article>"
      "  <section data-mobile-role=\"content\">this too"
      "    <aside>but not this, right?</aside></section>"
      "</div>"
      "<aside data-mobile-role=\"marginal\">Labeled</aside>"
      "<footer data-mobile-role=\"marginal\">labeled"
      "  <menu>unlabeled</menu></footer>"
      "</body>";
  ValidateExpected("html5_tags_in_body", kInputHtml, kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(2, navigational_roles_->Get());
  EXPECT_EQ(1, header_roles_->Get());
  EXPECT_EQ(3, content_roles_->Get());
  EXPECT_EQ(2, marginal_roles_->Get());
}

}  // namespace

}  // namespace net_instaweb
