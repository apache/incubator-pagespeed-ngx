/*
 * Copyright 2012 Google Inc.
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

// Authors: mmohabey@google.com (Megha Mohabey)
//          rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/cache_html_filter.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const int64 kCacheTimeMs =
    RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs;

const char kRequestUrl[] = "http://www.test.com";

const char kRequestUrlWithPath[] = "http://www.test.com/path";

const char kHtmlInput[] =
    "<html>"
    "<body>\n"
    "<noscript></noscript>"
    "<div CLASS=\"An \t \r \n item\">"
      "<script></script>"
    "</div>"
    "<div> abcd"
      "<span class=\"Item again\"></span>"
    "</div>"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<div><span class=\"item\"></span></div>"
      "<h2 Id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"another item here\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>";

const char kExpectedOutput[] =
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"<div CLASS=\\\"An \\t \\r \\n item\\\"><script><\\/script></div>\",\"xpath\":\"//div[2]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.1\":{\"instance_html\":\"<span class=\\\"Item again\\\"></span>\",\"xpath\":\"//div[3]/span[1]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.2\":{\"instance_html\":\"<span class=\\\"item\\\"></span>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[1]/span[1]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"<h2 Id=\\\"beforeItems\\\"> This is before Items </h2>\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[2]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.3\":{\"instance_html\":\"<div class=\\\"another item here\\\"><img src=\\\"image1\\\"><img src=\\\"image2\\\"></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.4\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image3\\\"><div class=\\\"item\\\"><img src=\\\"image4\\\"></div></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[4]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.bufferNonCriticalData({});</script>";

}  // namespace

class CacheHtmlFilterTest : public RewriteTestBase {
 public:
  CacheHtmlFilterTest() {}

  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions(factory()->thread_system());
    options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);

    RewriteTestBase::SetUp();

    rewrite_driver()->SetWriter(&write_to_string_);
    cache_html_filter_ = new CacheHtmlFilter(rewrite_driver());
    html_writer_filter_.reset(cache_html_filter_);
    rewrite_driver()->AddFilter(html_writer_filter_.get());

    response_headers_.set_status_code(HttpStatus::kOK);
    response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 0);
    response_headers_.Add(HttpAttributes::kSetCookie, "helo=world; path=/");
    rewrite_driver()->set_response_headers_ptr(&response_headers_);

    PopulatePropertyCache();
  }

  void PopulatePropertyCache() {
    PropertyCache* property_cache = page_property_cache();
    property_cache->set_enabled(true);
    SetupCohort(property_cache, BlinkUtil::kBlinkCohort);
    SetupCohort(property_cache, RewriteDriver::kDomCohort);

    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    property_cache->Read(page);
  }

  void Validate(const StringPiece& url, const GoogleString& html_input,
                const GoogleString& expected) {
    ParseUrl(url, html_input);
    EXPECT_EQ(expected, output_buffer_) << "Test url:" << url;
    output_buffer_.clear();
  }

  bool IsCacheHtmlInfoInPropertyCache() {
    PropertyCache* property_cache = page_property_cache();
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        BlinkUtil::kBlinkCohort);
    PropertyValue* value = rewrite_driver()->property_page()->GetProperty(
        cohort, BlinkUtil::kCacheHtmlRewriterInfo);
    return value != NULL && value->has_value();
  }

 protected:
  CacheHtmlFilter* cache_html_filter_;
  ResponseHeaders response_headers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheHtmlFilterTest);
};

TEST_F(CacheHtmlFilterTest, SendNonCacheable) {
  options_->set_non_cacheables_for_cache_partial_html(
      "class=\"item\",id='beforeItems'");
  Validate(kRequestUrl, kHtmlInput, kExpectedOutput);
}

TEST_F(CacheHtmlFilterTest, SendNonCacheableWithMultipleFamilies) {
  options_->set_non_cacheables_for_cache_partial_html(
      "class=item,id=beforeItems");
  Validate(kRequestUrlWithPath, kHtmlInput, kExpectedOutput);
}

TEST_F(CacheHtmlFilterTest, SendOnlyCookies) {
  rewrite_driver()->set_flushed_cached_html(false);
  GoogleString json_expected_output =
      "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);"
      "</script>"
      "<script>pagespeed.panelLoader.bufferNonCriticalData({});</script>";
  Validate(kRequestUrl, kHtmlInput, json_expected_output);
}

}  // namespace net_instaweb
