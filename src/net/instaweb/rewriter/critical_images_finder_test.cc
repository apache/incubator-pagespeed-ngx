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
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

namespace {

// Mock class for testing a critical image finder like the beacon finder that
// stores a history of previous critical image sets.
class HistoryTestCriticalImagesFinder : public TestCriticalImagesFinder {
 public:
  HistoryTestCriticalImagesFinder(const PropertyCache::Cohort* cohort,
                                  Statistics* stats)
      : TestCriticalImagesFinder(cohort, stats) {}

  virtual int PercentSeenForCritical() const {
    return 80;
  }

  virtual int SupportInterval() const {
    return 10;
  }
};

const char kCriticalImagesCohort[] = "critical_images";

}  // namespace

class CriticalImagesFinderTest : public CriticalImagesFinderTestBase {
 public:
  virtual CriticalImagesFinder* finder() { return finder_.get(); }

 protected:
  virtual void SetUp() {
    CriticalImagesFinderTestBase::SetUp();
    SetupCohort(page_property_cache(), kCriticalImagesCohort);
    finder_.reset(new TestCriticalImagesFinder(
        page_property_cache()->GetCohort(kCriticalImagesCohort), statistics()));
    ResetDriver();
  }

 private:
  friend class CriticalImagesHistoryFinderTest;

  scoped_ptr<TestCriticalImagesFinder> finder_;
};

class CriticalImagesHistoryFinderTest : public CriticalImagesFinderTest {
 protected:
  virtual void SetUp() {
    CriticalImagesFinderTestBase::SetUp();
    SetupCohort(page_property_cache(), kCriticalImagesCohort);
    finder_.reset(new HistoryTestCriticalImagesFinder(
        page_property_cache()->GetCohort(kCriticalImagesCohort), statistics()));
    ResetDriver();
  }
};

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySuccess) {
  // Include an actual value in the RPC result to induce a cache write.
  StringSet html_critical_images_set;
  html_critical_images_set.insert("imageA.jpeg");
  StringSet css_critical_images_set;
  css_critical_images_set.insert("imageB.jpeg");
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
      &html_critical_images_set, &css_critical_images_set));
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());

  // Verify the contents of the support protobuf, and ensure we're no longer
  // generating legacy data.
  ArrayInputStream input(GetCriticalImagesUpdatedValue()->value().data(),
                         GetCriticalImagesUpdatedValue()->value().size());
  CriticalImages parsed_proto;
  parsed_proto.ParseFromZeroCopyStream(&input);
  ASSERT_TRUE(parsed_proto.has_html_critical_image_support());
  const CriticalKeys& html_support = parsed_proto.html_critical_image_support();
  EXPECT_EQ(1, html_support.key_evidence_size());
  ASSERT_TRUE(parsed_proto.has_css_critical_image_support());
  const CriticalKeys& css_support = parsed_proto.css_critical_image_support();
  EXPECT_EQ(1, css_support.key_evidence_size());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntrySuccessEmptySet) {
  // Include an actual value in the RPC result to induce a cache write.
  StringSet html_critical_images_set;
  StringSet css_critical_images_set;
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
      &html_critical_images_set, &css_critical_images_set));
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());
  rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
}

TEST_F(CriticalImagesFinderTest, UpdateCriticalImagesCacheEntrySetNULL) {
  EXPECT_FALSE(UpdateCriticalImagesCacheEntry(NULL, NULL));
  EXPECT_FALSE(GetCriticalImagesUpdatedValue()->has_value());
}

TEST_F(CriticalImagesFinderTest,
       UpdateCriticalImagesCacheEntryPropertyPageMissing) {
  // No cache insert if PropertyPage is not set in RewriteDriver.
  rewrite_driver()->set_property_page(NULL);
  // Include an actual value in the RPC result to induce a cache write. We
  // expect no writes, but not from a lack of results!
  StringSet html_critical_images_set;
  StringSet css_critical_images_set;
  EXPECT_FALSE(UpdateCriticalImagesCacheEntry(
      &html_critical_images_set, &css_critical_images_set));
  EXPECT_EQ(NULL, GetCriticalImagesUpdatedValue());
}

TEST_F(CriticalImagesFinderTest, GetCriticalImagesTest) {
  // First it will insert the value in cache, then it retrieves critical images.
  // Include an actual value in the RPC result to induce a cache write.
  StringSet html_critical_images_set;
  html_critical_images_set.insert("imageA.jpeg");
  html_critical_images_set.insert("imageB.jpeg");
  StringSet css_critical_images_set;
  css_critical_images_set.insert("imageD.jpeg");

  // Calling IsHtmlCriticalImage should update the CriticalImagesInfo in
  // RewriteDriver.
  EXPECT_FALSE(IsHtmlCriticalImage("imageA.jpg"));
  // We should get 1 miss for the critical images value.
  CheckCriticalImageFinderStats(0, 0, 1);
  // Here and below, -1 results mean "no critical image data reported".
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
  ClearStats();

  // Calling IsHtmlCriticalImage again should not update the stats, because the
  // CriticalImagesInfo has already been updated.
  EXPECT_FALSE(IsHtmlCriticalImage("imageA.jpg"));
  CheckCriticalImageFinderStats(0, 0, 0);
  // ClearStats() creates a new request context and hence a new log record. So
  // the critical image counts are not set.
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
  ClearStats();

  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
      &html_critical_images_set, &css_critical_images_set));
  // Write the updated value to the pcache.
  rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
  EXPECT_TRUE(GetCriticalImagesUpdatedValue()->has_value());

  // critical_images_info() is NULL because there is no previous call to
  // GetCriticalImages()
  ResetDriver();
  EXPECT_TRUE(rewrite_driver()->critical_images_info() == NULL);
  EXPECT_TRUE(IsHtmlCriticalImage("imageA.jpeg"));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  // GetCriticalImages() updates critical_images set in RewriteDriver().
  EXPECT_TRUE(rewrite_driver()->critical_images_info() != NULL);
  // EXPECT_EQ(2, GetCriticalImages(rewrite_driver()).size());
  EXPECT_TRUE(IsHtmlCriticalImage("imageA.jpeg"));
  EXPECT_TRUE(IsHtmlCriticalImage("imageB.jpeg"));
  EXPECT_FALSE(IsHtmlCriticalImage("imageC.jpeg"));

  // EXPECT_EQ(1, css_critical_images->size());
  EXPECT_TRUE(IsCssCriticalImage("imageD.jpeg"));
  EXPECT_FALSE(IsCssCriticalImage("imageA.jpeg"));

  // Reset the driver, read the page and call UpdateCriticalImagesSetInDriver by
  // calling IsHtmlCriticalImage.
  // We read it from cache.
  ResetDriver();
  EXPECT_TRUE(IsHtmlCriticalImage("imageA.jpeg"));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  // Advance to 90% of expiry. We get a hit from cache and must_compute is true.
  AdvanceTimeMs(0.9 * options()->finder_properties_cache_expiration_time_ms());
  ResetDriver();
  EXPECT_TRUE(IsHtmlCriticalImage("imageA.jpeg"));
  CheckCriticalImageFinderStats(1, 0, 0);
  EXPECT_EQ(2, logging_info()->num_html_critical_images());
  EXPECT_EQ(1, logging_info()->num_css_critical_images());
  ClearStats();

  ResetDriver();
  // Advance past expiry, so that the pages expire; now no images are critical.
  AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
  EXPECT_TRUE(rewrite_driver()->critical_images_info() == NULL);
  EXPECT_FALSE(IsHtmlCriticalImage("imageA.jpeg"));
  EXPECT_TRUE(rewrite_driver()->critical_images_info() != NULL);
  CheckCriticalImageFinderStats(0, 1, 0);
  // Here -1 results mean "no valid critical image data" due to expiry.
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
}

TEST_F(CriticalImagesHistoryFinderTest, GetCriticalImagesTest) {
  // Verify that storing multiple critical images, like we do with the beacon
  // critical image finder, works correctly.

  // Write images to property cache, ensuring that they are critical images
  StringSet html_critical_images_set;
  html_critical_images_set.insert("imgA.jpeg");
  html_critical_images_set.insert("imgB.jpeg");
  StringSet css_critical_images_set;
  css_critical_images_set.insert("imgD.jpeg");
  for (int i = 0; i < finder()->SupportInterval() * 3; ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, &css_critical_images_set));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_TRUE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_TRUE(IsHtmlCriticalImage("imgB.jpeg"));
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
    EXPECT_FALSE(IsCssCriticalImage("imgA.jpeg"));
  }

  // Now, write just imgA twice. Since our limit is set to 80%, B should still
  // be critical afterwards.
  html_critical_images_set.clear();
  html_critical_images_set.insert("imgA.jpeg");
  for (int i = 0; i < 2; ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_TRUE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_TRUE(IsHtmlCriticalImage("imgB.jpeg"));
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
  }

  // Continue writing imgA, but now imgB should be below our threshold.
  for (int i = 0; i < finder()->SupportInterval(); ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_TRUE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgB.jpeg"));
    // We didn't write CSS critical images, so imgD should still be critical.
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
  }

  // Write imgC twice. imgA should still be critical, and C should not.
  html_critical_images_set.clear();
  html_critical_images_set.insert("imgC.jpeg");
  for (int i = 0; i < 2; ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_TRUE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgB.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgC.jpeg"));
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
  }

  // Continue writing imgC; it won't have enough support to make it critical,
  // and A should no longer be critical.  That's because the maximum possible
  // support value will have saturated, so we need a fair amount of support
  // before we reach the saturated value.  Basically we're iterating until:
  //   sum{k<-1..n} ((s(s-1))/s)^k  >=  r sum{k<-1..infinity} ((s(s-1)/s)^k
  // And in this case, where s=10 and r=80%, k happens to be 14 (2 iterations
  // above and 12 iterations here).  To make things more fun, the above
  // calculations are done approximately using integer arithmetic, which makes
  // the limit much easier to compute.
  for (int i = 0; i < 12; ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_FALSE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgB.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgC.jpeg"));
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
  }

  // And finally, write imgC, making sure it is critical.
  for (int i = 0; i < finder()->SupportInterval(); ++i) {
    ResetDriver();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL));
    rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
    ResetDriver();
    EXPECT_FALSE(IsHtmlCriticalImage("imgA.jpeg"));
    EXPECT_FALSE(IsHtmlCriticalImage("imgB.jpeg"));
    EXPECT_TRUE(IsHtmlCriticalImage("imgC.jpeg"));
    EXPECT_TRUE(IsCssCriticalImage("imgD.jpeg"));
  }
}

TEST_F(CriticalImagesFinderTest, NoCriticalImages) {
  // Make sure we deal gracefully when there are no critical images in a beacon
  // result.
  StringSet critical;
  EXPECT_TRUE(critical.empty());
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&critical, &critical));
  rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
  ResetDriver();
  EXPECT_FALSE(IsHtmlCriticalImage("imgA.jpeg"));
  EXPECT_FALSE(IsCssCriticalImage("imgA.jpeg"));
  EXPECT_TRUE(finder()->GetHtmlCriticalImages(rewrite_driver()).empty());
  EXPECT_TRUE(finder()->GetCssCriticalImages(rewrite_driver()).empty());
  // Now register critical images and make sure we can leave the empty state.
  critical.insert("imgA.jpeg");
  for (int i = 0; i < finder()->SupportInterval(); ++i) {
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&critical, &critical));
  }
  rewrite_driver()->property_page()->WriteCohort(finder()->cohort());
  ResetDriver();
  EXPECT_TRUE(IsHtmlCriticalImage("imgA.jpeg"));
  EXPECT_TRUE(IsCssCriticalImage("imgA.jpeg"));
}

TEST_F(CriticalImagesFinderTest, TestRenderedImageExtractionFromPropertyCache) {
  RenderedImages rendered_images;
  RenderedImages_Image* images = rendered_images.add_image();
  GoogleString url_str = "http://example.com/imageA.jpeg";
  images->set_src(url_str);
  images->set_rendered_width(40);
  images->set_rendered_height(54);
  PropertyPage* page = rewrite_driver()->property_page();
  UpdateInPropertyCache(rendered_images, finder()->cohort(),
                        finder()->kRenderedImageDimensionsProperty,
                        false /* don't write cohort */, page);
  // Check if Finder extracts properly.
  scoped_ptr<RenderedImages> extracted_rendered_images(
          finder()->ExtractRenderedImageDimensionsFromCache(rewrite_driver()));

  EXPECT_EQ(1, extracted_rendered_images->image_size());
  EXPECT_STREQ(url_str, extracted_rendered_images->image(0).src());
  EXPECT_EQ(40, extracted_rendered_images->image(0).rendered_width());
  EXPECT_EQ(54, extracted_rendered_images->image(0).rendered_height());

  options()->EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  std::pair<int32, int32> dimensions;
  GoogleUrl gurl(url_str);
  EXPECT_TRUE(finder()->GetRenderedImageDimensions(rewrite_driver(), gurl,
                                                   &dimensions));
  EXPECT_EQ(std::make_pair(40, 54), dimensions);
}

}  // namespace net_instaweb
