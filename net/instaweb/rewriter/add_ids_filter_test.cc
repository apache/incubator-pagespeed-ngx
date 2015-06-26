/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jhoch@google.com (Jason R. Hoch)
// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/add_ids_filter.h"

#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

namespace {

class AddIdsFilterTest : public RewriteTestBase {
 protected:
  AddIdsFilterTest()
      : add_ids_filter_(rewrite_driver()) {
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    html_parse()->AddFilter(&add_ids_filter_);
  }

  virtual bool AddBody() const { return false; }

  // Remove PageSpeed- ids
  GoogleString Unlabel(StringPiece labeled) {
    GoogleString result;
    labeled.CopyToString(&result);
    GlobalEraseBracketedSubstring(" id=\"PageSpeed-", "\"", &result);
    return result;
  }

 private:
  AddIdsFilter add_ids_filter_;
};

TEST_F(AddIdsFilterTest, NoDivtest) {
  static const char kExpected[] =
      "<html><body>\n"
      "  <p>Today's top stories are:</p>\n"
      "  <ol>\n"
      "    <li><a href='http://www.example1.com/'>"
               "Website wins award for most boring URL.</a></li>\n"
      "    <li><a href='http://www.example2.com/'>"
               "Copycats quickly try to steal some spotlight.</a></li>\n"
      "    <li><a href='http://www.example3.com/'>Internet "
               "proves itself capable of spawning copycat copycats.</a></li>\n"
      "    <li><a href='http://www.example5.com/'>Embarrassed "
               "imitator ruins trend.</a></li>\n"
      "  </ol>\n"
      "</body></html>\n";
  ValidateNoChanges("no_div_test", kExpected);
}

TEST_F(AddIdsFilterTest, WithDivsTest) {
  static const char kExpected[] =
      "<html><body>\n"
      "  <div id='menu'>\n"
      "   <ul id=\"PageSpeed-menu-0\">\n"
      "    <li><a href='http://www.example.com/home'>"
               "HOME</a></li>\n"
      "    <li><a href='http://www.example.com/contact_us'>"
               "CONTACT US</a></li>\n"
      "    <li><a href='http://www.example.com/about'>"
               "ABOUT</a></li>\n"
      "    <li><div id=\"PageSpeed-menu-0-3-0\">"
               "Share this</div></li>\n"
      "   </ul>\n"
      "  </div>\n"
      "  <div id=\"PageSpeed-1\"></div>\n"
      "  <div id='content'>\n"
      "    <div class='top_story' id=\"PageSpeed-content-0\">\n"
      "      <div id=\"PageSpeed-content-0-0\">TOP STORY</div>\n"
      "    </div>\n"
      "    <div class='stories' id=\"PageSpeed-content-1\">\n"
      "      <div id=\"PageSpeed-content-1-0\">STORY ONE</div>\n"
      "      <div id=\"PageSpeed-content-1-1\">STORY TWO</div>\n"
      "      <div id=\"PageSpeed-content-1-2\">STORY THREE</div>\n"
      "    </div>\n"
      "  </div>\n"
      "</body>\n"
      "<div id=\"Pagespeed-2\">"
      "   Post-BODY content"
      "</div></html>\n";
  ValidateExpected("with_divs_test", Unlabel(kExpected), kExpected);
}

TEST_F(AddIdsFilterTest, BodyHasId) {
  static const char kExpected[] =
      "<html><body id='body'>\n"
      "  <div id='menu'>\n"
      "   <ul id=\"PageSpeed-menu-0\">\n"
      "    <li><a href='http://www.example.com/home'>"
               "HOME</a></li>\n"
      "    <li><a href='http://www.example.com/contact_us'>"
               "CONTACT US</a></li>\n"
      "    <li><a href='http://www.example.com/about'>"
               "ABOUT</a></li>\n"
      "    <li><div id=\"PageSpeed-menu-0-3-0\">"
               "Share this</div></li>\n"
      "   </ul>\n"
      "  </div>\n"
      "  <div id=\"PageSpeed-body-1\"></div>\n"
      "  <div id='content'>\n"
      "    <div class='top_story' id=\"PageSpeed-content-0\">\n"
      "      <div id=\"PageSpeed-content-0-0\">TOP STORY</div>\n"
      "    </div>\n"
      "    <div class='stories' id=\"PageSpeed-content-1\">\n"
      "      <div id=\"PageSpeed-content-1-0\">STORY ONE</div>\n"
      "      <div id=\"PageSpeed-content-1-1\">STORY TWO</div>\n"
      "      <div id=\"PageSpeed-content-1-2\">STORY THREE</div>\n"
      "    </div>\n"
      "  </div>\n"
      "</body>\n"
      "<div id=\"Pagespeed-2\">"
      "   Post-BODY content"
      "</div></html>\n";
  ValidateExpected("body_has_id", Unlabel(kExpected), kExpected);
}

TEST_F(AddIdsFilterTest, TwoDigitDivCountTest) {
  static const char kExpected[] =
      "<html><body>\n"
      "  <menu id='menu'>\n"
      "    <div id=\"PageSpeed-menu-0\">Link 1</div>\n"
      "    <div id=\"PageSpeed-menu-1\">Link 2</div>\n"
      "    <div id=\"PageSpeed-menu-2\">Link 3</div>\n"
      "    <div id=\"PageSpeed-menu-3\">Link 4</div>\n"
      "    <div id=\"PageSpeed-menu-4\">Link 5</div>\n"
      "    <div id=\"PageSpeed-menu-5\">Link 6</div>\n"
      "    <div id=\"PageSpeed-menu-6\">Link 7</div>\n"
      "    <div id=\"PageSpeed-menu-7\">Link 8</div>\n"
      "    <div id=\"PageSpeed-menu-8\">Link 9</div>\n"
      "    <div id=\"PageSpeed-menu-9\">Link 10</div>\n"
      "    <div id=\"PageSpeed-menu-10\">Submenu 11\n"
      "      <div id=\"PageSpeed-menu-10-0\">Nested 0</div>\n"
      "      <div id=\"PageSpeed-menu-10-1\">Nested 1</div>\n"
      "      <div id=\"PageSpeed-menu-10-2\">Nested 2</div>\n"
      "      <div id=\"PageSpeed-menu-10-3\">Nested 3</div>\n"
      "      <div id=\"PageSpeed-menu-10-4\">Nested 4</div>\n"
      "      <div id=\"PageSpeed-menu-10-5\">Nested 5</div>\n"
      "      <div id=\"PageSpeed-menu-10-6\">Nested 6</div>\n"
      "      <div id=\"PageSpeed-menu-10-7\">Nested 7</div>\n"
      "      <div id=\"PageSpeed-menu-10-8\">Nested 8</div>\n"
      "      <div id=\"PageSpeed-menu-10-9\">Nested 9</div>\n"
      "      <div id=\"PageSpeed-menu-10-10\">Nested 10</div>\n"
      "    </div>\n"
      "  </menu>\n"
      "  <div id=\"PageSpeed-1\">\n"
      "    This page contains a large menu of links.\n"
      "  </div>\n"
      "</body></html>\n";
  ValidateExpected("with_divs_test", Unlabel(kExpected), kExpected);
}

TEST_F(AddIdsFilterTest, MidTagFlushTest) {
  // The filter relies on the fact that the attributes of a tag stay alive
  // across a flush window if the tag is still unclosed (but can safely
  // disappear immediately thereafter).
  // So we start with some unclosed divs with explicit ids...
  static const char kExpected1[] =
      "<html><body>\n"
      "  <div id='a'>\n"
      "    <div id='b'>\n";
  // Then after the flush we use those ids to label contained divs.
  static const char kExpected2[] =
      "      <div id=\"PageSpeed-b-0\">\n"
      "        <div id=\"PageSpeed-b-0-0\">\n"
      "        </div>\n"
      "      </div>\n"
      "    </div>\n"
      "    <div id=\"Pagespeed-a-0\">\n"
      "      <div id=\"Pagespeed-a-0-0\">\n"
      "      </div>\n"
      "    </div>\n"
      "  </div>\n"
      "</body></html>\n";
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(kExpected1);
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(Unlabel(kExpected2));
  rewrite_driver()->FinishParse();
  EXPECT_EQ(StrCat(kExpected1, kExpected2), output_buffer_);
}

TEST_F(AddIdsFilterTest, TableIds) {
  // For tables, id assignment still reflects DOM order; this may differ from
  // semantic order if the table footer is above the body (as here) or if the
  // head is below the body.
  static const char kExpected[] =
      "<table id=\"PageSpeed-0\">\n"
      "  <thead>\n"
      "    <tr id=\"PageSpeed-0-0-0\">\n"
      "      <th>Header content 1</th>\n"
      "      <th>Header content 2</th>\n"
      "    </tr>\n"
      "  </thead>\n"
      "  <tfoot>\n"
      "    <tr id=\"PageSpeed-0-1-0\">\n"
      "      <td>Footer content 1</td>\n"
      "      <td>Footer content 2</td>\n"
      "    </tr>\n"
      "  </tfoot>\n"
      "  <tbody>\n"
      "    <tr id=\"PageSpeed-0-2-0\">\n"
      "      <td>Body content 1</td>\n"
      "      <td>Body content 2</td>\n"
      "    </tr>\n"
      "  </tbody>\n"
      "</table>";
  ValidateExpected("table_ids", Unlabel(kExpected), kExpected);
}

TEST_F(AddIdsFilterTest, UnicodeInIdTest) {
  // If there's already an id, don't add another one -- even if the existing id
  // uses characters we don't like!
  const char kExpected[] =
      "<header id='g\xc5\x82\xc3\xb3wna'>Header</header>\n";
  ValidateNoChanges("Unicode id", kExpected);
}

TEST_F(AddIdsFilterTest, EmptyIdTest) {
  // Don't touch an empty or bare id.
  const char kExpected[] =
      "<header id=''>Header</header>\n"
      "<content id>Content</content>\n";
  ValidateNoChanges("Empty id", kExpected);
}

}  // namespace

}  // namespace net_instaweb
