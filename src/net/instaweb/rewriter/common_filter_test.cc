/**
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/common_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class CountingFilter : public CommonFilter {
 public:
  CountingFilter(HtmlParse* html_parse) : CommonFilter(html_parse),
                                          start_doc_calls_(0),
                                          start_element_calls_(0),
                                          end_element_calls_(0) {}

  virtual void StartDocumentImpl() { ++start_doc_calls_; }
  virtual void StartElementImpl(HtmlElement* element) {
    ++start_element_calls_;
  }
  virtual void EndElementImpl(HtmlElement* element) { ++end_element_calls_; }

  virtual const char* Name() const { return "CommonFilterTest.CountingFilter"; }

  int start_doc_calls_;
  int start_element_calls_;
  int end_element_calls_;
};

class CommonFilterTest : public ::testing::Test {
 protected:
  CommonFilterTest() : html_parse_(&handler_),
                       filter_(&html_parse_) {
    html_parse_.AddFilter(&filter_);
  }

  void ExpectUrl(const std::string& expected_url, const GURL& actual_gurl) {
    LOG(INFO) << actual_gurl;
    EXPECT_EQ(expected_url, GoogleUrl::Spec(actual_gurl));
  }

  GoogleMessageHandler handler_;
  HtmlParse html_parse_;
  CountingFilter filter_;
};

TEST_F(CommonFilterTest, DoesCallImpls) {
  EXPECT_EQ(0, filter_.start_doc_calls_);
  filter_.StartDocument();
  EXPECT_EQ(1, filter_.start_doc_calls_);

  HtmlElement* element =
      html_parse_.NewElement(NULL, html_parse_.Intern("foo"));
  EXPECT_EQ(0, filter_.start_element_calls_);
  filter_.StartElement(element);
  EXPECT_EQ(1, filter_.start_element_calls_);

  EXPECT_EQ(0, filter_.end_element_calls_);
  filter_.EndElement(element);
  EXPECT_EQ(1, filter_.end_element_calls_);
}

TEST_F(CommonFilterTest, StoresCorrectBaseUrl) {
  std::string doc_url = "http://www.example.com/";
  html_parse_.StartParse(doc_url);
  html_parse_.Flush();
  // Base URL starts out as document URL.
  ExpectUrl(doc_url, html_parse_.gurl());
  ExpectUrl(doc_url, filter_.base_gurl());

  html_parse_.ParseText("<html><head><link rel='stylesheet' href='foo.css'>");
  html_parse_.Flush();
  ExpectUrl(doc_url, filter_.base_gurl());

  std::string base_url = "http://www.baseurl.com/foo/";
  html_parse_.ParseText("<base href='");
  html_parse_.ParseText(base_url);
  html_parse_.ParseText("' />");
  html_parse_.Flush();
  // Update to base URL.
  ExpectUrl(base_url, filter_.base_gurl());
  // Make sure we didn't change the document URL.
  ExpectUrl(doc_url, html_parse_.gurl());

  html_parse_.ParseText("<link rel='stylesheet' href='foo.css'>");
  html_parse_.Flush();
  ExpectUrl(base_url, filter_.base_gurl());

  std::string new_base_url = "http://www.somewhere-else.com/";
  html_parse_.ParseText("<base href='");
  html_parse_.ParseText(new_base_url);
  html_parse_.ParseText("' />");
  html_parse_.Flush();
  // Uses new base URL.
  ExpectUrl(new_base_url, filter_.base_gurl());

  html_parse_.ParseText("</head></html>");
  html_parse_.FinishParse();
  ExpectUrl(new_base_url, filter_.base_gurl());
  ExpectUrl(doc_url, html_parse_.gurl());
}

TEST_F(CommonFilterTest, DetectsNoScriptCorrectly) {
  std::string doc_url = "http://www.example.com/";
  html_parse_.StartParse(doc_url);
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() == NULL);

  html_parse_.ParseText("<html><head><title>Example Site");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() == NULL);

  html_parse_.ParseText("</title><noscript>");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() != NULL);

  // Nested <noscript> elements
  html_parse_.ParseText("Blah blah blah <noscript><noscript> do-de-do-do ");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() != NULL);

  html_parse_.ParseText("<link href='style.css'>");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() != NULL);

  // Close inner <noscript>s
  html_parse_.ParseText("</noscript></noscript>");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() != NULL);

  // Close outter <noscript>
  html_parse_.ParseText("</noscript>");
  html_parse_.Flush();
  EXPECT_TRUE(filter_.noscript_element() == NULL);

  html_parse_.ParseText("</head></html>");
  html_parse_.FinishParse();
  EXPECT_TRUE(filter_.noscript_element() == NULL);

}

}  // namespace

}  // namespace net_instaweb
