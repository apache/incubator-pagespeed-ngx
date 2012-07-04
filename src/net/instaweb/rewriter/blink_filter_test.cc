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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/blink_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/blink_critical_line_data.pb.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;

namespace {

const char kRequestUrl[] = "http://www.test.com";

const char kRequestUrlWithPath[] = "http://www.test.com/path";

const char kHtmlInput[] =
    "<html>"
    "<body>\n"
    "<noscript></noscript>"
    "<div class=\"An \t \r \n item\"></div>"
    "<div> abcd"
      "<span class=\"Item again\"></span>"
    "</div>"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<div><span class=\"item\"></span></div>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
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
    "</div>"
    "</body></html>";

const char kJsonExpectedOutput[] =
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"__psa_lt;div class=\\\"An \\t \\r \\n item\\\"__psa_gt;__psa_lt;/div__psa_gt;\",\"xpath\":\"//div[1]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.1\":{\"instance_html\":\"__psa_lt;span class=\\\"Item again\\\"__psa_gt;__psa_lt;/span__psa_gt;\",\"xpath\":\"//div[2]/span[1]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.2\":{\"instance_html\":\"__psa_lt;span class=\\\"item\\\"__psa_gt;__psa_lt;/span__psa_gt;\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[1]/span[1]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"__psa_lt;h2 id=\\\"beforeItems\\\"__psa_gt; This is before Items __psa_lt;/h2__psa_gt;\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[2]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.3\":{\"instance_html\":\"__psa_lt;div class=\\\"another item here\\\"__psa_gt;__psa_lt;img src=\\\"image1\\\"__psa_gt;__psa_lt;img src=\\\"image2\\\"__psa_gt;__psa_lt;/div__psa_gt;\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.4\":{\"instance_html\":\"__psa_lt;div class=\\\"item\\\"__psa_gt;__psa_lt;img src=\\\"image3\\\"__psa_gt;__psa_lt;div class=\\\"item\\\"__psa_gt;__psa_lt;img src=\\\"image4\\\"__psa_gt;__psa_lt;/div__psa_gt;__psa_lt;/div__psa_gt;\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[4]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.bufferNonCriticalData(non_critical_json);</script>"
    "\n</body></html>\n";

class MockPage : public PropertyPage {
 public:
  MockPage(AbstractMutex* mutex, const StringPiece& key)
      : PropertyPage(mutex, key) {}
  virtual ~MockPage() {}
  virtual void Done(bool valid) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPage);
};

}  // namespace

class BlinkFilterTest : public ResourceManagerTestBase {
 public:
  BlinkFilterTest() {}

  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions();
    options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);
    options_->set_passthrough_blink_for_last_invalid_response_code(true);

    ResourceManagerTestBase::SetUp();

    rewrite_driver()->SetWriter(&write_to_string_);
    blink_filter_ = new BlinkFilter(rewrite_driver());
    html_writer_filter_.reset(blink_filter_);
    rewrite_driver()->AddFilter(html_writer_filter_.get());

    response_headers_.set_status_code(HttpStatus::kOK);
    response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 0);
    response_headers_.Add(HttpAttributes::kSetCookie, "helo=world; path=/");
    rewrite_driver()->set_response_headers_ptr(&response_headers_);

    PopulatePropertyCache();
  }

  void PopulatePropertyCache() {
    PropertyCache* property_cache = factory_->page_property_cache();
    property_cache->set_enabled(true);
    property_cache->AddCohort(BlinkFilter::kBlinkCohort);
    property_cache->AddCohort(RewriteDriver::kDomCohort);

    MockPage* page = new MockPage(factory_->thread_system()->NewMutex(),
                                  kRequestUrl);
    rewrite_driver()->set_property_page(page);
    property_cache->Read(page);
  }

  void WriteBlinkCriticalLineData(const char* last_modified_value) {
    PropertyCache* property_cache = factory_->page_property_cache();
    PropertyPage* page = rewrite_driver()->property_page();

    BlinkCriticalLineData response;
    response.set_url(kRequestUrl);
    response.set_non_critical_json("non_critical_json");
    if (last_modified_value != NULL) {
      response.set_last_modified_date(last_modified_value);
    }
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        BlinkFilter::kBlinkCohort);

    if (cohort == NULL || page == NULL) {
      LOG(ERROR) << "PropertyPage or Cohort NULL";
    }

    GoogleString buf;
    response.SerializeToString(&buf);
    PropertyValue* property_value = page->GetProperty(
        cohort, BlinkFilter::kBlinkCriticalLineDataPropertyName);
    property_cache->UpdateValue(buf, property_value);
    property_cache->WriteCohort(cohort, page);

    EXPECT_EQ(1, lru_cache()->num_inserts());
  }

  void CheckResponseCodeInPropertyCache(const int expected_code) {
    PropertyCache* property_cache = factory_->page_property_cache();
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        RewriteDriver::kDomCohort);
    PropertyValue* value = rewrite_driver()->property_page()->GetProperty(
      cohort, BlinkUtil::kBlinkResponseCodePropertyName);
    int code;
    ASSERT_TRUE(StringToInt(value->value().as_string(), &code));
    EXPECT_EQ(expected_code, code);
  }

  void CheckNoResponseCodeInPropertyCache() {
    PropertyCache* property_cache = factory_->page_property_cache();
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        RewriteDriver::kDomCohort);
    PropertyValue* value = rewrite_driver()->property_page()->GetProperty(
      cohort, BlinkUtil::kBlinkResponseCodePropertyName);
    EXPECT_FALSE(value->has_value());
  }

  bool IsBlinkCriticalLineDataInPropertyCache() {
    PropertyCache* property_cache = factory_->page_property_cache();
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        BlinkFilter::kBlinkCohort);
    PropertyValue* value = rewrite_driver()->property_page()->GetProperty(
        cohort, BlinkFilter::kBlinkCriticalLineDataPropertyName);
    return value != NULL && value->has_value();
  }

  virtual bool AddHtmlTags() const { return false; }

 protected:
  BlinkFilter* blink_filter_;
  ResponseHeaders response_headers_;
  GoogleString last_modified_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkFilterTest);
};

TEST_F(BlinkFilterTest, SendNonCritical404) {
  WriteBlinkCriticalLineData(NULL);
  response_headers_.set_status_code(HttpStatus::kNotFound);
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  // The following is a little odd (the output does not like anything like a
  // 404!).
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, kJsonExpectedOutput);
  CheckResponseCodeInPropertyCache(HttpStatus::kNotFound);
}

TEST_F(BlinkFilterTest, SendNonCritical) {
  WriteBlinkCriticalLineData(NULL);
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=\"item\",id='beforeItems'");
  options_->set_serve_blink_non_critical(true);
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, kJsonExpectedOutput);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_TRUE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, SendNonCriticalDoNotWriteResponseCode) {
  WriteBlinkCriticalLineData(NULL);
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  options_->set_passthrough_blink_for_last_invalid_response_code(false);
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, kJsonExpectedOutput);
  CheckNoResponseCodeInPropertyCache();
  EXPECT_TRUE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, SendNonCriticalWithMultipleFamilies) {
  WriteBlinkCriticalLineData(NULL);
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:id=random;/path:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  ValidateExpectedUrl(kRequestUrlWithPath, kHtmlInput, kJsonExpectedOutput);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_TRUE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, SendOnlyCookies) {
  WriteBlinkCriticalLineData(NULL);
  GoogleString json_expected_output =
      "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);"
      "</script>"
      "\n</body></html>\n";
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, json_expected_output);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_TRUE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, RequestLastModifiedNotInCache) {
  WriteBlinkCriticalLineData(NULL);
  response_headers_.Add(kPsaLastModified, "dummy");
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, kJsonExpectedOutput);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_FALSE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, RequestLastModifiedSameInCacheSendNonCritical) {
  WriteBlinkCriticalLineData("old");
  response_headers_.Add(kPsaLastModified, "old");
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, kJsonExpectedOutput);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_TRUE(IsBlinkCriticalLineDataInPropertyCache());
}


TEST_F(BlinkFilterTest, RequestLastModifiedDifferentFromCache1) {
  WriteBlinkCriticalLineData("old");
  response_headers_.Add(kPsaLastModified, "changed");
  options_->set_prioritize_visible_content_non_cacheable_elements(
      "/:class=item,id=beforeItems");
  options_->set_serve_blink_non_critical(true);
  GoogleString json_expected_output = StrCat(BlinkFilter::kRefreshPageJs,
                                             "\n</body></html>\n");
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, json_expected_output);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_FALSE(IsBlinkCriticalLineDataInPropertyCache());
}

TEST_F(BlinkFilterTest, RequestLastModifiedDifferentFromCache2) {
  WriteBlinkCriticalLineData("old");
  response_headers_.Add(kPsaLastModified, "changed");
  GoogleString json_expected_output = StrCat(BlinkFilter::kRefreshPageJs,
                                             "\n</body></html>\n");
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, json_expected_output);
  CheckResponseCodeInPropertyCache(HttpStatus::kOK);
  EXPECT_FALSE(IsBlinkCriticalLineDataInPropertyCache());
}

}  // namespace net_instaweb
