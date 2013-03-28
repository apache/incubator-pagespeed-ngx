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

#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
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

  virtual bool IsMeaningful(const RewriteDriver* driver) const {
    return false;
  }
 private:
  static const char kCriticalImagesCohort[];
};

// Mock class for testing a critical image finder like the beacon finder that
// stores a history of previous critical image sets.
class CriticalImagesHistoryFinderMock : public CriticalImagesFinderMock {
 public:
  explicit CriticalImagesHistoryFinderMock(Statistics* stats)
      : CriticalImagesFinderMock(stats) {}

  virtual int PercentSeenForCritical() const {
    return 80;
  }

  virtual int NumSetsToKeep() const {
    return 10;
  }
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
  friend class CriticalImagesHistoryFinderTest;

  scoped_ptr<CriticalImagesFinderMock> finder_;
};

class CriticalImagesHistoryFinderTest : public CriticalImagesFinderTest {
 protected:
  virtual void SetUp() {
    CriticalImagesFinderTestBase::SetUp();
    finder_.reset(new CriticalImagesHistoryFinderMock(statistics()));
    SetupCohort(page_property_cache(), finder()->GetCriticalImagesCohort());
    ResetDriver();
  }
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
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntrySuccessEmptySet) {
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  StringSet* css_critical_images_set = new StringSet;
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  const PropertyCache::Cohort* cohort = page_property_cache()->GetCohort(
      finder()->GetCriticalImagesCohort());
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  rewrite_driver()->property_page()->WriteCohort(cohort);
}

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySetNULL) {
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), NULL, NULL));
  EXPECT_FALSE(GetCriticalImagesUpdatedValue()->has_value());
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
}

TEST_F(CriticalImagesFinderTest, GetCriticalImagesTest) {
  // First it will insert the value in cache, then it retrieves critical images.
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  critical_images_set->insert("imageA.jpeg");
  critical_images_set->insert("imageB.jpeg");
  StringSet* css_critical_images_set = new StringSet;
  css_critical_images_set->insert("imageD.jpeg");

  // Calling IsHtmlCriticalImage should update the CriticalImagesInfo in
  // RewriteDriver.
  finder()->IsHtmlCriticalImage("imageA.jpg", rewrite_driver());
  // We should get 1 miss for the critical images value.
  CheckCriticalImageFinderStats(0, 0, 1);
  EXPECT_EQ(0, logging_info()->num_html_critical_images());
  EXPECT_EQ(0, logging_info()->num_css_critical_images());
  ClearStats();

  // Calling IsHtmlCriticalImage again should not update the stats, because the
  // CriticalImagesInfo has already been updated.
  finder()->IsHtmlCriticalImage("imageA.jpg", rewrite_driver());
  CheckCriticalImageFinderStats(0, 0, 0);
  // ClearStats() creates a new request context and hence a new log record. So
  // the critical image counts are not set.
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
  ClearStats();

  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
      rewrite_driver(), critical_images_set, css_critical_images_set));
  const PropertyCache::Cohort* cohort = page_property_cache()->GetCohort(
      finder()->GetCriticalImagesCohort());
  // Write the updated value to the pcache.
  rewrite_driver()->property_page()->WriteCohort(cohort);
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());

  // critical_images() is NULL because there is no previous call to
  // GetCriticalImages()
  ResetDriver();
  EXPECT_TRUE(rewrite_driver()->critical_images_info() == NULL);
  EXPECT_TRUE(finder()->IsHtmlCriticalImage("imageA.jpeg", rewrite_driver()));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  // GetCriticalImages() updates critical_images set in RewriteDriver().
  EXPECT_TRUE(rewrite_driver()->critical_images_info() != NULL);
  // EXPECT_EQ(2, GetCriticalImages(rewrite_driver()).size());
  EXPECT_TRUE(finder()->IsHtmlCriticalImage("imageA.jpeg", rewrite_driver()));
  EXPECT_TRUE(finder()->IsHtmlCriticalImage("imageB.jpeg", rewrite_driver()));
  EXPECT_FALSE(finder()->IsHtmlCriticalImage("imageC.jpeg", rewrite_driver()));

  // EXPECT_EQ(1, css_critical_images->size());
  EXPECT_TRUE(finder()->IsCssCriticalImage("imageD.jpeg", rewrite_driver()));
  EXPECT_FALSE(finder()->IsCssCriticalImage("imageA.jpeg", rewrite_driver()));

  // Reset the driver, read the page and call UpdateCriticalImagesSetInDriver by
  // calling IsHtmlCriticalImage.
  // We read it from cache.
  ResetDriver();
  EXPECT_TRUE(finder()->IsHtmlCriticalImage("imageA.jpeg", rewrite_driver()));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  // Advance to 90% of expiry. We get a hit from cache and must_compute is true.
  AdvanceTimeMs(0.9 * options()->finder_properties_cache_expiration_time_ms());
  ResetDriver();
  EXPECT_TRUE(finder()->IsHtmlCriticalImage("imageA.jpeg", rewrite_driver()));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  ResetDriver();
  // Advance past expiry, so that the pages expire.
  AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
  EXPECT_FALSE(finder()->IsHtmlCriticalImage("imageA.jpeg", rewrite_driver()));
  CheckCriticalImageFinderStats(0, 1, 0);
  EXPECT_EQ(0, logging_info()->num_html_critical_images());
  EXPECT_EQ(0, logging_info()->num_css_critical_images());
}

TEST_F(CriticalImagesHistoryFinderTest, GetCriticalImagesTest) {
  const PropertyCache::Cohort* cohort = page_property_cache()->GetCohort(
      finder()->GetCriticalImagesCohort());

  // Verify that storing multiple critical images, like we do with the beacon
  // critical image finder, works correctly.

  // Write images to property cache, ensuring that they are critical images, and
  // verify that we have only stored up to NumSetsToKeep() sets at the end.
  for (int i = 0; i < finder()->NumSetsToKeep() * 2; ++i) {
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgA.jpeg");
    critical_images_set->insert("imgB.jpeg");
    StringSet* css_critical_images_set = new StringSet;
    css_critical_images_set->insert("imgD.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, css_critical_images_set));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();

    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsCssCriticalImage("imgA.jpeg", rewrite_driver()));
  }

  // Verify that we are only storing NumSetsToKeep() sets.
  ArrayInputStream input(GetCriticalImagesUpdatedValue()->value().data(),
                         GetCriticalImagesUpdatedValue()->value().size());
  CriticalImages parsed_proto;
  parsed_proto.ParseFromZeroCopyStream(&input);
  EXPECT_EQ(finder()->NumSetsToKeep(),
            parsed_proto.html_critical_images_sets_size());

  // Now, write just imgA twice. Since our limit is set to 80%, B should still
  // be critical afterwards.
  for (int i = 0; i < 2; ++i) {
    ResetDriver();
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgA.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
  }

  // Continue writing imgA, but now imgB should be below our threshold.
  for (int i = 0; i < finder()->NumSetsToKeep(); ++i) {
    ResetDriver();
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgA.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    // We didn't write CSS critical images, so imgD should still be critical.
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
  }

  // Write imgC twice. imgA should still be critical, and C should not.
  for (int i = 0; i < 2; ++i) {
    ResetDriver();
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgC.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgC.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
  }

  // Continue writing imgC, but A should no longer be critical.
  for (int i = 0; i < 5; ++i) {
    ResetDriver();
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgC.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgC.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
  }

  // And finally, write imgC, making sure it is critical.
  for (int i = 0; i < finder()->NumSetsToKeep(); ++i) {
    ResetDriver();
    StringSet* critical_images_set = new StringSet;
    critical_images_set->insert("imgC.jpeg");
    EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(
        rewrite_driver(), critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(cohort);
    ResetDriver();
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgA.jpeg", rewrite_driver()));
    EXPECT_FALSE(finder()->IsHtmlCriticalImage("imgB.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsHtmlCriticalImage("imgC.jpeg", rewrite_driver()));
    EXPECT_TRUE(finder()->IsCssCriticalImage("imgD.jpeg", rewrite_driver()));
  }
}

}  // namespace net_instaweb
