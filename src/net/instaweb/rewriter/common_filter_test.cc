/*
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

#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class HtmlElement;

namespace {

class CountingFilter : public CommonFilter {
 public:
  explicit CountingFilter(RewriteDriver* driver) : CommonFilter(driver),
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

class CommonFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_.reset(new CountingFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
  }

  void ExpectUrl(const GoogleString& expected_url,
                 const GoogleUrl& actual_gurl) {
    EXPECT_EQ(expected_url, actual_gurl.Spec());
  }

  bool CanRewriteResource(CommonFilter* filter, const StringPiece& url) {
    ResourcePtr resource(filter->CreateInputResource(url));
    return (resource.get() != NULL);
  }

  CommonFilter* MakeFilter(const StringPiece& base_url,
                           const StringPiece& domain,
                           RewriteOptions* options,
                           RewriteDriver* driver) {
    options->WriteableDomainLawyer()->AddDomain(domain, message_handler());
    CountingFilter* filter = new CountingFilter(driver);
    driver->AddOwnedPostRenderFilter(filter);
    driver->StartParse(base_url);
    driver->Flush();
    return filter;
  }

  scoped_ptr<CountingFilter> filter_;
};

TEST_F(CommonFilterTest, DoesCallImpls) {
  EXPECT_EQ(0, filter_->start_doc_calls_);
  filter_->StartDocument();
  EXPECT_EQ(1, filter_->start_doc_calls_);

  RewriteDriver* driver = rewrite_driver();
  HtmlElement* element = driver->NewElement(NULL, "foo");
  EXPECT_EQ(0, filter_->start_element_calls_);
  filter_->StartElement(element);
  EXPECT_EQ(1, filter_->start_element_calls_);

  EXPECT_EQ(0, filter_->end_element_calls_);
  filter_->EndElement(element);
  EXPECT_EQ(1, filter_->end_element_calls_);
}

TEST_F(CommonFilterTest, StoresCorrectBaseUrl) {
  GoogleString doc_url = "http://www.example.com/";
  RewriteDriver* driver = rewrite_driver();
  driver->StartParse(doc_url);
  driver->Flush();
  // Base URL starts out as document URL.
  ExpectUrl(doc_url, driver->google_url());
  ExpectUrl(doc_url, filter_->base_url());

  driver->ParseText(
      "<html><head><link rel='stylesheet' href='foo.css'>");
  driver->Flush();
  ExpectUrl(doc_url, filter_->base_url());

  GoogleString base_url = "http://www.baseurl.com/foo/";
  driver->ParseText("<base href='");
  driver->ParseText(base_url);
  driver->ParseText("' />");
  driver->Flush();
  // Update to base URL.
  ExpectUrl(base_url, filter_->base_url());
  // Make sure we didn't change the document URL.
  ExpectUrl(doc_url, driver->google_url());

  driver->ParseText("<link rel='stylesheet' href='foo.css'>");
  driver->Flush();
  ExpectUrl(base_url, filter_->base_url());

  GoogleString new_base_url = "http://www.somewhere-else.com/";
  driver->ParseText("<base href='");
  driver->ParseText(new_base_url);
  driver->ParseText("' />");
  driver->Flush();
  EXPECT_EQ(1, message_handler()->TotalMessages());

  // Uses old base URL.
  ExpectUrl(base_url, filter_->base_url());

  driver->ParseText("</head></html>");
  driver->Flush();
  ExpectUrl(base_url, filter_->base_url());
  driver->FinishParse();
  ExpectUrl(doc_url, driver->google_url());
}

TEST_F(CommonFilterTest, ResolveUrl) {
  GoogleUrl out;

  // Normal parse, no <base>
  GoogleString doc_url = "http://www.example.com/";
  RewriteDriver* driver = rewrite_driver();
  driver->StartParse(doc_url);
  filter_->ResolveUrl("a.css", &out);
  ExpectUrl("http://www.example.com/a.css", out);
  driver->FinishParse();

  // Refs from base
  driver->StartParse(doc_url);
  driver->ParseText("<base href='https://www.example.org/' >");
  driver->Flush();
  filter_->ResolveUrl("a.css", &out);
  ExpectUrl("https://www.example.org/a.css", out);
  driver->FinishParse();

  // Nasty case: refs before base.
  driver->StartParse(doc_url);
  driver->set_refs_before_base();
  driver->Flush();
  filter_->ResolveUrl("a.css", &out);
  EXPECT_FALSE(out.IsAnyValid());
  driver->ParseText("<base href='https://www.example.org/' >");
  driver->Flush();
  filter_->ResolveUrl("a.css", &out);
  ExpectUrl("https://www.example.org/a.css", out);
  driver->FinishParse();
}

TEST_F(CommonFilterTest, DetectsNoScriptCorrectly) {
  GoogleString doc_url = "http://www.example.com/";
  RewriteDriver* driver = rewrite_driver();
  driver->StartParse(doc_url);
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() == NULL);

  driver->ParseText("<html><head><title>Example Site");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() == NULL);

  driver->ParseText("</title><noscript>");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() != NULL);

  // Nested <noscript> elements
  driver->ParseText("Blah blah blah <noscript><noscript> do-de-do-do ");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() != NULL);

  driver->ParseText("<link href='style.css'>");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() != NULL);

  // Close inner <noscript>s
  driver->ParseText("</noscript></noscript>");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() != NULL);

  // Close outter <noscript>
  driver->ParseText("</noscript>");
  driver->Flush();
  EXPECT_TRUE(filter_->noscript_element() == NULL);

  driver->ParseText("</head></html>");
  driver->FinishParse();
  EXPECT_TRUE(filter_->noscript_element() == NULL);
}

TEST_F(CommonFilterTest, TestTwoDomainLawyers) {
  static const char kBaseUrl[] = "http://www.base.com/";
  CommonFilter* a = MakeFilter(kBaseUrl, "a.com", options(), rewrite_driver());
  CommonFilter* b = MakeFilter(kBaseUrl, "b.com", other_options(),
                               other_rewrite_driver());

  // Either filter can rewrite resources from the base URL
  EXPECT_TRUE(CanRewriteResource(a, StrCat(kBaseUrl, "base.css")));
  EXPECT_TRUE(CanRewriteResource(b, StrCat(kBaseUrl, "base.css")));

  // But the other domains are specific to the two different drivers/filters
  EXPECT_TRUE(CanRewriteResource(a, "http://a.com/a.css"));
  EXPECT_FALSE(CanRewriteResource(a, "http://b.com/b.css"));
  EXPECT_FALSE(CanRewriteResource(b, "http://a.com/a.css"));
  EXPECT_TRUE(CanRewriteResource(b, "http://b.com/b.css"));
}

const char kEndDocumentComment[] = "<!--test comment-->";

class EndDocumentInserterFilter : public CommonFilter {
 public:
  explicit EndDocumentInserterFilter(RewriteDriver* driver)
      : CommonFilter(driver)
  {}

  virtual void EndDocument() {
    InsertNodeAtBodyEnd(driver()->NewCommentNode(NULL, "test comment"));
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const {
    return "CommonFilterTest.EndDocumentInserterFilter";
  }
};

class CommonFilterInsertNodeAtBodyEndTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_.reset(new EndDocumentInserterFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
    SetupWriter();
  }

  void StartTest(StringPiece pre_comment) {
    GoogleString url = "http://www.example.com/";
    rewrite_driver()->StartParse(url);
    rewrite_driver()->ParseText(pre_comment);
  }

  const GoogleString FinishTest(StringPiece pre_comment,
                                StringPiece post_comment) {
    const GoogleString expected_html =
        StrCat(pre_comment, kEndDocumentComment, post_comment);
    rewrite_driver()->ParseText(post_comment);
    rewrite_driver()->FinishParse();
    return expected_html;
  }

  const GoogleString FullTest(StringPiece pre_comment,
                              StringPiece post_comment) {
    StartTest(pre_comment);
    return FinishTest(pre_comment, post_comment);
  }

  const GoogleString FlushTest(StringPiece pre_flush, StringPiece pre_comment,
                               StringPiece post_comment) {
    StartTest(pre_flush);
    rewrite_driver()->Flush();
    rewrite_driver()->ParseText(pre_comment);
    GoogleString full_pre_comment = StrCat(pre_flush, pre_comment);
    return FinishTest(full_pre_comment, post_comment);
  }

  scoped_ptr<EndDocumentInserterFilter> filter_;
};

TEST_F(CommonFilterInsertNodeAtBodyEndTest, OneBody) {
  GoogleString expected =
      FullTest("<html><head></head><body>", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, WhiteSpace) {
  GoogleString expected =
      FullTest("<html><head></head><body>", "</body>\n</html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, NoBody) {
  GoogleString expected =
      FullTest("some content without body tag\n</html>", "");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, NoCloseBody) {
  GoogleString expected =
      FullTest("<html><head></head><body><img src=\"a.jpg\">", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, FlushInBody) {
  GoogleString expected =
      FlushTest("<html><head></head><body>", "", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, FlushBeforeBody) {
  GoogleString expected =
      FlushTest("<html><head></head>", "<body>", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, FlushAfterCloseBody) {
  // kEndDocumentComment gets inserted after </body> since both the open and
  // close tags have been flushed already.
  GoogleString expected =
      FlushTest("<html><head></head><body></body>", "", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, FlushAtEnd) {
  // This causes us to append to the end of document after the flush.
  GoogleString expected =
      FlushTest("<html><head></head><body></body></html>", "", "");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, TwoBodies) {
  GoogleString expected =
      FullTest("<html><head></head><body></body><body>", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, TextAfterCloseBody) {
  GoogleString expected =
      FullTest("<html><head></head><body></body>extra text", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, TextAfterCloseHtml) {
  GoogleString expected =
      FullTest("<html><head></head><body></body></html>extra text", "");
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(CommonFilterInsertNodeAtBodyEndTest, BodyInNoscript) {
  GoogleString expected = FullTest(
      "<html><head></head><noscript><body></body></noscript>", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
