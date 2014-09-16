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

#include <cstddef>

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
const char kOriginalHtml5[] = "mobilize_test_html5.html";

// Erase shortest substrings in string bracketed by left and right.
// ("[", "]", "abc[def]g[h]i]j[k") -> "abcgi]j[k"
int GlobalEraseBracketedSubstring(StringPiece left, StringPiece right,
                                  GoogleString* string) {
  int deletions = 0;
  size_t left_pos = 0;
  while ((left_pos = string->find(left.data(), left_pos, left.size())) !=
         GoogleString::npos) {
    size_t right_pos =
        string->find(right.data(), left_pos + left.size(), right.size());
    if (right_pos == GoogleString::npos) {
      break;
    }
    right_pos += right.size();
    string->erase(left_pos, right_pos - left_pos);
    ++deletions;
  }
  return deletions;
}

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

  // Remove data-mobile-role labeling from a labeled document
  GoogleString Unlabel(StringPiece labeled) {
    GoogleString result;
    labeled.CopyToString(&result);
    GlobalEraseBracketedSubstring(" data-mobile-role=\"", "\"", &result);
    GlobalEraseBracketedSubstring("<!--ElementTagDepth: ", "-->", &result);
    GlobalEraseBracketedSubstring("<!--role: ", "-->", &result);
    return result;
  }

  // Remove percentages and previous content bytes, which are very
  // input-sensitive, from output buffer so that we just check raw statistics
  // counts.
  void RemoveRedundantDataFromOutputBuffer() {
    GlobalEraseBracketedSubstring(
        "PreviousTagPercent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "PreviousContentBytes:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "PreviousContentPercent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "PreviousNonBlankBytes:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "PreviousNonBlankPercent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "ContainedTagPercent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "ContainedContentPercent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring(
        "ContainedNonBlankPercent", ", ", &output_buffer_);
    GlobalReplaceSubstring("-->", ", -->", &output_buffer_);
    GlobalEraseBracketedSubstring("article percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("aside percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("div percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("footer percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("h1 percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("header percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("main percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("menu percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("nav percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("section percent:", ", ", &output_buffer_);
    GlobalReplaceSubstring(", -->", "-->", &output_buffer_);
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

TEST_F(MobilizeLabelFilterTest, EraseBracketedSubstring) {
  GoogleString test0("abc[def]g[h]i]j[k");
  EXPECT_EQ(2, GlobalEraseBracketedSubstring("[", "]", &test0));
  EXPECT_STREQ("abcgi]j[k", test0);
  GoogleString test1("abc/*ignored*/def/*also*/ghi");
  EXPECT_EQ(2, GlobalEraseBracketedSubstring("/*", "*/", &test1));
  EXPECT_STREQ("abcdefghi", test1);
  GoogleString test2("abc/*ignored*/def/*ghi");
  EXPECT_EQ(1, GlobalEraseBracketedSubstring("/*", "*/", &test2));
  EXPECT_STREQ("abcdef/*ghi", test2);
  GoogleString test3("abc/*ignored*/def*/ghi");
  EXPECT_EQ(1, GlobalEraseBracketedSubstring("/*", "*/", &test3));
  EXPECT_STREQ("abcdef*/ghi", test3);
  GoogleString test4("abc/*ignored/*nested*/def*/ghi");
  EXPECT_EQ(1, GlobalEraseBracketedSubstring("/*", "*/", &test4));
  EXPECT_STREQ("abcdef*/ghi", test4);
}

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
  EnableDebug();
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

TEST_F(MobilizeLabelFilterTest, TinyCount) {
  EnableDebug();
  const char kOutputHtml[] =
      "<div role='content'>Hello there,"
      " <a href='http://theworld.com/'>World</a></div>"
      "<!--ElementTagDepth: 1,"
      " ContainedTagDepth: 2,"       // <a> tag
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"       // Includes <div> itself.
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 17,"  // Whitespace before <a> ignored.
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 16,"
      " ContainedNonBlankPercent: 100.00,"
      " content: 1,"
      " a count: 1,"
      " a percent: 100.00,"
      " div count: 1,"
      " div percent: 100.00-->\n";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
}

TEST_F(MobilizeLabelFilterTest, SmallCountNav) {
  EnableDebug();
  const char kOutputHtml[] =
      "<head></head><body>\n"
      "<div class='container'>\n"
      " <a href='a'>a</a>\n"
      " <div class='menu' id='hdr' role='nav'><ul>\n"
      "  <li><a href='n1'>nav 1</a></li>\n"
      "  <li><a href='n2'>nav 2</a></li>\n"
      "  <li><a href='n3'>nav 3</a></li>\n"
      " </ul></div>"
      "<!--ElementTagDepth: 2,"
      " PreviousTagCount: 2,"
      " PreviousTagPercent: 20.00,"
      " PreviousContentBytes: 1,"
      " PreviousContentPercent: 6.25,"
      " PreviousNonBlankBytes: 1,"
      " PreviousNonBlankPercent: 7.69,"
      " ContainedTagDepth: 3,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 8,"
      " ContainedTagPercent: 80.00,"
      " ContainedContentBytes: 15,"
      " ContainedContentPercent: 93.75,"
      " ContainedNonBlankBytes: 12,"
      " ContainedNonBlankPercent: 92.31,"
      " hdr: 1,"
      " menu: 1,"
      " nav: 1,"
      " a count: 3,"
      " a percent: 75.00,"
      " div count: 1,"
      " div percent: 50.00-->\n"
      "</div>"
      "<!--ElementTagDepth: 1,"
      " ContainedTagDepth: 3,"
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 10,"
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 16,"
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 13,"
      " ContainedNonBlankPercent: 100.00,"
      " a count: 4,"
      " a percent: 100.00,"
      " div count: 2,"
      " div percent: 100.00-->\n"
      "</body>";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
}

TEST_F(MobilizeLabelFilterTest, Html5TagsInBody) {
  EnableDebug();
  // Just for clarity we include the labeled HTML without the sample comments
  // emitted by debug.  The input HTML is this with the data-mobile-role
  // annotations stripped out.
  const char kLabeledHtml[] =
      "<head></head><body>\n"
      "<nav data-mobile-role=\"navigational\">Labeled\n"
      "  <menu>unlabeled</menu>\n"
      "</nav>\n"
      "<menu data-mobile-role=\"navigational\">Labeled</menu>\n"
      "<header data-mobile-role=\"header\"><h1>Labeled</h1></header>\n"
      "<div id='body'>\n"
      "  <main data-mobile-role=\"content\">labeled\n"
      "    <article><section>unlabeled</section>\n"
      "    </article>\n"
      "  </main>\n"
      "  <article data-mobile-role=\"content\">also labeled</article>\n"
      "  <section data-mobile-role=\"content\">this too\n"
      "    <aside data-mobile-role=\"marginal\">and this, it differs.</aside>\n"
      "  </section>\n"
      "</div>\n"
      "<aside data-mobile-role=\"marginal\">Labeled</aside>\n"
      "<footer data-mobile-role=\"marginal\">labeled\n"
      "  <menu data-mobile-role=\"navigational\">navvy</menu>\n"
      "</footer>\n"
      "</body>";
  const char kOutputHtml[] =
      "<head></head><body>\n"
      "<nav data-mobile-role=\"navigational\">Labeled\n"
      "  <menu>unlabeled</menu>"
      "<!--ElementTagDepth: 2,"
      " PreviousTagCount: 1,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 9,"
      " ContainedNonBlankBytes: 9,"
      " menu count: 1-->\n"
      "</nav>"
      "<!--role: navigational,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 16,"
      " ContainedNonBlankBytes: 16,"
      " menu count: 1,"
      " nav count: 1-->\n"
      "<menu data-mobile-role=\"navigational\">Labeled</menu>"
      "<!--role: navigational,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 2,"
      " ContainedTagDepth: 1,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " menu count: 1-->\n"
      "<header data-mobile-role=\"header\"><h1>Labeled</h1></header>"
      "<!--role: header,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 3,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " h1 count: 1,"
      " header count: 1-->\n"
      "<div id='body'>\n"
      "  <main data-mobile-role=\"content\">labeled\n"
      "    <article><section>unlabeled</section>"
      "<!--ElementTagDepth: 4,"
      " PreviousTagCount: 8,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 9,"
      " ContainedNonBlankBytes: 9,"
      " section count: 1-->\n"
      "    </article>"
      "<!--ElementTagDepth: 3,"
      " PreviousTagCount: 7,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 9,"
      " ContainedNonBlankBytes: 9,"
      " article count: 1,"
      " section count: 1-->\n"
      "  </main>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 6,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 3,"
      " ContainedContentBytes: 16,"
      " ContainedNonBlankBytes: 16,"
      " article count: 1,"
      " main count: 1,"
      " section count: 1-->\n"
      "  <article data-mobile-role=\"content\">also labeled</article>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 9,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 12,"
      " ContainedNonBlankBytes: 11,"
      " article count: 1-->\n"
      "  <section data-mobile-role=\"content\">this too\n"
      "    <aside data-mobile-role=\"marginal\">and this, it differs.</aside>"
      "<!--role: marginal,"
      " ElementTagDepth: 3,"
      " PreviousTagCount: 11,"
      " ContainedTagDepth: 3,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 21,"
      " ContainedNonBlankBytes: 18,"
      " aside count: 1-->\n"
      "  </section>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 10,"
      " ContainedTagDepth: 3,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 29,"
      " ContainedNonBlankBytes: 25,"
      " aside count: 1,"
      " section count: 1-->\n"
      "</div>"
      "<!--ElementTagDepth: 1,"
      " PreviousTagCount: 5,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 3,"
      " ContainedTagCount: 7,"
      " ContainedContentBytes: 57,"
      " ContainedNonBlankBytes: 52,"
      " body: 1,"
      " article count: 2,"
      " aside count: 1,"
      " div count: 1,"
      " main count: 1,"
      " section count: 2-->\n"
      "<aside data-mobile-role=\"marginal\">Labeled</aside>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 12,"
      " ContainedTagDepth: 1,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " aside count: 1-->\n"
      "<footer data-mobile-role=\"marginal\">labeled\n"
      "  <menu data-mobile-role=\"navigational\">navvy</menu>"
      "<!--role: navigational,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 14,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 5,"
      " ContainedNonBlankBytes: 5,"
      " menu count: 1-->\n"
      "</footer>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 13,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 12,"
      " ContainedNonBlankBytes: 12,"
      " footer count: 1,"
      " menu count: 1-->\n"
      "</body>";
  Parse("html5_tags_in_body", Unlabel(kLabeledHtml));
  GoogleString xbody = StrCat(doctype_string_, AddHtmlBody(kOutputHtml));
  RemoveRedundantDataFromOutputBuffer();
  EXPECT_STREQ(xbody, output_buffer_) << "html5_tags_in_body";
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(3, navigational_roles_->Get());
  EXPECT_EQ(1, header_roles_->Get());
  EXPECT_EQ(3, content_roles_->Get());
  EXPECT_EQ(3, marginal_roles_->Get());
}

}  // namespace

}  // namespace net_instaweb
