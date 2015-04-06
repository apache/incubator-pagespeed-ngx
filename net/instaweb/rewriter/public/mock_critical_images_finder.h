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
// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_IMAGES_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_IMAGES_FINDER_H_

#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class RewriteDriver;
class Statistics;

// Mock implementation of CriticalImagesFinder that can store and retrieve
// critical images. Note that this doesn't use property cache.
class MockCriticalImagesFinder : public TestCriticalImagesFinder {
 public:
  explicit MockCriticalImagesFinder(Statistics* stats)
      : TestCriticalImagesFinder(NULL, stats), compute_calls_(0) {}

  MockCriticalImagesFinder(const PropertyCache::Cohort* cohort,
                           Statistics* stats)
      : TestCriticalImagesFinder(cohort, stats), compute_calls_(0) {}

  ~MockCriticalImagesFinder();

  virtual Availability Available(RewriteDriver* driver) {
    return kAvailable;
  }

  virtual void UpdateCriticalImagesSetInDriver(RewriteDriver* driver);

  // Extracts rendered image dimensions from property cache.
  virtual RenderedImages* ExtractRenderedImageDimensionsFromCache(
      RewriteDriver* driver);

  virtual void ComputeCriticalImages(RewriteDriver* driver) {
    ++compute_calls_;
  }

  int num_compute_calls() { return compute_calls_; }

  void set_critical_images(StringSet* critical_images) {
    critical_images_.reset(critical_images);
  }

  void set_css_critical_images(StringSet* css_critical_images) {
    css_critical_images_.reset(css_critical_images);
  }

  void set_rendered_images(RenderedImages* rendered_images) {
    rendered_images_.reset(rendered_images);
  }

  virtual bool IsCriticalImageInfoPresent(RewriteDriver* driver) {
    return true;
  }

 private:
  int compute_calls_;
  scoped_ptr<StringSet> critical_images_;
  scoped_ptr<StringSet> css_critical_images_;
  scoped_ptr<RenderedImages> rendered_images_;
  DISALLOW_COPY_AND_ASSIGN(MockCriticalImagesFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOCK_CRITICAL_IMAGES_FINDER_H_
