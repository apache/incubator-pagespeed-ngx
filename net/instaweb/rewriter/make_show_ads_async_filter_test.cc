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
// Author: chenyu@google.com (Yu Chen), morlovich@google.com (Maks Orlovich)

#include "net/instaweb/rewriter/public/make_show_ads_async_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {
namespace {

// Helper methods for generating test html page / snippet.
GoogleString GetPage(StringPiece content) {
  return StrCat("<head><title>Something</title></head><body>",
                content,
                "</body>");
}

GoogleString GetAdsByGoogleInsWithContent(StringPiece content) {
  return StrCat("<ins class=\"adsbygoogle\" ", content, " </ins>");
}

GoogleString GetShowAdsDataSnippetWithContent(StringPiece content) {
  return StrCat("<script type=\"text/javascript\"> ", content, " </script>");
}

GoogleString GetPageWithOneAdsByGoogle(StringPiece content) {
  return GetPage(GetAdsByGoogleInsWithContent(content));
}

// Constants used in input/output.
const char kAdsByGoogleJs[] = "<script async "
    "src=\"//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js\">"
    "</script>";
const char kAdsByGoogleApiCall[] =
    "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>";
const char kShowAdsApiCall[] =
    "<script type=\"text/javascript\" "
    "src=\"http://pagead2.googlesyndication.com/pagead/show_ads.js\">"
    " </script>";

// Test data for adsbygoogle snippet1 and the expected output.
const char kAdsByGoogleContent1[] =
    "style=\"display:inline-block;width:160px;height:600px\" "
    "data-ad-client=\"test-publishercode-expected\" "
    "data-ad-slot=\"1234567\" "
    "data-ad-channel=\"test-original-channel\">";  // Original channel.

// Test data for adsbygoogle snippet2 and the expected output.
// No channel is set in this data.
const char kAdsByGoogleContent2[] =
    "style=\"display:inline-block;width:162px;height:602px\" "
    "data-ad-client=\"test-publishercode-expected\" "
    "data-ad-slot=\"1234562\">";

// Test data for showads snippet1 and the expected output.
const char kShowAdsDataContentFormat1[] =
    "%s"
    "google_ad_client = \"test-publishercode-expected\"; "
    "%s"
    "google_ad_channel = \"test-original-channel\"; "  // Original channel.
    "google_ad_slot = \"1234567\";"
    "google_ad_width = 728;"
    "google_ad_height = 90;"
     "%s";

const char kShowAdsDataContentFormat1Output[] =
    "<ins class=\"adsbygoogle\" "
    "style=\"display:inline-block;width:728px;height:90px\" "
    "data-ad-channel=\"test-original-channel\" "
    "data-ad-client=\"test-publishercode-expected\" "
    "data-ad-slot=\"1234567\">"
    "</ins>";

// Help methods to get variants of input and the expected output for showads
// snippet1.
GoogleString GetShowAdsDataContent1() {
  return StringPrintf(kShowAdsDataContentFormat1, "", "", "");
}

GoogleString GetShowAdsDataContent1WithComments() {
  return StringPrintf(kShowAdsDataContentFormat1, "", "/* comment */", "");
}

GoogleString GetShowAdsDataContent1WithCommentTags() {
  return StringPrintf(kShowAdsDataContentFormat1,
                      "<!--", "", "//-->");
}

GoogleString GetShowAdsDataFormat1Output() {
  return StrCat(kAdsByGoogleJs,
                kShowAdsDataContentFormat1Output,
                kAdsByGoogleApiCall);
}

// Test data for showads snippet2.
// No original channel is set in this data.
const char kShowAdsDataContentFormat2[] =
    "<!--"
    "google_ad_client = \"test-publishercode-expected\"; "
    "/**/"
    "google_ad_slot = \"1234562\";"
    "google_ad_width = 722;"
    "google_ad_height = 92;"
    "google_ad_format = \"722x92\";"
    "//-->";

const char kShowAdsDataContentFormat2Output[] =
    "<ins class=\"adsbygoogle\" "
    "style=\"display:inline-block;width:722px;height:92px\" "
    "data-ad-client=\"test-publishercode-expected\" "
    "data-ad-format=\"722x92\" "
    "data-ad-slot=\"1234562\"></ins>";

GoogleString GetShowAdsDataFormat2Output() {
  return StrCat(kAdsByGoogleJs,
                kShowAdsDataContentFormat2Output,
                kAdsByGoogleApiCall);
}

// Help methods for tesing pages with multiple showads snippets.
GoogleString GetHtmlPageMultipleShowAds() {
  return GetPage(StrCat(
      GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
      kShowAdsApiCall,
      GetShowAdsDataSnippetWithContent(kShowAdsDataContentFormat2),
      kShowAdsApiCall));
}

// Test data for ad snippets for which conversion is not applicable.
const char kShowAdsHtmlPageWithMissingAttribute[] =
    "<!--"
    "google_ad_client = \"test-publishercode-expected\"; "
    "google_ad_slot = \"1234567\";"
    "google_ad_height = 90;"  // Attribute google_ad_width is missing.
    "//-->";
const char kShowAdsHtmlPageWithUnexpectedStatement[] =
    "<!--"
    "google_ad_client = \"test-publishercode-expected\"; "
    "google_ad_slot = \"1234567\";"
    "google_ad_height = 90;"
    "if (a) google_ad_width = 180; else google_ad_width = 190;"  // Invalid
    "//-->";
const char kShowAdsHtmlPageWithInvalidGoogleAdFormat[] =
    "<!--"
    "google_ad_client = \"test-publishercode-expected\"; "
    "google_ad_slot = \"1234567\";"
    "google_ad_width = 722;"
    "google_ad_height = 92;"
    "google_ad_format = \"weird_722x92_as_rimg\";"
    "//-->";

// Test fixture for MakeShowAdsAsyncFilter unit tests.
class MakeShowAdsAsyncFilterTest : public RewriteTestBase {
 protected:
  MakeShowAdsAsyncFilterTest() : add_tags_(true) {}
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    MakeShowAdsAsyncFilter::InitStats(rewrite_driver()->statistics());
    filter_.reset(new MakeShowAdsAsyncFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
  }

  int GetStat(const char* stat_name) {
    return rewrite_driver()->statistics()->FindVariable(stat_name)->Get();
  }

  int GetStatShowAdsSnippetsConverted() {
     return GetStat(MakeShowAdsAsyncFilter::kShowAdsSnippetsConverted);
  }

  int GetStatShowAdsSnippetsNotConverted() {
    return GetStat(MakeShowAdsAsyncFilter::kShowAdsSnippetsNotConverted);
  }

  int GetStatShowAdsapiReplaced() {
    return GetStat(MakeShowAdsAsyncFilter::kShowAdsApiReplacedForAsync);
  }

  void CheckStatForNoApplicableAds() {
    EXPECT_EQ(0, GetStatShowAdsSnippetsConverted());
    EXPECT_EQ(0, GetStatShowAdsSnippetsNotConverted());
    EXPECT_EQ(0, GetStatShowAdsapiReplaced());
  }

  void CheckStatForShowAds(int count) {
    EXPECT_EQ(count, GetStatShowAdsSnippetsConverted());
    EXPECT_EQ(0, GetStatShowAdsSnippetsNotConverted());
    EXPECT_EQ(count, GetStatShowAdsapiReplaced());
  }

  void CheckStatForShowAdsMissingAPICall(int count) {
    EXPECT_EQ(count, GetStatShowAdsSnippetsConverted());
    EXPECT_EQ(0, GetStatShowAdsSnippetsNotConverted());
    EXPECT_EQ(0, GetStatShowAdsapiReplaced());
  }

  bool AddBody() const override { return add_tags_; }
  bool AddHtmlTags() const override { return add_tags_; }

  scoped_ptr<MakeShowAdsAsyncFilter> filter_;
  bool add_tags_;
};

TEST_F(MakeShowAdsAsyncFilterTest, NoAds) {
  ValidateNoChanges(test_info_->name(), GetPage("HTML page with no ads"));
  CheckStatForNoApplicableAds();
}

// Test fixture for Html page with showads ads.

TEST_F(MakeShowAdsAsyncFilterTest, OneShowAds) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
          kShowAdsApiCall)),
      GetPage(GetShowAdsDataFormat1Output()));
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, OneShowAdsWithComments) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(
              GetShowAdsDataContent1WithComments()),
          kShowAdsApiCall)),
      GetPage(GetShowAdsDataFormat1Output()));
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, OneShowAdsWithEnclosingCommentTags) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(
              GetShowAdsDataContent1WithCommentTags()),
          kShowAdsApiCall)),
      GetPage(GetShowAdsDataFormat1Output()));
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, OneShowAdsData2) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(kShowAdsDataContentFormat2),
          kShowAdsApiCall)),
      GetPage(GetShowAdsDataFormat2Output()));
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, MultipleShowAds) {
  ValidateExpected(
      test_info_->name(),
      GetHtmlPageMultipleShowAds(),
      GetPage(StrCat(
            GetShowAdsDataFormat1Output(),
            kShowAdsDataContentFormat2Output,
            kAdsByGoogleApiCall)));
  CheckStatForShowAds(2);
}

TEST_F(MakeShowAdsAsyncFilterTest, ShowsAdsHtmlGoogleAdOutput) {
  // Hack the <ins> tag to have the data- field for google_ad_output.
  GoogleString output_with_ad_output = kShowAdsDataContentFormat1Output;
  GlobalReplaceSubstring("data-ad-slot",
                         "data-ad-output=\"html\" data-ad-slot",
                         &output_with_ad_output);
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(
              StringPrintf(kShowAdsDataContentFormat1,
                           "google_ad_output='html';",
                           "", "")),
          kShowAdsApiCall)),
      GetPage(StrCat(kAdsByGoogleJs,
                     output_with_ad_output,
                     kAdsByGoogleApiCall)));
  CheckStatForShowAds(1);
}

// Test fixture for mixed ads.
TEST_F(MakeShowAdsAsyncFilterTest, MixedAds) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          kAdsByGoogleJs,
          // An adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(kAdsByGoogleContent1),
          kAdsByGoogleApiCall,
          // A showads ad.
          GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
          kShowAdsApiCall,
          // An adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(kAdsByGoogleContent2),
          kAdsByGoogleApiCall,
          // A showads ad.
          GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
          kShowAdsApiCall)),
      GetPage(StrCat(
          kAdsByGoogleJs,
          // Output for an adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(
              kAdsByGoogleContent1),
          kAdsByGoogleApiCall,
          // Output for an showads snippet.
          kShowAdsDataContentFormat1Output,
          kAdsByGoogleApiCall,
          // Output for an adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(
              kAdsByGoogleContent2),
          kAdsByGoogleApiCall,
          // Output for an showads snippet.
          kShowAdsDataContentFormat1Output,
          kAdsByGoogleApiCall)));
  CheckStatForShowAds(2);
}

// Test fixture for misplaced ads.

TEST_F(MakeShowAdsAsyncFilterTest, ShowAdsMissingAPICallFlag) {
  ValidateExpected(
      test_info_->name(),
      GetPage(GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1())),
      GetPage(StrCat(kAdsByGoogleJs,
                     kShowAdsDataContentFormat1Output)));
  CheckStatForShowAdsMissingAPICall(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, MispairedShowAdsFlag) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          kShowAdsApiCall,   // Extra showads API call.
          GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
          kShowAdsApiCall,
          kShowAdsApiCall)),  // Extra showads API call.
      GetPage(StrCat(
          kShowAdsApiCall,
          kAdsByGoogleJs,
          kShowAdsDataContentFormat1Output,
          kAdsByGoogleApiCall,
          kShowAdsApiCall)));
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, MixedAdsWithMisplacedSnippet) {
  ValidateExpected(
      test_info_->name(),
      GetPage(StrCat(
          kAdsByGoogleJs,
          // An adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(kAdsByGoogleContent1),
          kAdsByGoogleApiCall,
          // A showads ad missing data
          kShowAdsApiCall,
          // An adsbygoogle snippet missing Api call.
          GetAdsByGoogleInsWithContent(kAdsByGoogleContent2),
          // A showads ad.
          GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
          kShowAdsApiCall)),
      GetPage(StrCat(
          kAdsByGoogleJs,
          // Output for an adsbygoogle snippet.
          GetAdsByGoogleInsWithContent(
              kAdsByGoogleContent1),
          kAdsByGoogleApiCall,
          // Output for an showads snippet missing data.
          kShowAdsApiCall,
          // Output for an adsbygoogle snippet missing Api call.
          GetAdsByGoogleInsWithContent(
              kAdsByGoogleContent2),
          // Output for an showads snippet.
          kShowAdsDataContentFormat1Output,
          kAdsByGoogleApiCall)));
  EXPECT_EQ(1, GetStatShowAdsSnippetsConverted());
  EXPECT_EQ(0, GetStatShowAdsSnippetsNotConverted());
  EXPECT_EQ(1, GetStatShowAdsapiReplaced());
}

// Test fixture for non-applicable snippets.

TEST_F(MakeShowAdsAsyncFilterTest, ShowAdsMissingAttribute) {
  ValidateNoChanges(
      test_info_->name(),
      GetPage(GetShowAdsDataSnippetWithContent(
          kShowAdsHtmlPageWithMissingAttribute)));
  CheckStatForNoApplicableAds();
}

TEST_F(MakeShowAdsAsyncFilterTest, ShowAdsUnexpectedStatement) {
  ValidateNoChanges(
      test_info_->name(),
      GetPage(GetShowAdsDataSnippetWithContent(
          kShowAdsHtmlPageWithUnexpectedStatement)));
  CheckStatForNoApplicableAds();
}

TEST_F(MakeShowAdsAsyncFilterTest, ShowAdsInvalidGoogleAdFormat) {
  ValidateNoChanges(
      test_info_->name(),
      GetPage(GetShowAdsDataSnippetWithContent(
          kShowAdsHtmlPageWithInvalidGoogleAdFormat)));
}

TEST_F(MakeShowAdsAsyncFilterTest, ShowsAdsJsGoogleAdOutput) {
  ValidateNoChanges(
      test_info_->name(),
      GetPage(StrCat(
          GetShowAdsDataSnippetWithContent(
              StringPrintf(kShowAdsDataContentFormat1, "google_ad_output='js';",
                           "", "")),
          kShowAdsApiCall)));
}

TEST_F(MakeShowAdsAsyncFilterTest, FlushInTheMiddleOfShowAdsDataScript) {
  // TODO(morlovich): Split more flush configurations, perhaps even
  // arbitrary ones.
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<head><title>Something</title></head><body>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\"> "
      "google_ad_client = \"test-publishercode-expected\"; "
      "google_ad_channel = \"test-original-channel\"; ");
  // The flush makes the showads data script not rewritable.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(
      "google_ad_slot = \"1234567\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;");
  rewrite_driver()->ParseText("</script>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\" "
      "src=\"http://pagead2.googlesyndication.com/pagead/show_ads.js\"> "
      "</script>");
  rewrite_driver()->ParseText("</body>");
  rewrite_driver()->FinishParse();

  // The showads data script element is rewritten because HtmlParse will
  // buffer the <script>... until it sees "</script>"
  EXPECT_STREQ(
      "<head><title>Something</title></head><body><script async "
      "src=\"//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js\">"
      "</script><ins class=\"adsbygoogle\" "
      "style=\"display:inline-block;width:728px;height:90px\" "
      "data-ad-channel=\"test-original-channel\" "
      "data-ad-client=\"test-publishercode-expected\" "
      "data-ad-slot=\"1234567\"></ins>"
      "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>"
      "</body>",
      output_buffer_);
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, FlushInTheMiddleOfShowAdsApiCall) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<head><title>Something</title></head><body>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\"> "
      "google_ad_client = \"test-publishercode-expected\"; "
      "google_ad_channel = \"test-original-channel\"; "
      "google_ad_slot = \"1234567\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;</script>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\" "
      "src=\"http://pagead2.googlesyndication.com/pagead/show_ads.js\"> ");
  // The flush makes the showads API call not rewritable.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</script>");
  rewrite_driver()->ParseText("</body>");
  rewrite_driver()->FinishParse();

  // The showads data script element is rewritten and the showads api call is
  // as well, because HtmlParse will buffer the <script> contents until it
  // sees </script>.
  EXPECT_STREQ(
      "<head><title>Something</title></head><body><script async "
      "src=\"//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js\">"
      "</script><ins class=\"adsbygoogle\" "
      "style=\"display:inline-block;width:728px;height:90px\" "
      "data-ad-channel=\"test-original-channel\" "
      "data-ad-client=\"test-publishercode-expected\" "
      "data-ad-slot=\"1234567\"></ins>"
      "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>"
      "</body>",
      output_buffer_);
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, FlushInTheMiddleOfShowAdsDataAndApiCall) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<head><title>Something</title></head><body>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\"> "
      "google_ad_client = \"test-publishercode-expected\"; "
      "google_ad_channel = \"test-original-channel\"; ");
  rewrite_driver()->ParseText(
      "google_ad_slot = \"1234567\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;");
  // The flush makes the showads data script not rewritable.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</script>");
  rewrite_driver()->ParseText(
      "<script type=\"text/javascript\" "
      "src=\"http://pagead2.googlesyndication.com/pagead/show_ads.js\"> ");
  // The flush makes the showads api call script not rewritable.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</script>");
  rewrite_driver()->ParseText("</body>");
  rewrite_driver()->FinishParse();

  EXPECT_STREQ(
      "<head><title>Something</title></head><body><script async "
      "src=\"//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js\">"
      "</script><ins class=\"adsbygoogle\" "
      "style=\"display:inline-block;width:728px;height:90px\" "
      "data-ad-channel=\"test-original-channel\" "
      "data-ad-client=\"test-publishercode-expected\" "
      "data-ad-slot=\"1234567\"></ins>"
      "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>"
      "</body>",
      output_buffer_);
  CheckStatForShowAds(1);
}

TEST_F(MakeShowAdsAsyncFilterTest, ShowAdsNoParent) {
  add_tags_ = false;
  ValidateExpected(
      test_info_->name(),
      StrCat(GetShowAdsDataSnippetWithContent(GetShowAdsDataContent1()),
             kShowAdsApiCall),
      GetShowAdsDataFormat1Output());
  CheckStatForShowAds(1);
}

}  // namespace
}  // namespace net_instaweb
