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
// Author: chenyu@google.com (Yu Chen)

#include "pagespeed/opt/ads/show_ads_snippet_parser.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
namespace ads_attribute {
namespace {

class ShowAdsSnippetParserTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ::testing::Test::SetUp();
    parsed_attributes_.clear();
  }

  void CheckParsedResults() {
    EXPECT_EQ(4, parsed_attributes_.size());
    EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
    EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
    EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
    EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
  }

  ShowAdsSnippetParser parser_;
  std::map<GoogleString, GoogleString> parsed_attributes_;
};

TEST_F(ShowAdsSnippetParserTest, ParseStrictEmpty) {
  EXPECT_TRUE(parser_.ParseStrict("", &parsed_attributes_));

  EXPECT_EQ(0, parsed_attributes_.size());
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValid) {
  EXPECT_TRUE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidSingleQuote) {
  EXPECT_TRUE(parser_.ParseStrict(
      "google_ad_client = 'ca-pub-xxxxxxxxxxxxxx';"
      "/* ad served */"
      "google_ad_slot = 'xxxxxxxxx';"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidEmptyLines) {
  EXPECT_TRUE(parser_.ParseStrict(
      "\n\n\n\n\n"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";\n\n\n\n"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidEmptyStatement) {
  EXPECT_TRUE(parser_.ParseStrict(
      "\n\n\n\n\n"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";;;;;"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidWithoutSemicolon) {
  EXPECT_TRUE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\"\n"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\"\n"
      "google_ad_width = 728\n"
      "google_ad_height = 90\n",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidWithEnclosingCommentTag) {
  EXPECT_TRUE(parser_.ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictValidWithEnclosingCommentTagAndWhitespaces) {
  EXPECT_TRUE(parser_.ParseStrict(
      "    <!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->    ",
      &parsed_attributes_));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormat) {
  EXPECT_TRUE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"728x90\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));

  EXPECT_EQ(5, parsed_attributes_.size());
  EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
  EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
  EXPECT_EQ("728x90", parsed_attributes_["google_ad_format"]);
  EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
  EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormatWithWhiteSpaces) {
  EXPECT_TRUE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"  728x90  \";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));

  EXPECT_EQ(5, parsed_attributes_.size());
  EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
  EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
  EXPECT_EQ("  728x90  ", parsed_attributes_["google_ad_format"]);
  EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
  EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictGoogleAdFormatWithUnexpectedPrefix) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"test_722x92\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictGoogleAdFormatWithUnexpectedSuffix) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"722x92_rimg\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormatWithUnexpectedEnds) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"test_722x92_rimg\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictInvalidAttributeNameNotStartedWithGoogle) {
  EXPECT_FALSE(parser_.ParseStrict(
      "<!--"
      "dgoogle_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"  // Invalid.
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidAttributeNameIllegalChar) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_invalid-name = \"ca-pub-xxxxxxxxxxxxxx\";"  // Invalid.
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidDuplicate) {
  EXPECT_FALSE(parser_.ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_slot = \"xxxxxxxxy\";"  // Duplicate assignment
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidMissingSemicolon) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\" "  // ; or \n is missing
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\"\n"
      "google_ad_width = 728\n"
      "google_ad_height = 90\n",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidModified) {
  EXPECT_FALSE(parser_.ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "if (test) google_ad_client = \"ca-pub-xxxxxxxxxxxxxy\";"  // Invalid
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->",
      &parsed_attributes_));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidAssignment) {
  EXPECT_FALSE(parser_.ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = google_ad_width;",
      &parsed_attributes_));
}

}  // namespace
}  // namespace ads_attribute
}  // namespace net_instaweb
