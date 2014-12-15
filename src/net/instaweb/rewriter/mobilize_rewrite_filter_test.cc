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

// Author: stevensr@google.com (Ryan Stevens)

#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

namespace {

const char kTestDataDir[] = "/net/instaweb/rewriter/testdata/";
const char kOriginal[] = "mobilize_test.html";
const char kRewritten[] = "mobilize_test_output.html";
const char kStyles[] = "<link rel=\"stylesheet\" href=\"mobilize.css\">";
const char kHeadAndViewport[] =
    "<script>var psDebugMode=false;var psNavMode=true;</script>"
    "<meta name='viewport' content='width=device-width'/>"
    "<script src=\"goog/base.js\"></script>"
    "<script src=\"mobilize_xhr.js\"></script>";

}  // namespace

// Base class for doing our tests. Can access MobilizeRewriteFilter's private
// API.
class MobilizeRewriteFilterTest : public RewriteTestBase {
 protected:
  MobilizeRewriteFilterTest() {}

  void CheckExpected(const GoogleString& expected) {
    PrepareWrite();
    EXPECT_STREQ(expected, output_buffer_);
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->ClearSignatureForTesting();
    options()->set_mob_always(true);
    options()->set_mob_layout(true);
    options()->set_mob_logo(true);
    options()->set_mob_nav(true);
    server_context()->ComputeSignature(options());
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>

    filter_.reset(new MobilizeRewriteFilter(rewrite_driver()));
  }

  virtual void TearDown() {
    RewriteTestBase::TearDown();
  }

  virtual bool AddBody() const { return false; }
  virtual bool AddHtmlTags() const { return false; }

  void CheckVariable(const char* name, int value) {
    Variable* var = rewrite_driver()->statistics()->FindVariable(name);
    if (var == NULL) {
      CHECK(false) << "Checked for a variable that doesn't exit.";
    } else {
      EXPECT_EQ(value, var->Get()) << name;
    }
  }

  // Wrappers for MobilizeRewriteFilter private API.
  void FilterAddStyle(HtmlElement* element) {
    filter_->AddStyle(element);
  }
  MobileRole::Level FilterGetMobileRole(HtmlElement* element) {
    return filter_->GetMobileRole(element);
  }
  void FilterSetAddedProgress(bool added) {
    filter_->added_progress_ = added;
  }

  GoogleString ScriptsAtEndOfBody() {
    return
        "<script src=\"mob_logo.js\"></script>"
        "<script src=\"mobilize_util.js\"></script>"
        "<script src=\"mobilize_layout.js\"></script>"
        "<script src=\"mobilize_nav.js\"></script>"
        "<script src=\"mobilize.js\"></script>";
  }

  scoped_ptr<MobilizeRewriteFilter> filter_;

 private:
  void PrepareWrite() {
    SetupWriter();
    html_parse()->ApplyFilter(html_writer_filter_.get());
  }

  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFilterTest);
};

namespace {

// For testing private functions in isolation.
class MobilizeRewriteUnitTest : public MobilizeRewriteFilterTest {
 protected:
  MobilizeRewriteUnitTest() {}

  virtual void SetUp() {
    MobilizeRewriteFilterTest::SetUp();
    static const char kUrl[] = "http://mob.rewrite.test/test.html";
    ASSERT_TRUE(html_parse()->StartParse(kUrl));
  }

  virtual void TearDown() {
    html_parse()->FinishParse();
    MobilizeRewriteFilterTest::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteUnitTest);
};

TEST_F(MobilizeRewriteUnitTest, AddStyle) {
  HtmlElement* head = html_parse()->NewElement(NULL, HtmlName::kHead);
  html_parse()->InsertNodeBeforeCurrent(head);
  HtmlCharactersNode* content = html_parse()->NewCharactersNode(head, "123");
  html_parse()->AppendChild(head, content);
  CheckExpected("<head>123</head>");
  FilterAddStyle(head);
  CheckExpected(StrCat("<head>123", kStyles, "</head>"));
}

TEST_F(MobilizeRewriteUnitTest, MobileRoleAttribute) {
  HtmlElement* div = html_parse()->NewElement(NULL, HtmlName::kDiv);
  html_parse()->AddAttribute(div, "data-mobile-role", "navigational");
  // Add the new node to the parse tree so it will be deleted.
  html_parse()->InsertNodeBeforeCurrent(div);
  EXPECT_EQ(MobileRole::kNavigational,
            FilterGetMobileRole(div));
}

TEST_F(MobilizeRewriteUnitTest, InvalidMobileRoleAttribute) {
  HtmlElement* div = html_parse()->NewElement(NULL, HtmlName::kDiv);
  html_parse()->AddAttribute(div, "data-mobile-role", "garbage");
  // Add the new node to the parse tree so it will be deleted.
  html_parse()->InsertNodeBeforeCurrent(div);
  EXPECT_EQ(MobileRole::kInvalid,
            FilterGetMobileRole(div));
}

TEST_F(MobilizeRewriteUnitTest, KeeperMobileRoleAttribute) {
  HtmlElement* script = html_parse()->NewElement(NULL, HtmlName::kScript);
  // Add the new node to the parse tree so it will be deleted.
  html_parse()->InsertNodeBeforeCurrent(script);
  EXPECT_EQ(MobileRole::kKeeper,
            FilterGetMobileRole(script));
}

class MobilizeRewriteFunctionalTest : public MobilizeRewriteFilterTest {
 protected:
  MobilizeRewriteFunctionalTest() {}

  virtual void SetUp() {
    MobilizeRewriteFilterTest::SetUp();
    html_parse()->AddFilter(filter_.get());
    // By default we *don't* add the progress bar scrim.  This explicitly gets
    // overridden in subclasses.
    FilterSetAddedProgress(true);
  }

  void HeadTest(const char* name,
                StringPiece original_head, StringPiece expected_mid_head,
                int deleted_elements) {
    GoogleString original = StrCat("<head>", original_head, "</head>");
    GoogleString expected =
        StrCat("<head>", kHeadAndViewport, expected_mid_head, kStyles,
               "</head>");
    ValidateExpected(name, original, expected);
    CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
    CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kDeletedElements, deleted_elements);
  }

  void BodyTest(const char* name,
                StringPiece original_body, StringPiece expected_mid_body) {
    // TODO(jmaessen): We should inject a head in these cases, possibly by
    // requiring AddHeadFilter to run.  We should also deal with the complete
    // absence of a body tag.
    GoogleString original =
        StrCat("<body>", original_body, "</body>");
    GoogleString expected =
        StrCat("<body>", expected_mid_body, ScriptsAtEndOfBody(), "</body>");
    ValidateExpected(name, original, expected);
    CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  }

  void BodyUnchanged(const char* name, StringPiece body) {
    BodyTest(name, body, body);
  }

  void KeeperTagsTest(const char* name, GoogleString keeper) {
    BodyUnchanged(name, keeper);
    CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 1);
    CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
    CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
  }

  void TwoBodysTest(const char* name,
                    StringPiece first_body, StringPiece second_body) {
    GoogleString original =
        StrCat("<body>", first_body,
               "</body><body>", second_body, "</body>");
    GoogleString expected = StrCat("<body>", first_body, ScriptsAtEndOfBody(),
                                   "</body><body>", second_body, "</body>");
    ValidateExpected(name, original, expected);
    CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFunctionalTest);
};

TEST_F(MobilizeRewriteFunctionalTest, AddStyleAndViewport) {
  HeadTest("add_style_and_viewport", "", "", 0);
}

TEST_F(MobilizeRewriteFunctionalTest, RemoveExistingViewport) {
  HeadTest("remove_existing_viewport",
           "<meta name='viewport' content='value' />", "", 1);
}

TEST_F(MobilizeRewriteFunctionalTest, RemoveExistingViewportThatMatches) {
  HeadTest("remove_existing_viewport",
           "<meta name='viewport' content='width=device-width'/>", "", 1);
}

TEST_F(MobilizeRewriteFunctionalTest, HeadUnmodified) {
  const char kHeadTags[] =
      "<meta name='keywords' content='cool,stuff'/>"
      "<style>abcd</style>";
  HeadTest("head_unmodified", kHeadTags, kHeadTags, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, HeadLinksUnmodified) {
  const char kLink[] =
      "<link rel='stylesheet' type='text/css' href='theme.css'>";
  HeadTest("head_unmodified", kLink, kLink, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, EmptyBody) {
  GoogleString expected = StrCat("<body>", ScriptsAtEndOfBody(), "</body>");
  ValidateExpected("empty_body",
                   "<body></body>", expected);
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, EmptyBodyWithProgress) {
  FilterSetAddedProgress(false);
  GoogleString expected = StrCat(
      "<body>"
      "<div id=\"ps-progress-scrim\" class=\"psProgressScrim\">"
      "<a href=\"javascript:psRemoveProgressBar();\" id=\"ps-progress-remove\""
      " id=\"ps-progress-show-log\">Remove Progress Bar"
      " (doesn't stop mobilization)</a><br>"
      "<a href=\"javascript:psSetDebugMode();\">"
      "Show Debug Log In Progress Bar</a>"
      "<div class=\"psProgressBar\">"
      "<span id=\"ps-progress-span\" class=\"psProgressSpan\"></span>"
      "</div><pre id=\"ps-progress-log\" class=\"psProgressLog\"/></div>",
      ScriptsAtEndOfBody(), "</body>");
  ValidateExpected("empty_body_with_progress",
                   "<body></body>", expected);
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, MapTagsUnmodified) {
  KeeperTagsTest("map_tags_unmodified",
                 "<map name='planetmap'><area shape='rect'"
                 " coords='0,0,82,126' alt='Sun'></map>");
}

TEST_F(MobilizeRewriteFunctionalTest, ScriptTagsUnmodified) {
  KeeperTagsTest("script_tags_unmodified",
                 "<script>document.getElementById('demo')."
                 "innerHTML = 'Hello JavaScript!';</script>");
}

TEST_F(MobilizeRewriteFunctionalTest, StyleTagsUnmodified) {
  KeeperTagsTest("style_tags_unmodified",
                 "<style>* { foo: bar; }</style>");
}

TEST_F(MobilizeRewriteFunctionalTest, UnknownMobileRole) {
  // Its probably OK if the behavior resulting from having a weird
  // data-mobile-role value is unexpected, as long as it doesn't crash.
  BodyUnchanged(
      "unknown_mobile_role",
      "<div data-mobile-role='garbage'><a>123</a></div>");
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, MultipleHeads) {
  // Check we only add the style and viewport tag once.
  const char kRestOfHeads[] = "</head><head></head>";
  GoogleString original = StrCat("<head>", kRestOfHeads);
  GoogleString expected =
      StrCat("<head>", kHeadAndViewport, kStyles, kRestOfHeads);
  ValidateExpected("multiple_heads", original, expected);
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, MultipleBodys) {
  // Each body should be handled as its own unit.
  TwoBodysTest("multiple_bodys", "", "");
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, MultipleBodysWithContent) {
  TwoBodysTest(
      "multiple_bodys_with_content",
      "123<div data-mobile-role='marginal'>567</div>",
      "<div data-mobile-role='content'>890</div>"
      "<div data-mobile-role='header'>abc</div>");
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, HeaderWithinBody) {
  BodyUnchanged(
      "header_within_body",
      "<div data-mobile-role='content'>123<div data-mobile-role='header'>"
      "456</div>789</div>");
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, HeaderWithinHeader) {
  // Note: this should occur primarily as a result of a nested HTML5 tag, as the
  // labeler should not label children with the parent's label.
  BodyUnchanged(
      "header_within_header",
      "<div data-mobile-role='header'>123<div data-mobile-role='header'>"
      "456</div>789</div>");
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 2);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

// Check we are called correctly from the driver.
class MobilizeRewriteEndToEndTest : public MobilizeRewriteFilterTest {
 protected:
  MobilizeRewriteEndToEndTest() {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
    options()->ClearSignatureForTesting();
    options()->set_mob_layout(true);
    options()->set_mob_logo(true);
    options()->set_mob_nav(true);
    AddFilter(RewriteOptions::kMobilize);
  }

  virtual bool AddBody() const { return false; }
  virtual bool AddHtmlTags() const { return false; }

  StdioFileSystem filesystem_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteEndToEndTest);
};

TEST_F(MobilizeRewriteEndToEndTest, FullPage) {
  // This test will break when the CSS is changed. Update the expected output
  // accordingly.
  GoogleString original_buffer;
  GoogleString original_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginal);
  ASSERT_TRUE(filesystem_.ReadFile(original_filename.c_str(), &original_buffer,
                                   message_handler()));
  GoogleString rewritten_buffer;
  GoogleString rewritten_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kRewritten);
  ASSERT_TRUE(filesystem_.ReadFile(rewritten_filename.c_str(),
                                   &rewritten_buffer, message_handler()));
  GlobalReplaceSubstring("@@HEAD_SCRIPT_LOAD@@", kHeadAndViewport,
                         &rewritten_buffer);
  GlobalReplaceSubstring("@@HEAD_STYLES@@", kStyles, &rewritten_buffer);
  GlobalReplaceSubstring("@@TRAILING_SCRIPT_LOADS@@", ScriptsAtEndOfBody(),
                         &rewritten_buffer);
  rewrite_driver()->SetUserAgent(
      UserAgentMatcherTestBase::kAndroidChrome21UserAgent);
  ValidateExpected("full_page", original_buffer, rewritten_buffer);
}

TEST_F(MobilizeRewriteEndToEndTest, NonMobile) {
  // Don't mobilize on a non-mobile browser.
  GoogleString original_buffer;
  GoogleString original_filename =
      StrCat(GTestSrcDir(), kTestDataDir, kOriginal);
  ASSERT_TRUE(filesystem_.ReadFile(original_filename.c_str(), &original_buffer,
                                   message_handler()));
  // We don't particularly care for the moment if the labeler runs and annotates
  // the page, or if add-ids adds ids.
  GoogleString expected_buffer(original_buffer);
  GlobalEraseBracketedSubstring(" data-mobile-role=\"", "\"", &expected_buffer);
  GlobalEraseBracketedSubstring(" id=\"PageSpeed-", "\"", &expected_buffer);
  rewrite_driver()->SetUserAgent(
      UserAgentMatcherTestBase::kChrome37UserAgent);
  Parse("EndToEndNonMobile", original_buffer);
  GlobalEraseBracketedSubstring(" data-mobile-role=\"", "\"", &output_buffer_);
  GlobalEraseBracketedSubstring(" id=\"PageSpeed-", "\"", &output_buffer_);
  EXPECT_STREQ(expected_buffer, output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
