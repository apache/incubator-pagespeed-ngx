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

// Author: sjnickerson@google.com (Simon Nickerson)

#include "net/instaweb/rewriter/public/insert_amp_link_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const char kHtmlPrefix[] = "<html>\n<head>\n";
const char kNonAmpLink[] = "<link rel=stylesheet href=\"style.css\"/>\n";
const char kHtmlSuffix[] = "</head>\n<body><p>Hello world!</p></body>\n</html>";
// Default AMP pattern which changes the domain, inserts an extra '/amp',
// respects the trailing slash (if present) and includes the query.
const char kDefaultAmpPattern[] =
    "https://ampversion.com${path_no_trailing_slash}"
    "/amp${maybe_trailing_slash}${maybe_query}";

class InsertAmpLinkFilterTest : public RewriteTestBase {
 protected:
  InsertAmpLinkFilterTest() {}
  void SetUp() override { RewriteTestBase::SetUp(); }

  bool AddBody() const override { return false; }

  virtual void AddFilter(const GoogleString& pattern) {
    options()->set_amp_link_pattern(pattern);
    options()->EnableFilter(RewriteOptions::kInsertAmpLink);
  }
};

TEST_F(InsertAmpLinkFilterTest, DoNotAddAmpLinkIfAlreadyPresent) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a.html",
      StrCat(kHtmlPrefix, "<link rel=AMPHtml href=blah>", kHtmlSuffix),
      StrCat(kHtmlPrefix, "<link rel=AMPHtml href=blah>", kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, DoNotAddAmpLinkIfAlreadyPresentQuoted) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a.html",
      StrCat(kHtmlPrefix, "<link rel=\"AMPHtml\" href=blah>", kHtmlSuffix),
      StrCat(kHtmlPrefix, "<link rel=\"AMPHtml\" href=blah>", kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, AmpLinkAddedIfOtherLinkTypePresent) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a/b/",
      StrCat(kHtmlPrefix, kNonAmpLink, kHtmlSuffix),
      StrCat(kHtmlPrefix, kNonAmpLink,
             "<link rel=\"amphtml\" href=\"https://ampversion.com/a/b/amp/\">",
             kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, MultipleHeadTagsOnlyOneLinkTagAdded) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a?q=3",

      "<html><head></head><head></head></html>",

      "<html><head>"
      "<link rel=\"amphtml\" href=\"https://ampversion.com/a/amp?q=3\">"
      "</head><head></head></html>");
}

TEST_F(InsertAmpLinkFilterTest, NoAmpTagAddedIfNoHeadTag) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a?q=3",
      "<html><body></body></html>",
      "<html><body></body></html>");
}

// Tests for badly formed patterns.

TEST_F(InsertAmpLinkFilterTest, NoClosingBrace) {
  AddFilter("${url");
  ValidateExpectedUrl(
      "http://test.com/a",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(kHtmlPrefix,
             "<link rel=\"amphtml\" href=\"${url\">",
             kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, UnknownPattern) {
  AddFilter("a${unknown_pattern}b");
  ValidateExpectedUrl(
      "http://test.com/a",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(kHtmlPrefix,
             "<link rel=\"amphtml\" href=\"a${unknown_pattern}b\">",
             kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, ClosingBraceWithoutOpeningBrace) {
  AddFilter("}${url}");
  ValidateExpectedUrl(
      "http://test.com/a",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(kHtmlPrefix,
             "<link rel=\"amphtml\" href=\"}http://test.com/a\">",
             kHtmlSuffix));
}

// Tests that the default pattern can produce URLs in the
// expected form (i.e. "insert an extra '/amp', but respect the trailing
// slash if present and include the query).

TEST_F(InsertAmpLinkFilterTest, DefaultTemplateUrlHasNoTrailingSlash) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a.html",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(
          kHtmlPrefix,
          "<link rel=\"amphtml\" href=\"https://ampversion.com/a.html/amp\">",
          kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, DefaultTemplateUrlHasTrailingSlash) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a/b/",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(kHtmlPrefix,
             "<link rel=\"amphtml\" href=\"https://ampversion.com/a/b/amp/\">",
             kHtmlSuffix));
}

TEST_F(InsertAmpLinkFilterTest, DefaultTemplateUrlHasQueryString) {
  AddFilter(kDefaultAmpPattern);
  ValidateExpectedUrl(
      "http://test.com/a?q=3",
      StrCat(kHtmlPrefix, kHtmlSuffix),
      StrCat(kHtmlPrefix,
             "<link rel=\"amphtml\" href=\"https://ampversion.com/a/amp?q=3\">",
             kHtmlSuffix));
}

}  // namespace net_instaweb
