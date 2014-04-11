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
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/gmock.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "pagespeed/kernel/base/mock_timer.h"

using testing::HasSubstr;
using testing::Not;

namespace {

const char kChefGifFile[] = "IronChef2.gif";
// Set image dimensions such that image will be inlined.
const char kChefGifDims[] = "width=48 height=64";

const char kRequestUrl[] = "http://www.example.com";

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
    options()->EnableFilter(RewriteOptions::kDelayImages);
    RewriteTestBase::SetUp();
    https_mode_ = false;
    // Setup the property cache. The DetermineEnable logic for the
    // CriticalImagesBeaconFinder will only inject the beacon if the property
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

    GoogleUrl base(GetTestUrl());
    image_gurl_.Reset(base, kChefGifFile);
    ResetDriver();
    SetDummyRequestHeaders();
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->SetUserAgent(
        UserAgentMatcherTestBase::kChrome18UserAgent);
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    PropertyCache* pcache = server_context_->page_property_cache();
    pcache->Read(page);
  }

  void WriteToPropertyCache() {
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
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
    SetupAndProcessUrl();
  }

  void SetupAndProcessUrl() {
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

  void VerifyInjection(int expected_beacon_count) {
    EXPECT_EQ(expected_beacon_count, statistics()->GetVariable(
        CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount)->Get());
    EXPECT_THAT(output_buffer_, HasSubstr(CreateInitString()));
  }

  void VerifyNoInjection(int expected_beacon_count) {
    EXPECT_EQ(expected_beacon_count, statistics()->GetVariable(
        CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount)->Get());
    EXPECT_THAT(output_buffer_, Not(HasSubstr("pagespeed.CriticalImages.Run")));
  }

  void VerifyWithNoImageRewrite() {
    const GoogleString hash_str = ImageUrlHash(kChefGifFile);
    EXPECT_THAT(output_buffer_,
                HasSubstr(StrCat("pagespeed_url_hash=\"", hash_str)));
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
    bool lazyload_will_run_beacon =
        rewrite_driver()->options()->Enabled(RewriteOptions::kLazyloadImages) &&
        LazyloadImagesFilter::ShouldApply(rewrite_driver()) ==
            RewriterHtmlApplication::ACTIVE;
    GoogleString str = "pagespeed.CriticalImages.Run(";
    StrAppend(&str, "'", beacon_url, "',");
    StrAppend(&str, "'", url, "',");
    StrAppend(&str, "'", options_signature_hash, "',");
    StrAppend(&str, BoolToString(!lazyload_will_run_beacon), ",");
    StrAppend(&str, BoolToString(rewrite_driver()->options()->Enabled(
                        RewriteOptions::kResizeToRenderedImageDimensions)),
              ",");
    StrAppend(&str, "'", ExpectedNonce(), "');");
    return str;
  }

  bool https_mode_;
  GoogleUrl image_gurl_;
};

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjection) {
  RunInjection();
  VerifyInjection(1);

  // Verify that image onload criticality check has been added.
  int img_begin = output_buffer_.find("IronChef2");
  EXPECT_TRUE(img_begin != GoogleString::npos);
  int img_end = output_buffer_.substr(img_begin).find(">");
  EXPECT_TRUE(img_end != GoogleString::npos);
  EXPECT_TRUE(output_buffer_.substr(img_begin, img_end).find(
      "onload=\"pagespeed.CriticalImages."
      "checkImageForCriticality(this);\"") !=
      GoogleString::npos);
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjectionNoBody) {
  RunInjectionNoBody();
  VerifyInjection(1);
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, ScriptInjectionWithHttps) {
  AssumeHttps();
  RunInjection();
  VerifyInjection(1);
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
  options()->DisableFilter(RewriteOptions::kDelayImages);
  RunInjection();
  VerifyInjection(1);

  EXPECT_TRUE(output_buffer_.find("data:") != GoogleString::npos);
  EXPECT_TRUE(output_buffer_.find(hash_str) != GoogleString::npos);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
}

TEST_F(CriticalImagesBeaconFilterTest, NoScriptInjectionWithNoScript) {
  PrepareInjection();
  GoogleString html = "<head></head><body><noscript>";
  AddImageTags(&html);
  StrAppend(&html, "</noscript></body>");
  ParseUrl(GetTestUrl(), html);
  VerifyNoInjection(0);
  VerifyWithNoImageRewrite();
}

TEST_F(CriticalImagesBeaconFilterTest, DontRebeaconBeforeTimeout) {
  RunInjection();
  VerifyInjection(1);
  VerifyWithNoImageRewrite();

  // Write a dummy value to the property cache.
  WriteToPropertyCache();

  // No beacon injection happens on the immediately succeeding request.
  ResetDriver();
  SetDummyRequestHeaders();
  SetupAndProcessUrl();
  VerifyNoInjection(1);

  // Beacon injection happens when the pcache value expires or when the
  // reinstrumentation time interval is exceeded.
  factory()->mock_timer()->AdvanceMs(
      options()->beacon_reinstrument_time_sec() * 1000);
  ResetDriver();
  SetDummyRequestHeaders();
  SetupAndProcessUrl();
  VerifyInjection(2);
}

TEST_F(CriticalImagesBeaconFilterTest, BeaconReinstrumentationWithHeader) {
  RunInjection();
  VerifyInjection(1);
  VerifyWithNoImageRewrite();

  // Write a dummy value to the property cache.
  WriteToPropertyCache();

  // Beacon injection happens when the PS-ShouldBeacon header is present even
  // when the pcache value has not expired and the reinstrumentation time
  // interval has not been exceeded.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", "random_rebeaconing_key");
  RequestHeaders new_request_headers;
  new_request_headers.Add(kPsaShouldBeacon, "random_rebeaconing_key");
  rewrite_driver()->SetRequestHeaders(new_request_headers);
  SetupAndProcessUrl();
  VerifyInjection(2);
}

TEST_F(CriticalImagesBeaconFilterTest, UnsupportedUserAgent) {
  // Test that the filter is not applied for unsupported user agents.
  rewrite_driver()->SetUserAgent("Firefox/1.0");
  RunInjection();
  VerifyNoInjection(0);
}

TEST_F(CriticalImagesBeaconFilterTest, Googlebot) {
  // Verify that the filter is not applied for bots.
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  RunInjection();
  VerifyNoInjection(0);
}

// Verify that the init string is set correctly to not run the beacon's onload
// handler when lazyload is enabled. The lazyload JS will take care of running
// the beacon when all images have been loaded.
TEST_F(CriticalImagesBeaconFilterTest, LazyloadEnabled) {
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  // On the first page access, there will be no critical image data and lazyload
  // will be disabled.
  RunInjection();
  VerifyInjection(1);

  // Advance time to force re-beaconing.  Now there are extant non-critical
  // images, and lazyload ought to be enabled.
  factory()->mock_timer()->AdvanceMs(
      options()->beacon_reinstrument_time_sec() * 1000);
  ResetDriver();
  SetupAndProcessUrl();
  VerifyInjection(2);
}

}  // namespace net_instaweb
