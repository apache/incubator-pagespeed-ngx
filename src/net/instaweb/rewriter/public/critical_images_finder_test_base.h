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

#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class RewriteDriver;

class CriticalImagesFinderTestBase : public RewriteTestBase {
 public:
  virtual CriticalImagesFinder* finder() = 0;

  bool CallUpdateCriticalImagesCacheEntry(RewriteDriver* driver,
                                          StringSet* critical_images_set) {
    return finder()->UpdateCriticalImagesCacheEntry(driver,
                                                    critical_images_set);
  }

 protected:
  NullStatistics stats_;

  virtual void SetUp();

  const PropertyValue* GetUpdatedValue();

 private:
  static const char kRequestUrl[];

  class MockPage : public PropertyPage {
   public:
    MockPage(AbstractMutex* mutex, const StringPiece& key)
        : PropertyPage(mutex, key) {}
    virtual ~MockPage();
    virtual void Done(bool valid) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(MockPage);
  };
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_TEST_BASE_H_
