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
// Author: slamm@google.com (Stephen Lamm)

#include "net/instaweb/rewriter/public/critical_css_finder.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

const char kFallbackUrl[] = "http://www.test.com?a=b";

// Provide stub implementation of abstract base class for testing purposes.
// Note: Not using the implementation from mock_critical_css_finder.h, so that
// we can test with property cache.
class TestCriticalCssFinder : public CriticalCssFinder {
 public:
  TestCriticalCssFinder(Statistics* stats, const PropertyCache::Cohort* cohort)
      : CriticalCssFinder(cohort, stats) {}

  // Provide stub instantions for pure virtual functions.
  virtual void ComputeCriticalCss(RewriteDriver* driver) {}
};

}  // namespace

const char kCriticalCssCohort[] = "critical_css";

class CriticalCssFinderTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetupCohort(page_property_cache(), kCriticalCssCohort);
    finder_.reset(new TestCriticalCssFinder(
        statistics(), page_property_cache()->GetCohort(kCriticalCssCohort)));
    ResetDriver();
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    PropertyCache* pcache = server_context_->page_property_cache();
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    MockPropertyPage* page_with_fallback_values =
        NewMockPage(kFallbackUrl);
    rewrite_driver()->set_fallback_property_page(
        new FallbackPropertyPage(page, page_with_fallback_values));
    pcache->Read(page_with_fallback_values);
    pcache->Read(page);
  }

  const PropertyValue* GetUpdatedValue() {
    AbstractPropertyPage* page = rewrite_driver()->property_page();
    const PropertyCache::Cohort* cohort = finder_->cohort();
    if (cohort != NULL && page != NULL) {
      return page->GetProperty(cohort,
                               CriticalCssFinder::kCriticalCssPropertyName);
    }
    return NULL;
  }

  void CheckCriticalCssFinderStats(int hits, int expiries, int not_found) {
    EXPECT_EQ(hits, statistics()->GetVariable(
        CriticalCssFinder::kCriticalCssValidCount)->Get());
    EXPECT_EQ(expiries, statistics()->GetVariable(
        CriticalCssFinder::kCriticalCssExpiredCount)->Get());
    EXPECT_EQ(not_found, statistics()->GetVariable(
        CriticalCssFinder::kCriticalCssNotFoundCount)->Get());
  }

 protected:
  scoped_ptr<TestCriticalCssFinder> finder_;
  static const char kRequestUrl[];
};

const char CriticalCssFinderTest::kRequestUrl[] = "http://www.test.com";

TEST_F(CriticalCssFinderTest, UpdateCacheOnSuccess) {
  // Include an actual value in the RPC result to induce a cache write.
  CriticalCssResult result;
  CriticalCssResult_LinkRules* link_rules = result.add_link_rules();
  link_rules->set_link_url(GoogleString("http://test.com/a.css"));
  link_rules->set_critical_rules(GoogleString("a_critical {color: black;}"));
  link_rules->set_original_size(100);
  EXPECT_TRUE(finder_->UpdateCache(rewrite_driver(), result));
  // Property present in actual page.
  EXPECT_TRUE(GetUpdatedValue()->has_value());
  // Property present in page containing fallback values.
  EXPECT_TRUE(rewrite_driver()->fallback_property_page()->GetFallbackProperty(
      finder_->cohort(), CriticalCssFinder::kCriticalCssPropertyName));
}

TEST_F(CriticalCssFinderTest,
       UpdateCriticalCssCacheEntrySuccessEmptySet) {
  // Include an actual value in the RPC result to induce a cache write.
  CriticalCssResult result;
  EXPECT_TRUE(finder_->UpdateCache(rewrite_driver(), result));
  EXPECT_TRUE(GetUpdatedValue()->has_value());
}

TEST_F(CriticalCssFinderTest,
       UpdateCriticalCssCacheEntryPropertyPageMissing) {
  // No cache insert if PropertyPage is not set in RewriteDriver.
  rewrite_driver()->set_property_page(NULL);
  CriticalCssResult result;
  EXPECT_FALSE(finder_->UpdateCache(rewrite_driver(), result));
  EXPECT_EQ(NULL, GetUpdatedValue());
}

TEST_F(CriticalCssFinderTest, CheckCacheHandling) {
  {
    scoped_ptr<CriticalCssResult> result(
        finder_->GetCriticalCssFromCache(rewrite_driver()));
    EXPECT_TRUE(result == NULL);
    CheckCriticalCssFinderStats(0, 0, 1);  // hits, expiries, not_found
    ClearStats();
  }

  GoogleString result_str;
  {
    CriticalCssResult result;

    // Insert a rewritten url.
    CriticalCssResult_LinkRules* link_rules = result.add_link_rules();
    link_rules->set_link_url(
        GoogleString("http://test.com/I.b.css.pagespeed.cf.0.css"));
    link_rules->set_critical_rules(
        GoogleString("b_critical {color: black }"));
    link_rules->set_original_size(999);

    link_rules = result.add_link_rules();
    link_rules->set_link_url(
        GoogleString("http://test.com/c.css"));
    link_rules->set_critical_rules(
        GoogleString("c_critical {color: cyan }"));
    link_rules->set_original_size(100);

    result.SerializeToString(&result_str);

    EXPECT_TRUE(finder_->UpdateCache(rewrite_driver(), result));
    // Write the updated value for both actual property page and page with
    // fallback values to the pcache.
    rewrite_driver()->property_page()->WriteCohort(finder_->cohort());
    EXPECT_TRUE(GetUpdatedValue()->has_value());
    // Property present in page containing fallback values.
    EXPECT_TRUE(rewrite_driver()->fallback_property_page()->GetFallbackProperty(
        finder_->cohort(), CriticalCssFinder::kCriticalCssPropertyName));
  }

  {
    ResetDriver();
    scoped_ptr<CriticalCssResult> cached_result(
        finder_->GetCriticalCssFromCache(rewrite_driver()));
    EXPECT_TRUE(cached_result.get() != NULL);
    EXPECT_EQ(2, cached_result->link_rules_size());
    GoogleString cached_result_str;
    EXPECT_TRUE(cached_result->SerializeToString(&cached_result_str));
    EXPECT_EQ(result_str, cached_result_str);
    CheckCriticalCssFinderStats(1, 0, 0);  // hits, expiries, not_found
    ClearStats();
  }

  {
    // Advance past expiry. Result is unavailable.
    ResetDriver();
    AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
    scoped_ptr<CriticalCssResult> cached_result(
        finder_->GetCriticalCssFromCache(rewrite_driver()));
    EXPECT_TRUE(cached_result.get() == NULL);
    CheckCriticalCssFinderStats(0, 1, 0);  // hits, expiries, not_found
  }
}

TEST_F(CriticalCssFinderTest, EmptyResultWritesValueToCache) {
  CriticalCssResult result;
  EXPECT_TRUE(finder_->UpdateCache(rewrite_driver(), result));
  // Write the updated value to the pcache.
  rewrite_driver()->property_page()->WriteCohort(finder_->cohort());
  EXPECT_TRUE(GetUpdatedValue()->has_value());
}

}  // namespace net_instaweb
