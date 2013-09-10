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

// Author: jud@google.com (Jud Porter)

#include "net/instaweb/rewriter/public/critical_images_beacon_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kChefGifFile[] = "IronChef2.gif";
// Set image dimensions such that image will be inlined.
const char kChefGifDims[] = "width=48 height=64";

}  // namespace

namespace net_instaweb {

class CriticalImagesBeaconFilterTest : public RewriteTestBase {
 protected:
  CriticalImagesBeaconFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon");
    CriticalImagesBeaconFilter::InitStats(statistics());
    // Enable a filter that uses critical images, which in turn will enable
    // beacon insertion.
    factory()->set_use_beacon_results_in_filters(true);
    options()->EnableFilter(RewriteOptions::kLazyloadImages);
    RewriteTestBase::SetUp();
    https_mode_ = false;
    // Setup the property cache. The DetermineEnable logic for the
    // CriticalIMagesBeaconFinder will only inject the beacon if the property
    // cache is enabled, since beaconed results are intended to be stored in the
    // pcache.
    PropertyCache* pcache = page_property_cache();
    server_context_->set_enable_property_cache(true);
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(pcache, RewriteDriver::kBeaconCohort);
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(pcache, RewriteDriver::kDomCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    server_context()->set_dom_cohort(dom_cohort);

    server_context()->set_critical_images_finder(
        new BeaconCriticalImagesFinder(
            beacon_cohort, factory()->nonce_generator(), statistics()));
    MockPropertyPage* page = NewMockPage("http://example.com");
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);

    GoogleUrl base(GetTestUrl());
    image_gurl_.Reset(base, kChefGifFile);
  }

  void PrepareInjection() {
    rewrite_driver()->AddFilters();
    AddFileToMockFetcher(image_gurl_.Spec(), kChefGifFile,
                         kContentTypeJpeg, 100);
  }

  void AddImageTags(GoogleString* html) {
    // Add the relative image URL.
    StrAppend(html, "<img src=\"", kChefGifFile, "\" ", kChefGifDims, ">");
    // Add the absolute image URL.
    StrAppend(html, "<img src=\"", image_gurl_.Spec(), "\" ",
              kChefGifDims, ">");
  }

  void RunInjection() {
    PrepareInjection();
    GoogleString html = "<head></head><body>";
    AddImageTags(&html);
    StrAppend(&html, "</body>");
    ParseUrl(GetTestUrl(), html);
  }

  void RunInjectionNoBody() {
    // As above, but we omit <head> and (more relevant) <body> tags.  We should
    // still inject the script at the end of the document.  The filter used to
    // get this wrong.
    PrepareInjection();
    GoogleString html;
    AddImageTags(&html);
    ParseUrl(GetTestUrl(), html);
  }

  void VerifyInjection() {
    EXPECT_EQ(1, statistics()->GetVariable(
        CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount)->Get());
    EXPECT_TRUE(output_buffer_.find(CreateInitString()) != GoogleString::npos);
  }

  void VerifyNoInjection() {
    EXPECT_EQ(0, statistics()->GetVariable(
        CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount)->Get());
    EXPECT_TRUE(output_buffer_.find("criticalImagesBeaconInit") ==
                GoogleString::npos);
  }

  void VerifyWithNoImageRewrite() {
    const GoogleString hash_str = ImageUrlHash(kChefGifFile);
    EXPECT_TRUE(output_buffer_.find(
        StrCat("pagespeed_url_hash=\"", hash_str)) != GoogleString::npos);
  }

  void AssumeHttps() {
    https_mode_ = true;
  }

  GoogleString GetTestUrl() {
    return StrCat((https_mode_ ? "https://example.com/" : kTestDomain),
                  "index.html?a&b");
  }

  GoogleString ImageUrlHash(StringPiece url) {
    // Absolutify the URL before hashing.
    unsigned int hash_val = HashString<CasePreserve, unsigned int>(
        image_gurl_.spec_c_str(), strlen(image_gurl_.spec_c_str()));
    return UintToString(hash_val);
  }


  GoogleString CreateInitString() {
    GoogleString url;
    EscapeToJsStringLiteral(rewrite_driver()->google_url().Spec(), false, &url);
    StringPiece beacon_url = https_mode_ ? options()->beacon_url().https :
        options()->beacon_url().http;
    GoogleString options_signature_hash =
        rewrite_driver()->server_context()->hasher()->Hash(
            rewrite_driver()->options()->signature());
    GoogleString str = "pagespeed.criticalImagesBeaconInit(";
    StrAppend(&str, "'", beacon_url, "',");
    StrAppend(&str, "'", url, "',");
    StrAppend(&str, "'", options_signature_hash, "',");
    StrAppend(&str, BoolToString(
        CriticalImagesBeaconFilter::IncludeRenderedImagesInBeacon(
            rewrite_driver())), ",");
    StrAppend(&str, "'", ExpectedNonce(), "');");
    return str;
  }

  bool https_mode_;
  GoogleUrl image_gurl_;
};

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjection) {
  RunInjection();
  VerifyInjection();
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjectionNoBody) {
  RunInjectionNoBody();
  VerifyInjection();
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjectionWithHttps) {
  AssumeHttps();
  RunInjection();
  VerifyInjection();
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjectionWithImageInlining) {
  // Verify that the URL hash is applied to the absolute image URL, and not to
  // the rewritten URL. In this case, make sure that an image inlined to a data
  // URI has the correct hash. We need to add the image hash to the critical
  // image set to make sure that the image is inlined.
  GoogleString hash_str = ImageUrlHash(kChefGifFile);
  StringSet* crit_img_set = server_context()->critical_images_finder()->
      mutable_html_critical_images(rewrite_driver());
  crit_img_set->insert(hash_str);
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->DisableFilter(RewriteOptions::kLazyloadImages);
  RunInjection();
  VerifyInjection();

  EXPECT_TRUE(output_buffer_.find("data:") != GoogleString::npos);
  EXPECT_TRUE(output_buffer_.find(hash_str) != GoogleString::npos);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
}

TEST_F(CriticalImagesBeaconFilterTest, UnsupportedUserAgent) {
  // Test that the filter is not applied for unsupported user agents.
  rewrite_driver()->SetUserAgent("Firefox/1.0");
  RunInjection();
  VerifyNoInjection();
}

}  // namespace net_instaweb
