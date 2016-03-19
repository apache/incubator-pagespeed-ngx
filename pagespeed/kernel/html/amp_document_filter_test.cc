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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the AmpDocumentFilter.

#include "pagespeed/kernel/html/amp_document_filter.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

namespace {

class AmpDocumentFilterTest : public HtmlParseTestBase  {
 protected:
  static const char kUtf8LightningBolt[];

  AmpDocumentFilterTest()
      : amp_document_filter_(&html_parse_, NewPermanentCallback(
            this, &AmpDocumentFilterTest::AmpDiscovered)) {
  }

  void AmpDiscovered(bool is_amp) {
    is_amp_ = is_amp;
    EXPECT_FALSE(disovered_called_);
    disovered_called_ = true;
  }

  void SetUp() override {
    HtmlParseTestBase::SetUp();
    html_parse_.AddFilter(&amp_document_filter_);
    is_amp_ = false;
    disovered_called_ = false;
  }

  void RunWithEveryFlushPoint(StringPiece html, bool expect_is_amp) {
    for (int i = 0, n = html.size(); i < n; ++i) {
      is_amp_ = false;
      disovered_called_ = false;
      html_parse_.StartParse(
          StringPrintf("http://example.com/amp_doc_%d.html", i));
      html_parse_.ParseText(html.substr(0, i));
      html_parse_.Flush();
      html_parse_.ParseText(html.substr(i));
      html_parse_.FinishParse();
      EXPECT_EQ(expect_is_amp, is_amp_);
      EXPECT_TRUE(disovered_called_);
    }
  }

  bool AddBody() const override { return false; }
  bool AddHtmlTags() const override { return false; }

  bool is_amp_;
  bool disovered_called_;
  AmpDocumentFilter amp_document_filter_;
};

TEST_F(AmpDocumentFilterTest, IsAmpHtml) {
  RunWithEveryFlushPoint("<!doctype foo> <html amp><head/><body></body></html>",
                         true);
}

TEST_F(AmpDocumentFilterTest, IsAMPHtml) {
  RunWithEveryFlushPoint("<!doctype foo> <html AMP><head/><body></body></html>",
                         true);
}

TEST_F(AmpDocumentFilterTest, IsaMPHtml) {
  RunWithEveryFlushPoint("<!doctype foo> <html aMP><head/><body></body></html>",
                         true);
}

TEST_F(AmpDocumentFilterTest, IsAmpLightningBolt) {
  RunWithEveryFlushPoint(StrCat("<!doctype foo>  <html ",
                                AmpDocumentFilter::kUtf8LightningBolt,
                                "><head/><body></body></html>"),
                         true);
}

TEST_F(AmpDocumentFilterTest, IsNotAmp) {
  RunWithEveryFlushPoint("<!doctype foo>  <html><head/><body></body></html>",
                         false);
}

TEST_F(AmpDocumentFilterTest, EmptyHtml) {
  RunWithEveryFlushPoint("", false);
}

TEST_F(AmpDocumentFilterTest, JustText) {
  RunWithEveryFlushPoint("Hello, world!", false);
}

TEST_F(AmpDocumentFilterTest, JustImg) {
  RunWithEveryFlushPoint("<img src='foo.png' />", false);
}

TEST_F(AmpDocumentFilterTest, DoctypeOnly) {
  RunWithEveryFlushPoint("<!doctype foo>", false);
}

TEST_F(AmpDocumentFilterTest, DoctypeText) {
  RunWithEveryFlushPoint("<!doctype foo>Hello", false);
}

TEST_F(AmpDocumentFilterTest, DoctypeImage) {
  RunWithEveryFlushPoint("<!doctype foo><img src='foo.png' />", false);
}

TEST_F(AmpDocumentFilterTest, TwoConsecutiveHtmlTagsAmpInFirst) {
  RunWithEveryFlushPoint("<!doctype foo><html amp></html><html></html>", true);
}

TEST_F(AmpDocumentFilterTest, TwoConsecutiveHtmlTagsAmpInSecond) {
  RunWithEveryFlushPoint("<!doctype foo><html></html><html amp></html>", false);
}

TEST_F(AmpDocumentFilterTest, TooLateForAmpTag) {
  ValidateExpected(
      "invalid_amp",
      "<!doctype foo><other/><html amp><body></body></html>",
      StrCat("<!doctype foo><other/><!--",
             AmpDocumentFilter::kInvalidAmpDirectiveComment,
             "--><html amp><body></body></html>"));
  EXPECT_FALSE(is_amp_);
  EXPECT_TRUE(disovered_called_);
}

TEST_F(AmpDocumentFilterTest, TooLateForAmpNonWhitespaceCharacters) {
  ValidateExpected(
      "invalid_amp",
      "<!doctype foo>  stuff <html amp><body></body></html>",
      StrCat("<!doctype foo>  stuff <!--",
             AmpDocumentFilter::kInvalidAmpDirectiveComment,
             "--><html amp><body></body></html>"));
  EXPECT_FALSE(is_amp_);
  EXPECT_TRUE(disovered_called_);
}

}  // namespace

}  // namespace net_instaweb
