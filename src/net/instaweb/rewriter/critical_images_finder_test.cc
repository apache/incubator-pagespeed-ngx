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

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/property_cache.h"

namespace net_instaweb {

// Provide stub implementation of abstract base class for testing purposes.
class CriticalImagesFinderMock : public CriticalImagesFinder {
 public:
  // Provide stub instantions for pure virtual functions
  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver,
                                     bool must_compute) {
  }

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
    finder_.reset(new CriticalImagesFinderMock());
  }

 private:
  scoped_ptr<CriticalImagesFinderMock> finder_;
};

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySuccess) {
  page_property_cache()->AddCohort(finder()->GetCriticalImagesCohort());
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  critical_images_set->insert("imageA.jpeg");
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(),
                                                 critical_images_set));
  EXPECT_TRUE(GetUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntrySuccessEmptySet) {
  page_property_cache()->AddCohort(finder()->GetCriticalImagesCohort());
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(),
                                                 critical_images_set));
  EXPECT_TRUE(GetUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySetNULL) {
  page_property_cache()->AddCohort(finder()->GetCriticalImagesCohort());
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(), NULL));
  EXPECT_FALSE(GetUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntryCohortMissing) {
  // No cache insert if render cohort is missing.
  // Include an actual value in the RPC result to induce a cache write. We
  // expect no writes, but not from a lack of results!
  StringSet* critical_images_set = new StringSet;
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(),
                                                  critical_images_set));
  EXPECT_EQ(NULL, GetUpdatedValue());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntryPropertyPageMissing) {
  // No cache insert if PropertyPage is not set in RewriteDriver.
  rewrite_driver()->set_property_page(NULL);
  page_property_cache()->AddCohort(finder()->GetCriticalImagesCohort());
  // Include an actual value in the RPC result to induce a cache write. We
  // expect no writes, but not from a lack of results!
  StringSet* critical_images_set = new StringSet;
  EXPECT_FALSE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(),
                                                  critical_images_set));
  EXPECT_EQ(NULL, GetUpdatedValue());
}

TEST_F(CriticalImagesFinderTest, GetCriticalImagesTest) {
  // First it will insert the value in cache, then it retrieves critical images.
  page_property_cache()->AddCohort(finder()->GetCriticalImagesCohort());
  // Include an actual value in the RPC result to induce a cache write.
  StringSet* critical_images_set = new StringSet;
  critical_images_set->insert("imageA.jpeg");
  critical_images_set->insert("imageB.jpeg");
  EXPECT_TRUE(CallUpdateCriticalImagesCacheEntry(rewrite_driver(),
                                                 critical_images_set));
  EXPECT_TRUE(GetUpdatedValue()->has_value());

  // critical_images() is NULL because there is no previous call to
  // GetCriticalImages()
  EXPECT_TRUE(rewrite_driver()->critical_images() == NULL);
  finder()->UpdateCriticalImagesSetInDriver(rewrite_driver());
  // GetCriticalImages() upates critical_images set in RewriteDriver().
  const StringSet* critical_images = rewrite_driver()->critical_images();
  EXPECT_TRUE(critical_images != NULL);
  EXPECT_EQ(2, critical_images->size());
  EXPECT_TRUE(finder()->IsCriticalImage("imageA.jpeg", rewrite_driver()));
  EXPECT_TRUE(finder()->IsCriticalImage("imageB.jpeg", rewrite_driver()));
  EXPECT_FALSE(finder()->IsCriticalImage("imageC.jpeg", rewrite_driver()));
}

}  // namespace net_instaweb
