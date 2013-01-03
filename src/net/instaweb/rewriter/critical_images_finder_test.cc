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
// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/rewriter/public/critical_images_finder.h"

#include <map>

#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

// Provide stub implementation of abstract base class for testing purposes.
class CriticalImagesFinderMock : public CriticalImagesFinder {
 public:
  explicit CriticalImagesFinderMock(Statistics* stats)
      : CriticalImagesFinder(stats) {}

  // Provide stub instantions for pure virtual functions
  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver) {}

  virtual const char* GetCriticalImagesCohort() const {
    return kCriticalImagesCohort;
  }

  virtual bool IsMeaningful() const {
    return false;
  }
 private:
  static const char kCriticalImagesCohort[];
};

const char CriticalImagesFinderMock::kCriticalImagesCohort[] =
    "critical_images";

class CriticalImagesFinderTest : public CriticalImagesFinderTestBase {
 public:
  virtual CriticalImagesFinder* finder() { return finder_.get(); }

 protected:
  virtual void SetUp() {
    CriticalImagesFinderTestBase::SetUp();
    finder_.reset(new CriticalImagesFinderMock(statistics()));
    SetupCohort(page_property_cache(), finder()->GetCriticalImagesCohort());
    ResetDriver();
  }

  void CheckCriticalImageFinderStats(int hits, int expiries, int not_found) {
    EXPECT_EQ(hits, statistics()->GetVariable(
        CriticalImagesFinder::kCriticalImagesValidCount)->Get());
    EXPECT_EQ(expiries, statistics()->GetVariable(
        CriticalImagesFinder::kCriticalImagesExpiredCount)->Get());
    EXPECT_EQ(not_found, statistics()->GetVariable(
        CriticalImagesFinder::kCriticalImagesNotFoundCount)->Get());
  }

 private:
  scoped_ptr<CriticalImagesFinderMock> finder_;
};

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySuccess) {
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  critical_images_set->insert("imageA.jpeg");
  StringSet* css_critical_images_set = new StringSet;
  css_critical_images_set->insert("imageB.jpeg");
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  EXPECT_TRUE(GetCssCriticalImagesUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntrySuccessEmptySet) {
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  StringSet* css_critical_images_set = new StringSet;
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  EXPECT_TRUE(GetCssCriticalImagesUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySetNULL) {
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), NULL, NULL));
  EXPECT_FALSE(GetCriticalImagesUpdatedValue()->has_value());
  EXPECT_FALSE(GetCssCriticalImagesUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntryPropertyPageMissing) {
  // No cache insert if PropertyPage is not set in RewriteDriver.
  rewrite_driver()->set_property_page(NULL);
  // Include an actual value in the RPC result to induce a cache write. We
  // expect no writes, but not from a lack of results!
  StringSet* critical_images_set = new StringSet;
  StringSet* css_critical_images_set = new StringSet;
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  EXPECT_EQ(NULL, GetCriticalImagesUpdatedValue());
  EXPECT_EQ(NULL, GetCssCriticalImagesUpdatedValue());
}

TEST_F(CriticalImagesFinderTest, GetCriticalImagesTest) {
  // First it will insert the value in cache, then it retrieves critical images.
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  critical_images_set->insert("imageA.jpeg");
  critical_images_set->insert("imageB.jpeg");
  StringSet* css_critical_images_set = new StringSet;
  css_critical_images_set->insert("imageD.jpeg");

  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  CheckCriticalImageFinderStats(0, 0, 1);
  ClearStats();

  // Call update again without resetting updated_critical_images. The stats do
  // not get updated.
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  CheckCriticalImageFinderStats(0, 0, 0);
  ClearStats();

  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  const PropertyCache::Cohort* cohort = page_property_cache()->GetCohort(
      finder()->GetCriticalImagesCohort());
  // Write the updated value to the pcache.
  page_property_cache()->WriteCohort(
      cohort, rewrite_driver()->property_page());
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  EXPECT_TRUE(GetCssCriticalImagesUpdatedValue()->has_value());

  // critical_images() is NULL because there is no previous call to
  // GetCriticalImages()
  EXPECT_TRUE(rewrite_driver()->critical_images() == NULL);
  ResetDriver();
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  EXPECT_FALSE(rewrite_driver()->must_compute_finder_properties());
  CheckCriticalImageFinderStats(1, 0, 0);
  ClearStats();

  // GetCriticalImages() upates critical_images set in RewriteDriver().
  const StringSet* critical_images = rewrite_driver()->critical_images();
  EXPECT_TRUE(critical_images != NULL);
  EXPECT_EQ(2, critical_images->size());
  EXPECT_TRUE(finder()->IsCriticalImage("imageA.jpeg", rewrite_driver()));
  EXPECT_TRUE(finder()->IsCriticalImage("imageB.jpeg", rewrite_driver()));
  EXPECT_FALSE(finder()->IsCriticalImage("imageC.jpeg", rewrite_driver()));

  const StringSet* css_critical_images =
      rewrite_driver()->css_critical_images();
  EXPECT_TRUE(css_critical_images != NULL);
  EXPECT_EQ(1, css_critical_images->size());
  EXPECT_TRUE(
      css_critical_images->find("imageD.jpeg") != css_critical_images->end());
  EXPECT_TRUE(
      css_critical_images->find("imageA.jpeg") == css_critical_images->end());

  // Reset the driver, read the page and call UpdateCriticalImagesSetInDriver.
  // We read it from cache.
  ResetDriver();
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  EXPECT_FALSE(rewrite_driver()->must_compute_finder_properties());
  CheckCriticalImageFinderStats(1, 0, 0);
  ClearStats();

  // Advance to 90% of expiry. We get a hit from cache and must_compute is true.
  AdvanceTimeMs(0.9 * options()->finder_properties_cache_expiration_time_ms());
  ResetDriver();
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  EXPECT_TRUE(rewrite_driver()->must_compute_finder_properties());
  CheckCriticalImageFinderStats(1, 0, 0);
  ClearStats();

  ResetDriver();
  // Advance past expiry, so that the pages expire.
  AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
  rewrite_driver()->set_updated_critical_images(false);
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  EXPECT_TRUE(rewrite_driver()->must_compute_finder_properties());
  CheckCriticalImageFinderStats(0, 1, 0);
}

}  // namespace net_instaweb
