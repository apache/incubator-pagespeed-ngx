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

// Author: slamm@google.com (Stephen Lamm)

#include "net/instaweb/rewriter/public/critical_css_filter.h"

#include <utility>

#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kRequestUrl[] = "http://test.com";

}  // namespace

namespace net_instaweb {

class Statistics;

namespace {

class MockCriticalCssFinder : public CriticalCssFinder {
 public:
  explicit MockCriticalCssFinder(RewriteDriver* driver, Statistics* stats)
      : CriticalCssFinder(stats),
        driver_(driver),
        critical_css_map_(new StringStringMap()) {
  }

  void AddCriticalCss(const StringPiece& url, const StringPiece& critical_css) {
    critical_css_map_->insert(
        std::make_pair(url.as_string(), critical_css.as_string()));
  }

  // Mock to avoid dealing with property cache.
  StringStringMap* CriticalCssMap(RewriteDriver* driver) {
    return critical_css_map_;
  }

  void ComputeCriticalCss(StringPiece url, RewriteDriver* driver) {}
  const char* GetCohort() const { return "critical_css"; }

 private:
  RewriteDriver* driver_;
  StringStringMap* critical_css_map_;
};

}  // namespace

class CriticalCssFilterTest : public RewriteTestBase {
 public:
  CriticalCssFilterTest() {  }

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();

    finder_ = new MockCriticalCssFinder(rewrite_driver(), statistics());
    server_context()->set_critical_css_finder(finder_);

    filter_.reset(new CriticalCssFilter(rewrite_driver(), finder_));
    rewrite_driver()->AddFilter(filter_.get());

    ResetDriver();
  }

  void ResetDriver() {
    PropertyCache* pcache = page_property_cache();
    server_context_->set_enable_property_cache(true);
    SetupCohort(pcache, finder_->GetCohort());

    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);
  }

  scoped_ptr<CriticalCssFilter> filter_;
  MockCriticalCssFinder* finder_;
};

TEST_F(CriticalCssFilterTest, UnchangedWhenPcacheEmpty) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>a {color: red }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  ValidateExpected("unchanged_when_pcache_empty", input_html, input_html);

  // TODO(slamm): webkit headless gets called
}

TEST_F(CriticalCssFilterTest, InlineAndMove) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <style media=\"print\">a_used {color: azure }</style>"
      "<style>b_used {color: blue }</style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>";

  finder_->AddCriticalCss("http://test.com/a.css", "a_used {color: azure }");
  finder_->AddCriticalCss("http://test.com/b.css", "b_used {color: blue }");
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }");

  ValidateExpected("inline_and_move", input_html, expected_html);
}

TEST_F(CriticalCssFilterTest, InvalidUrl) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<link rel='stylesheet' href='Hi there!' type='text/css'>"
      "<link rel='stylesheet' href='c.css' type='text/css'>";

  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }");

  ValidateExpected("inline_and_move", input_html, expected_html);
}

TEST_F(CriticalCssFilterTest, NullAndEmptyCriticalRules) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<style></style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>";

  // Skip adding a critical CSS for a.css.
  //     In the filtered html, the original link is left in place and
  //     a duplicate link is added to the full set of CSS at the bottom
  //     to make sure CSS rules are applied in the correct order.

  finder_->AddCriticalCss("http://test.com/b.css", "");  // no critical rules
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }");

  ValidateExpected("inline_and_move", input_html, expected_html);
}

}  // namespace net_instaweb
