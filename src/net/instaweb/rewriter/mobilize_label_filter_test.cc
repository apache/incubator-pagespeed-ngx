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
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

namespace {

const char kTestDataDir[] = "/net/instaweb/rewriter/testdata/";
const char kOriginal[] = "mobilize_test.html";
const char kOriginalHtml5[] = "mobilize_test_html5.html";
const char kOriginalLabeled[] = "mobilize_test_labeled.html";
const char kOriginalHtml5Labeled[] = "mobilize_test_html5_labeled.html";

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
    divs_unlabeled_ =
        stats->GetVariable(MobilizeLabelFilter::kDivsUnlabeled);
    ambiguous_role_labels_ =
        stats->GetVariable(MobilizeLabelFilter::kAmbiguousRoleLabels);
  }

  void EnableVerbose() {
    options()->set_log_mobilization_samples(true);
    EnableDebug();
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
    GlobalEraseBracketedSubstring("div percent:", ", ", &output_buffer_);
    GlobalEraseBracketedSubstring("h1 percent:", ", ", &output_buffer_);
    GlobalReplaceSubstring(", -->", "-->", &output_buffer_);
  }

  scoped_ptr<MobilizeLabelFilter> filter_;
  Variable* pages_labeled_;
  Variable* pages_role_added_;
  Variable* navigational_roles_;
  Variable* header_roles_;
  Variable* content_roles_;
  Variable* marginal_roles_;
  Variable* divs_unlabeled_;
  Variable* ambiguous_role_labels_;

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
  GoogleString html5_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginalHtml5);
  GoogleString html5_contents;
  ASSERT_TRUE(filesystem.ReadFile(
      html5_filename.c_str(), &html5_contents, message_handler()));
  // Classify using only tag names.  Shouldn't change anything.
  filter_->set_labeling_mode(MobilizeLabelFilter::kUseTagNames);
  ValidateNoChanges("already_labeled", html5_contents);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
  // Classify fully, compare against gold labeling.
  // Note that changes are fairly minimal.
  filter_->set_labeling_mode(MobilizeLabelFilter::kUseTagNamesAndClassifier);
  GoogleString labeled_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginalHtml5Labeled);
  GoogleString labeled_contents;
  ASSERT_TRUE(filesystem.ReadFile(
      labeled_filename.c_str(), &labeled_contents, message_handler()));
  ValidateExpected("already_labeled_adding_labels",
                   html5_contents, labeled_contents);
  EXPECT_EQ(2, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(0, navigational_roles_->Get());
  EXPECT_EQ(1, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(0, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(25, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, Html5TagsInHead) {
  EnableVerbose();
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
  EnableVerbose();
  const char kOutputHtml[] =
      "<div role='content' data-mobile-role=\"marginal\">Hello there,"
      " <a href='http://theworld.com/'>World</a></div>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 2,"       // <a> tag
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"       // Includes <div> itself.
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 17,"  // Whitespace before <a> ignored.
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 16,"
      " ContainedNonBlankPercent: 100.00,"
      " ContainedAContentBytes: 5,"
      " ContainedAContentLocalPercent: 29.41,"
      " ContainedNonAContentBytes: 12,"
      " content: 1,"
      " a count: 1,"
      " a percent: 100.00,"
      " div count: 1,"
      " div percent: 100.00-->\n";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(0, navigational_roles_->Get());
  EXPECT_EQ(0, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(1, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(0, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, TinyCountNbsp) {
  EnableVerbose();
  const char kOutputHtml[] =
      "<div role='content' data-mobile-role=\"marginal\">"
      "  &nbsp;Hello&nbsp;there,&nbsp;&nbsp;  "
      " <a href='http://theworld.com/'>World</a></div>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 2,"       // <a> tag
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"       // Includes <div> itself.
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 17,"  // Whitespace before <a> ignored.
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 16,"
      " ContainedNonBlankPercent: 100.00,"
      " ContainedAContentBytes: 5,"
      " ContainedAContentLocalPercent: 29.41,"
      " ContainedNonAContentBytes: 12,"
      " content: 1,"
      " a count: 1,"
      " a percent: 100.00,"
      " div count: 1,"
      " div percent: 100.00-->\n";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(0, navigational_roles_->Get());
  EXPECT_EQ(0, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(1, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(0, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, ImgInsideAndOutsideA) {
  EnableVerbose();
  const char kOutputHtml[] =
      "<div role='content' data-mobile-role=\"marginal\">"
      " <img src='a.png'>"
      " <img src='b.jpg'>"
      " <a href='http://theworld.com/'><img src='world.gif'></a></div>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 3,"       // <a><img></a>
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 5,"       // Includes <div> itself.
      " ContainedTagPercent: 100.00,"
      " ContainedAImgTag: 1,"
      " ContainedAImgLocalPercent: 33.33,"
      " ContainedNonAImgTag: 2,"
      " content: 1,"
      " a count: 1,"
      " a percent: 100.00,"
      " div count: 1,"
      " div percent: 100.00,"
      " img count: 3,"
      " img percent: 100.00-->\n";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(0, navigational_roles_->Get());
  EXPECT_EQ(0, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(1, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(0, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, DontCrashWithFlush) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<html><head></head><body>\n"
      "<div role='nav'><a href='http://theworld.com/'>\n"
      "Hello, World\n"
      "</a></div>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</body></html>");
  rewrite_driver()->FinishParse();
}

TEST_F(MobilizeLabelFilterTest, DontCrashWithFlushAndDebug) {
  options()->EnableFilter(RewriteOptions::kDebug);
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<html><head></head><body>\n"
      "<div role='nav'><a href='http://theworld.com/'>\n"
      "Hello, World\n"
      "</a></div>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</body></html>");
  rewrite_driver()->FinishParse();
}

TEST_F(MobilizeLabelFilterTest, MarginalPropagation) {
  // Test that marginal content gets labeled as such, and the
  // labels get propagated up the DOM (but only as far as the
  // outermost parent that isn't otherwise labeled).
  const char kOutputHtml[] =
      "<div>\n"
      " <div data-mobile-role='header'>header</div>\n"
      " <div data-mobile-role=\"marginal\">\n"
      "  <div role='footer'>footer</div>\n"
      "  <div role='junk'>junk</div>\n"
      "  <div>more junk</div>\n"
      " </div>\n"
      "</div>";
  ValidateExpected("Marginal propagation",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(0, navigational_roles_->Get());
  EXPECT_EQ(0, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(1, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(4, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, SmallCountNav) {
  EnableVerbose();
  const char kOutputHtml[] =
      "<head></head><body>\n"
      "<div class='container'>\n"
      " <a href='a'>a</a>\n"
      " <div class='menu' id='hdr' role='nav'"
      " data-mobile-role=\"navigational\"><ul>\n"
      "  <li><a href='n1'>nav 1</a></li>\n"
      "  <li><a href='n2'>nav 2</a></li>\n"
      "  <li><a href='n3'>nav 3</a></li>\n"
      " </ul>"
      "<!--ElementTagDepth: 3,"
      " PreviousTagCount: 3,"
      " PreviousTagPercent: 30.00,"
      " PreviousContentBytes: 1,"
      " PreviousContentPercent: 6.25,"
      " PreviousNonBlankBytes: 1,"
      " PreviousNonBlankPercent: 7.69,"
      " ContainedTagDepth: 5,"
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 7,"
      " ContainedTagPercent: 70.00,"
      " ContainedContentBytes: 15,"
      " ContainedContentPercent: 93.75,"
      " ContainedNonBlankBytes: 12,"
      " ContainedNonBlankPercent: 92.31,"
      " ContainedAContentBytes: 15,"
      " ContainedAContentLocalPercent: 100.00,"
      " a count: 3,"
      " a percent: 75.00,"
      " li count: 3,"
      " li percent: 100.00,"
      " ul count: 1,"
      " ul percent: 100.00-->\n"
      " </div>"
      "<!--role: navigational,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 2,"
      " PreviousTagPercent: 20.00,"
      " PreviousContentBytes: 1,"
      " PreviousContentPercent: 6.25,"
      " PreviousNonBlankBytes: 1,"
      " PreviousNonBlankPercent: 7.69,"
      " ContainedTagDepth: 5,"
      " ContainedTagRelativeDepth: 3,"
      " ContainedTagCount: 8,"
      " ContainedTagPercent: 80.00,"
      " ContainedContentBytes: 15,"
      " ContainedContentPercent: 93.75,"
      " ContainedNonBlankBytes: 12,"
      " ContainedNonBlankPercent: 92.31,"
      " ContainedAContentBytes: 15,"
      " ContainedAContentLocalPercent: 100.00,"
      " hdr: 1,"
      " menu: 1,"
      " nav: 1,"
      " a count: 3,"
      " a percent: 75.00,"
      " div count: 1,"
      " div percent: 50.00,"
      " li count: 3,"
      " li percent: 100.00,"
      " ul count: 1,"
      " ul percent: 100.00-->\n"
      "</div>"
      "<!--ElementTagDepth: 1,"
      " ContainedTagDepth: 5,"
      " ContainedTagRelativeDepth: 4,"
      " ContainedTagCount: 10,"
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 16,"
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 13,"
      " ContainedNonBlankPercent: 100.00,"
      " ContainedAContentBytes: 16,"
      " ContainedAContentLocalPercent: 100.00,"
      " a count: 4,"
      " a percent: 100.00,"
      " div count: 2,"
      " div percent: 100.00,"
      " li count: 3,"
      " li percent: 100.00,"
      " ul count: 1,"
      " ul percent: 100.00-->\n"
      "</body>";
  ValidateExpected("Small count nav",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(1, navigational_roles_->Get());
  EXPECT_EQ(0, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(0, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(2, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, NavInsideHeader) {
  // A common pattern in sites is to have a header area with a logo and some
  // navigational content.  We'd like to flag the navigational content!
  EnableVerbose();
  const char kOutputHtml[] =
      "<head></head><body>\n"
      " <header data-mobile-role=\"header\">\n"
      "  <img src='logo.gif'>\n"
      "  <ul id='nav_menu' data-mobile-role=\"navigational\">\n"
      "   <li><a href='about.html'>About us</a>\n"
      "   <li><a href='contact.html'>Contact</a>\n"
      "   <li><a href='faq.html'>FAQ</a>\n"
      "  </ul>"
      "<!--role: navigational,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 2,"
      " PreviousTagPercent: 22.22,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 7,"
      " ContainedTagPercent: 77.78,"
      " ContainedContentBytes: 18,"
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 17,"
      " ContainedNonBlankPercent: 100.00,"
      " ContainedAContentBytes: 18,"
      " ContainedAContentLocalPercent: 100.00,"
      " menu: 1,"
      " nav: 1,"
      " a count: 3,"
      " a percent: 100.00,"
      " li count: 3,"
      " li percent: 100.00,"
      " ul count: 1,"
      " ul percent: 100.00-->\n"
      " </header>"
      "<!--role: header,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 3,"
      " ContainedTagCount: 9,"
      " ContainedTagPercent: 100.00,"
      " ContainedContentBytes: 18,"
      " ContainedContentPercent: 100.00,"
      " ContainedNonBlankBytes: 17,"
      " ContainedNonBlankPercent: 100.00,"
      " ContainedAContentBytes: 18,"
      " ContainedAContentLocalPercent: 100.00,"
      " ContainedNonAImgTag: 1,"
      " a count: 3,"
      " a percent: 100.00,"
      " div count: 1,"
      " div percent: 100.00,"
      " img count: 1,"
      " img percent: 100.00,"
      " li count: 3,"
      " li percent: 100.00,"
      " ul count: 1,"
      " ul percent: 100.00-->\n"
      "</body>";
  ValidateExpected("Nav inside header",
                   Unlabel(kOutputHtml), kOutputHtml);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(1, navigational_roles_->Get());
  EXPECT_EQ(1, header_roles_->Get());
  EXPECT_EQ(0, content_roles_->Get());
  EXPECT_EQ(0, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(0, divs_unlabeled_->Get());
}

TEST_F(MobilizeLabelFilterTest, Html5TagsInBody) {
  EnableVerbose();
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
  // Note how the HTML5 tags used for training / instant classification are
  // treated as divs in the instrumented data.
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
      " ContainedNonAContentBytes: 9,"
      " div count: 1-->\n"
      "</nav>"
      "<!--role: navigational,"
      " ElementTagDepth: 1,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 16,"
      " ContainedNonBlankBytes: 16,"
      " ContainedNonAContentBytes: 16,"
      " div count: 2-->\n"
      "<menu data-mobile-role=\"navigational\">Labeled</menu>"
      "<!--role: navigational,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 2,"
      " ContainedTagDepth: 1,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " ContainedNonAContentBytes: 7,"
      " div count: 1-->\n"
      "<header data-mobile-role=\"header\"><h1>Labeled</h1></header>"
      "<!--role: header,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 3,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " ContainedNonAContentBytes: 7,"
      " div count: 1,"
      " h1 count: 1-->\n"
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
      " ContainedNonAContentBytes: 9,"
      " div count: 1-->\n"
      "    </article>"
      "<!--ElementTagDepth: 3,"
      " PreviousTagCount: 7,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 9,"
      " ContainedNonBlankBytes: 9,"
      " ContainedNonAContentBytes: 9,"
      " div count: 2-->\n"
      "  </main>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 6,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 2,"
      " ContainedTagCount: 3,"
      " ContainedContentBytes: 16,"
      " ContainedNonBlankBytes: 16,"
      " ContainedNonAContentBytes: 16,"
      " div count: 3-->\n"
      "  <article data-mobile-role=\"content\">also labeled</article>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 9,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 12,"
      " ContainedNonBlankBytes: 11,"
      " ContainedNonAContentBytes: 12,"
      " div count: 1-->\n"
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
      " ContainedNonAContentBytes: 21,"
      " div count: 1-->\n"
      "  </section>"
      "<!--role: content,"
      " ElementTagDepth: 2,"
      " PreviousTagCount: 10,"
      " ContainedTagDepth: 3,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 29,"
      " ContainedNonBlankBytes: 25,"
      " ContainedNonAContentBytes: 29,"
      " div count: 2-->\n"
      "</div>"
      "<!--ElementTagDepth: 1,"
      " PreviousTagCount: 5,"
      " ContainedTagDepth: 4,"
      " ContainedTagRelativeDepth: 3,"
      " ContainedTagCount: 7,"
      " ContainedContentBytes: 57,"
      " ContainedNonBlankBytes: 52,"
      " ContainedNonAContentBytes: 57,"
      " body: 1,"
      " div count: 7-->\n"
      "<aside data-mobile-role=\"marginal\">Labeled</aside>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 12,"
      " ContainedTagDepth: 1,"
      " ContainedTagRelativeDepth: 0,"
      " ContainedTagCount: 1,"
      " ContainedContentBytes: 7,"
      " ContainedNonBlankBytes: 7,"
      " ContainedNonAContentBytes: 7,"
      " div count: 1-->\n"
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
      " ContainedNonAContentBytes: 5,"
      " div count: 1-->\n"
      "</footer>"
      "<!--role: marginal,"
      " ElementTagDepth: 1,"
      " PreviousTagCount: 13,"
      " ContainedTagDepth: 2,"
      " ContainedTagRelativeDepth: 1,"
      " ContainedTagCount: 2,"
      " ContainedContentBytes: 12,"
      " ContainedNonBlankBytes: 12,"
      " ContainedNonAContentBytes: 12,"
      " div count: 2-->\n"
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

TEST_F(MobilizeLabelFilterTest, LargeUnlabeled) {
  StdioFileSystem filesystem;
  GoogleString original_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginal);
  GoogleString original_contents;
  ASSERT_TRUE(filesystem.ReadFile(
      original_filename.c_str(), &original_contents, message_handler()));
  GoogleString unlabeled_contents = Unlabel(original_contents);
  // Classify using only tag names.  Shouldn't change anything.
  filter_->set_labeling_mode(MobilizeLabelFilter::kUseTagNames);
  ValidateNoChanges("unlabeled", unlabeled_contents);
  EXPECT_EQ(1, pages_labeled_->Get());
  EXPECT_EQ(0, pages_role_added_->Get());
  // Classify fully, compare against gold labeling.
  // Note that we don't necessarily match the labeling of the original!
  filter_->set_labeling_mode(MobilizeLabelFilter::kUseTagNamesAndClassifier);
  GoogleString labeled_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginalLabeled);
  GoogleString labeled_contents;
  ASSERT_TRUE(filesystem.ReadFile(
      labeled_filename.c_str(), &labeled_contents, message_handler()));
  ValidateExpected("unlabeled_adding_labels",
                   unlabeled_contents, labeled_contents);
  EXPECT_EQ(2, pages_labeled_->Get());
  EXPECT_EQ(1, pages_role_added_->Get());
  EXPECT_EQ(2, navigational_roles_->Get());
  EXPECT_EQ(2, header_roles_->Get());
  EXPECT_EQ(1, content_roles_->Get());
  EXPECT_EQ(2, marginal_roles_->Get());
  EXPECT_EQ(0, ambiguous_role_labels_->Get());
  EXPECT_EQ(31, divs_unlabeled_->Get());
}

}  // namespace

}  // namespace net_instaweb
