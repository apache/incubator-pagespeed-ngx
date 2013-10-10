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
// Author: jud@google.com (Jud Porter)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_TEST_BASE_H_

#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteDriver;
class Statistics;

// Provide stub implementation of abstract base class for testing purposes.
class TestCriticalImagesFinder : public CriticalImagesFinder {
 public:
  TestCriticalImagesFinder(const PropertyCache::Cohort* cohort,
                           Statistics* stats)
      : CriticalImagesFinder(cohort, stats) {}
  virtual ~TestCriticalImagesFinder();
  virtual bool IsMeaningful(const RewriteDriver* driver) const { return true; }
  virtual void ComputeCriticalImages(RewriteDriver* driver) {}
};

class CriticalImagesFinderTestBase : public RewriteTestBase {
 public:
  virtual CriticalImagesFinder* finder() = 0;

  virtual bool UpdateCriticalImagesCacheEntry(
      const StringSet* critical_images_set,
      const StringSet* css_critical_images_set) {
    return finder()->UpdateCriticalImagesCacheEntryFromDriver(
        critical_images_set, css_critical_images_set, rewrite_driver());
  }

  void CheckCriticalImageFinderStats(int hits, int expiries, int not_found);

  bool IsHtmlCriticalImage(const GoogleString& url);
  bool IsCssCriticalImage(const GoogleString& url);

 protected:
  NullStatistics stats_;

  // Resets the state of the driver.
  void ResetDriver();

  const PropertyValue* GetCriticalImagesUpdatedValue();

 private:
  static const char kRequestUrl[];
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_TEST_BASE_H_
