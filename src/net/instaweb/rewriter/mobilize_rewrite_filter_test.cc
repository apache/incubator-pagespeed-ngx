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

namespace net_instaweb {

namespace {

const char kAddedStyle[] = "stylestring";
const char kTestDataDir[] = "/net/instaweb/rewriter/testdata/";
const char kOriginal[] = "mobilize_test.html";
const char kRewritten[] = "mobilize_test_output.html";

}  // namespace

// Base class for doing our tests. Can access MobilizeRewriteFilter's private
// API. Sets the content of the stylesheet added by the filter to be kAddedStyle
// instead of the default (which is large and will likely change).
class MobilizeRewriteFilterTest : public RewriteTestBase {
 protected:
  MobilizeRewriteFilterTest() {}

  void CheckExpected(const GoogleString& expected) {
    PrepareWrite();
    EXPECT_STREQ(expected, output_buffer_);
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_.reset(new MobilizeRewriteFilter(rewrite_driver()));
    filter_->style_css_ = kAddedStyle;
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
      EXPECT_EQ(var->Get(), value) << name;
    }
  }

  // Wrappers for MobilizeRewriteFilter private API.
  void FilterAddStyleAndViewport(HtmlElement* element) {
    filter_->AddStyleAndViewport(element);
  }
  void FilterAddReorderContainers(HtmlElement* element) {
    filter_->AddReorderContainers(element);
  }
  void FilterRemoveReorderContainers() {
    filter_->RemoveReorderContainers();
  }
  bool FilterIsReorderContainer(HtmlElement* element) {
    return filter_->IsReorderContainer(element);
  }
  HtmlElement* FilterMobileRoleToContainer(MobileRole::Level mobile_role) {
    return filter_->MobileRoleToContainer(mobile_role);
  }
  MobileRole::Level FilterGetMobileRole(HtmlElement* element) {
    return filter_->GetMobileRole(element);
  }

  scoped_ptr<MobilizeRewriteFilter> filter_;

 private:
  void PrepareWrite() {
    SetupWriter();
    html_parse()->ApplyFilter(html_writer_filter_.get());
  }

  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFilterTest);
};

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

TEST_F(MobilizeRewriteUnitTest, AddStyles) {
  HtmlElement* head = html_parse()->NewElement(NULL, HtmlName::kHead);
  html_parse()->InsertNodeBeforeCurrent(head);
  HtmlCharactersNode* content = html_parse()->NewCharactersNode(head, "123");
  html_parse()->AppendChild(head, content);
  CheckExpected("<head>123</head>");
  FilterAddStyleAndViewport(head);
  CheckExpected("<head>123<style>stylestring</style><meta name='viewport'"
                " content='width=device-width,user-scalable=no'/></head>");
}

TEST_F(MobilizeRewriteUnitTest, HandleContainers) {
  HtmlElement* body = html_parse()->NewElement(NULL, HtmlName::kBody);
  html_parse()->InsertNodeBeforeCurrent(body);
  HtmlCharactersNode* content = html_parse()->NewCharactersNode(body, "123");
  html_parse()->AppendChild(body, content);
  CheckExpected("<body>123</body>");
  FilterAddReorderContainers(body);
  CheckExpected("<body>123<div name='keeper'></div>"
                "<div name='header'></div>"
                "<div name='navigational'></div>"
                "<div name='content'></div>"
                "<div name='marginal'></div></body>");
  FilterRemoveReorderContainers();
  CheckExpected("<body>123</body>");
}

TEST_F(MobilizeRewriteUnitTest, CheckGetContainers) {
  HtmlElement* body = html_parse()->NewElement(NULL, HtmlName::kBody);
  html_parse()->InsertNodeBeforeCurrent(body);
  FilterAddReorderContainers(body);
  CheckExpected("<body><div name='keeper'></div>"
                "<div name='header'></div>"
                "<div name='navigational'></div>"
                "<div name='content'></div>"
                "<div name='marginal'></div></body>");
  for (int i = 0; i < MobileRole::kInvalid; i++) {
    MobileRole::Level expected = static_cast<MobileRole::Level>(i);
    const MobileRole* role = &MobileRole::kMobileRoles[i];
    EXPECT_EQ(expected, role->level);
    const char* string = MobileRole::StringFromLevel(expected);
    EXPECT_EQ(string, role->value);
    EXPECT_EQ(expected, MobileRole::LevelFromString(string));
    HtmlElement* container = FilterMobileRoleToContainer(expected);
    EXPECT_FALSE(container == NULL);
    EXPECT_TRUE(FilterIsReorderContainer(container));
  }
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
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFunctionalTest);
};

TEST_F(MobilizeRewriteFunctionalTest, AddStyleAndViewport) {
  ValidateExpected("add_style_and_viewport",
                   "<head></head>",
                   "<head><style>stylestring</style><meta name='viewport'"
                   " content='width=device-width,user-scalable=no'/></head>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, RemoveExistingViewport) {
  ValidateExpected("remove_existing_viewport",
                   "<head><meta name='viewport' content='value' /></head>",
                   "<head><style>stylestring</style><meta name='viewport'"
                   " content='width=device-width,user-scalable=no'/></head>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 1);
}

TEST_F(MobilizeRewriteFunctionalTest, HeadUnmodified) {
  ValidateExpected("head_unmodified",
                   "<head><meta name='keywords' content='cool,stuff'/>"
                   "<style>abcd</style></head>",
                   "<head><meta name='keywords' content='cool,stuff'/>"
                   "<style>abcd</style><style>stylestring</style><meta"
                   " name='viewport' content='width=device-width,"
                   "user-scalable=no'/></head>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, HeadLinksUnmodified) {
  ValidateExpected("head_unmodified",
                   "<head><link rel='stylesheet' type='text/css'"
                   " href='theme.css'></head>",
                   "<head><link rel='stylesheet' type='text/css'"
                   " href='theme.css'><style>stylestring</style>"
                   "<meta name='viewport' content='width=device-width,"
                   "user-scalable=no'/></head>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, EmptyBody) {
  ValidateNoChanges("empty_body",
                    "<body></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, KeeperTagsUnmodified) {
  ValidateNoChanges("map_tags_unmodified",
                    "<body><map name='planetmap'><area shape='rect'"
                    " coords='0,0,82,126' alt='Sun'></map></body>");
  ValidateNoChanges("script_tags_unmodified",
                    "<body><script>document.getElementById('demo')."
                    "innerHTML = 'Hello JavaScript!';</script></body>");
  ValidateNoChanges("style_tags_unmodified",
                    "<body><style>* { foo: bar; }</style></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 3);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 3);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, ReorderElements) {
  ValidateExpected(
      "reorder_elements", "<body><div data-mobile-role='marginal'>"
      "<span>123</span></div><div data-mobile-role='header'>"
      "<h1>foo</h1><p>bar</p></div></body>",
      "<body><div data-mobile-role='header'><h1>foo</h1><p>bar</p>"
      "</div><div data-mobile-role='marginal'><span>123</span></div>"
      "</body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, ReorderElements2) {
  ValidateExpected("reorder_elements_2",
                   "<body>123<div data-mobile-role='content'>890</div>456"
                   "<div data-mobile-role='header'>abc</div>def</body>",
                   "<body><div data-mobile-role='header'>abc</div>"
                   "<div data-mobile-role='content'>890</div></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 3);
}

TEST_F(MobilizeRewriteFunctionalTest, RemoveTables) {
  ValidateExpected(
      "remove_tables",
      "<body><div data-mobile-role='content'><table><tr><td>1</td>"
      "<td>2</td></tr></table></div></body>",
      "<body><div data-mobile-role='content'>12<br><br></div></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 4);
}

TEST_F(MobilizeRewriteFunctionalTest, StripNav) {
  ValidateExpected("strip_nav",
                   "<body><div data-mobile-role='navigational'><div>"
                   "<a href='foo.com'>123</a></div></div></body>",
                   "<body><div data-mobile-role='navigational'>"
                   "<a href='foo.com'>123</a></div></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 1);
}

TEST_F(MobilizeRewriteFunctionalTest, StripOnlyNav) {
  ValidateExpected(
      "strip_only_nav",
      "<body><div data-mobile-role='navigational'><div>"
      "<a href='foo.com'>123</a></div></div>"
      "<div data-mobile-role='header'><h1>foobar</h1></div></body>",
      "<body><div data-mobile-role='header'><h1>foobar</h1></div>"
      "<div data-mobile-role='navigational'><a href='foo.com'>123"
      "</a></div></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 1);
}

TEST_F(MobilizeRewriteFunctionalTest, UnknownMobileRole) {
  // Its probably OK if the behavior resulting from having a weird
  // data-mobile-role value is unexpected, as long as it doesn't crash.
  ValidateExpected(
      "unknown_mobile_role",
      "<body><div data-mobile-role='garbage'><a>123</a></div></body>",
      "<body></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 3);
}

TEST_F(MobilizeRewriteFunctionalTest, MultipleHeads) {
  // Check we only add the style and viewport tag once.
  ValidateExpected("multiple_heads",
                   "<head></head><head></head>",
                   "<head><style>stylestring</style><meta name='viewport'"
                   " content='width=device-width,user-scalable=no'/></head>"
                   "<head></head>");
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
  ValidateNoChanges("multiple_bodys",
                    "<body></body><body></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 0);
}

TEST_F(MobilizeRewriteFunctionalTest, MultipleBodysWithContent) {
  ValidateExpected(
      "multiple_bodys_with_content",
      "<body>123<div data-mobile-role='marginal'>567</div></body>"
      "<body><div data-mobile-role='content'>890</div>"
      "<div data-mobile-role='header'>abc</div></body>",
      "<body><div data-mobile-role='marginal'>567</div></body><body>"
      "<div data-mobile-role='header'>abc</div>"
      "<div data-mobile-role='content'>890</div></body>");
  CheckVariable(MobilizeRewriteFilter::kPagesMobilized, 1);
  CheckVariable(MobilizeRewriteFilter::kKeeperBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kHeaderBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kNavigationalBlocks, 0);
  CheckVariable(MobilizeRewriteFilter::kContentBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kMarginalBlocks, 1);
  CheckVariable(MobilizeRewriteFilter::kDeletedElements, 1);
}

// Check we are called correctly from the driver.
class MobilizeRewriteEndToEndTest : public RewriteTestBase {
 protected:
  MobilizeRewriteEndToEndTest() {}

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
  AddFilter(RewriteOptions::kMobilize);
  ValidateExpected("full_page", original_buffer, rewritten_buffer);
}

}  // namespace net_instaweb
