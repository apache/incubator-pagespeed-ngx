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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/url_partnership.h"

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kOriginalRequest[] = "http://www.nytimes.com/index.html";
const char kResourceUrl1[] = "r/styles/style.css?appearance=reader/writer?";
const char kResourceUrl2[] = "r/styles/style2.css?appearance=reader";
const char kResourceUrl3[] = "r/main.css";
const char kCdnResourceUrl[] = "http://graphics8.nytimes.com/styles/style.css";

// Resources 1-3 but specified absolutely
const char kAbsoluteResourceUrl1[] =
    "http://www.nytimes.com/r/styles/style.css?appearance=reader/writer?";
const char kAbsoluteResourceUrl2[] =
    "http://www.nytimes.com/r/styles/style2.css?appearance=reader";
const char kAbsoluteResourceUrl3[] = "http://www.nytimes.com/r/main.css";

}  // namespace


namespace net_instaweb {

class UrlPartnershipTest : public testing::Test {
 protected:
  UrlPartnershipTest()
      : domain_lawyer_(options_.domain_lawyer()),
        partnership_(&options_),
        styles_path_("http://www.nytimes.com/r/styles/"),
        r_path_("http://www.nytimes.com/r/"),
        style_url_("style.css?appearance=reader/writer?"),
        style2_url_("style2.css?appearance=reader") {
    GoogleUrl original_gurl(kOriginalRequest);
    partnership_.Reset(original_gurl);
  }

  // Add up to 3 URLs -- url2 and url3 are ignored if null.
  bool AddUrls(const char* url1, const char* url2, const char* url3) {
    bool ret = partnership_.AddUrl(url1, &message_handler_);
    if (url2 != NULL) {
      ret &= partnership_.AddUrl(url2, &message_handler_);
    }
    if (url3 != NULL) {
      ret &= partnership_.AddUrl(url3, &message_handler_);
    }
    return ret;
  }

  // Gets the full path of an index as a GoogleString.
  GoogleString FullPath(int index) {
    const GoogleUrl* gurl = partnership_.FullPath(index);
    StringPiece spec = gurl->Spec();
    return GoogleString(spec.data(), spec.size());
  }

  RewriteOptions options_;
  DomainLawyer* domain_lawyer_;
  UrlPartnership partnership_;
  GoogleString styles_path_;
  GoogleString r_path_;
  GoogleString style_url_;
  GoogleString style2_url_;
  GoogleMessageHandler message_handler_;
};

TEST_F(UrlPartnershipTest, OneUrlFlow) {
  ASSERT_TRUE(AddUrls(kResourceUrl1, NULL, NULL));
  ASSERT_EQ(1, partnership_.num_urls());
  EXPECT_EQ(styles_path_, partnership_.ResolvedBase());
  EXPECT_EQ(style_url_, partnership_.RelativePath(0));
  EXPECT_EQ(styles_path_ + style_url_, FullPath(0));
}

TEST_F(UrlPartnershipTest, OneUrlFlowAbsolute) {
  ASSERT_TRUE(AddUrls(kAbsoluteResourceUrl1, NULL, NULL));
  ASSERT_EQ(1, partnership_.num_urls());
  EXPECT_EQ(styles_path_, partnership_.ResolvedBase());
  EXPECT_EQ(style_url_, partnership_.RelativePath(0));
  EXPECT_EQ(styles_path_ + style_url_, FullPath(0));
}

TEST_F(UrlPartnershipTest, TwoUrlFlowSamePath) {
  AddUrls(kResourceUrl1, kResourceUrl2, NULL);
  ASSERT_EQ(2, partnership_.num_urls());
  EXPECT_EQ(styles_path_, partnership_.ResolvedBase());
  EXPECT_EQ(style_url_, partnership_.RelativePath(0));
  EXPECT_EQ(style2_url_, partnership_.RelativePath(1));
}

TEST_F(UrlPartnershipTest, TwoUrlFlowSamePathMixed) {
  AddUrls(kAbsoluteResourceUrl1, kResourceUrl2, NULL);
  ASSERT_EQ(2, partnership_.num_urls());
  EXPECT_EQ(styles_path_, partnership_.ResolvedBase());
  EXPECT_EQ(style_url_, partnership_.RelativePath(0));
  EXPECT_EQ(style2_url_, partnership_.RelativePath(1));
}

TEST_F(UrlPartnershipTest, ThreeUrlFlowDifferentPaths) {
  AddUrls(kResourceUrl1, kResourceUrl2, kResourceUrl3);
  ASSERT_EQ(3, partnership_.num_urls());
  EXPECT_EQ(r_path_, partnership_.ResolvedBase());
  // We add 2 to the expected values of the 3 kResourceUrl* below to
  // skip over the "r/".
  EXPECT_EQ(GoogleString(kResourceUrl1 + 2), partnership_.RelativePath(0));
  EXPECT_EQ(GoogleString(kResourceUrl2 + 2), partnership_.RelativePath(1));
  EXPECT_EQ(GoogleString(kResourceUrl3 + 2), partnership_.RelativePath(2));
}

TEST_F(UrlPartnershipTest, ThreeUrlFlowDifferentPathsAbsolute) {
  AddUrls(kAbsoluteResourceUrl1, kAbsoluteResourceUrl2, kAbsoluteResourceUrl3);
  ASSERT_EQ(3, partnership_.num_urls());
  EXPECT_EQ(r_path_, partnership_.ResolvedBase());
  // We add 2 to the expected values of the 3 kResourceUrl* below to
  // skip over the "r/".
  EXPECT_EQ(GoogleString(kResourceUrl1 + 2), partnership_.RelativePath(0));
  EXPECT_EQ(GoogleString(kResourceUrl2 + 2), partnership_.RelativePath(1));
  EXPECT_EQ(GoogleString(kResourceUrl3 + 2), partnership_.RelativePath(2));
}

TEST_F(UrlPartnershipTest, ThreeUrlFlowDifferentPathsMixed) {
  AddUrls(kAbsoluteResourceUrl1, kResourceUrl2, kAbsoluteResourceUrl3);
  ASSERT_EQ(3, partnership_.num_urls());
  EXPECT_EQ(r_path_, partnership_.ResolvedBase());
  // We add 2 to the expected values of the 3 kResourceUrl* below to
  // skip over the "r/".
  EXPECT_EQ(GoogleString(kResourceUrl1 + 2), partnership_.RelativePath(0));
  EXPECT_EQ(GoogleString(kResourceUrl2 + 2), partnership_.RelativePath(1));
  EXPECT_EQ(GoogleString(kResourceUrl3 + 2), partnership_.RelativePath(2));
}

TEST_F(UrlPartnershipTest, ExternalDomainNotDeclared) {
  EXPECT_FALSE(AddUrls(kCdnResourceUrl, NULL, NULL));
}

TEST_F(UrlPartnershipTest, ExternalDomainDeclared) {
  domain_lawyer_->AddDomain("http://graphics8.nytimes.com", &message_handler_);
  EXPECT_TRUE(partnership_.AddUrl(kCdnResourceUrl, &message_handler_));
}

TEST_F(UrlPartnershipTest, ExternalDomainDeclaredButNotMapped) {
  // This test shows that while we can start partnerships from nytimes.com
  // or graphics8.nytimes.com, we cannot combine those without a mapping.
  domain_lawyer_->AddDomain("http://graphics8.nytimes.com", &message_handler_);
  EXPECT_TRUE(partnership_.AddUrl(kCdnResourceUrl, &message_handler_));
  EXPECT_FALSE(partnership_.AddUrl(kResourceUrl1, &message_handler_));
}

TEST_F(UrlPartnershipTest, AbsExternalDomainDeclaredButNotMapped) {
  // This test shows that while we can start partnerships from nytimes.com
  // or graphics8.nytimes.com, we cannot combine those without a mapping.
  domain_lawyer_->AddDomain("http://graphics8.nytimes.com", &message_handler_);
  EXPECT_TRUE(partnership_.AddUrl(kCdnResourceUrl, &message_handler_));
  EXPECT_FALSE(partnership_.AddUrl(kAbsoluteResourceUrl1, &message_handler_));
}

TEST_F(UrlPartnershipTest, EmptyTail) {
  EXPECT_FALSE(partnership_.AddUrl("", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("http://www.nytimes.com",
                                  &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("http://www.nytimes.com/",
                                  &message_handler_));
}

TEST_F(UrlPartnershipTest, EmptyWithPartner) {
  GoogleUrl base_gurl("http://www.google.com/styles/x.html");
  UrlPartnership p(&options_, base_gurl);
  EXPECT_TRUE(p.AddUrl("/styles", &message_handler_));
  EXPECT_FALSE(p.AddUrl("", &message_handler_));
  EXPECT_TRUE(p.AddUrl("/", &message_handler_));
  EXPECT_TRUE(p.AddUrl("..", &message_handler_));
}

TEST_F(UrlPartnershipTest, NeedsATrim) {
  AddUrls(" http://www.nytimes.com/needs_a_trim.jpg ", NULL, NULL);
  EXPECT_EQ(GoogleString("needs_a_trim.jpg"), partnership_.RelativePath(0));
}

TEST_F(UrlPartnershipTest, RemoveLast) {
  AddUrls(kAbsoluteResourceUrl1, kAbsoluteResourceUrl2, kAbsoluteResourceUrl3);
  EXPECT_EQ(r_path_, partnership_.ResolvedBase());
  partnership_.RemoveLast();
  EXPECT_EQ(styles_path_, partnership_.ResolvedBase());
}

TEST_F(UrlPartnershipTest, ResourcesFromMappedDomains) {
  domain_lawyer_->AddRewriteDomainMapping(
      "http://graphics8.nytimes.com", "http://www.nytimes.com",
      &message_handler_);
  domain_lawyer_->AddRewriteDomainMapping(
      "http://graphics8.nytimes.com", "http://styles.com", &message_handler_);

  // We can legally combine resources across multiple domains if they are
  // all mapped together
  ASSERT_TRUE(AddUrls(kCdnResourceUrl, kResourceUrl1,
                      "http://styles.com/external.css"));
  EXPECT_EQ("http://graphics8.nytimes.com/", partnership_.ResolvedBase());
}

TEST_F(UrlPartnershipTest, AllowDisallow) {
  options_.Disallow("*/*.css");
  options_.Allow("*/a*.css");
  EXPECT_FALSE(partnership_.AddUrl("foo.css", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("afoo.css", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("foo.jpg", &message_handler_));
}

TEST_F(UrlPartnershipTest, CombineAcrossPaths) {
  options_.set_combine_across_paths(true);
  EXPECT_TRUE(partnership_.AddUrl("a/foo.css", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("b/bar.css", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("a/baz.css", &message_handler_));
}

TEST_F(UrlPartnershipTest, NoCombineAcrossPaths) {
  options_.set_combine_across_paths(false);
  EXPECT_TRUE(partnership_.AddUrl("a/foo.css", &message_handler_));
  EXPECT_FALSE(partnership_.AddUrl("b/bar.css", &message_handler_));
  EXPECT_TRUE(partnership_.AddUrl("a/baz.css", &message_handler_));
}

}  // namespace net_instaweb
