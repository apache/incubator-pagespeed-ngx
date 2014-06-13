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

#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const char CriticalImagesFinderTestBase::kRequestUrl[] = "http://www.test.com";

void CriticalImagesFinderTestBase::ResetDriver() {
  rewrite_driver()->Clear();
  rewrite_driver()->set_request_context(
      RequestContext::NewTestRequestContext(factory()->thread_system()));
  PropertyCache* pcache = server_context_->page_property_cache();
  MockPropertyPage* page = NewMockPage(kRequestUrl);
  rewrite_driver()->set_property_page(page);
  pcache->set_enabled(true);
  pcache->Read(page);
}

const PropertyValue*
CriticalImagesFinderTestBase::GetCriticalImagesUpdatedValue() {
  PropertyPage* page = rewrite_driver()->property_page();
  if (page == NULL) {
    return NULL;
  }
  const PropertyCache::Cohort* cohort = finder()->cohort();
  if (cohort == NULL) {
    return NULL;
  }
  const PropertyValue* property_value = page->GetProperty(
      cohort, CriticalImagesFinder::kCriticalImagesPropertyName);
  return property_value;
}

void CriticalImagesFinderTestBase::CheckCriticalImageFinderStats(
    int hits, int expiries, int not_found) {
  EXPECT_EQ(hits, statistics()->GetVariable(
      CriticalImagesFinder::kCriticalImagesValidCount)->Get());
  EXPECT_EQ(expiries, statistics()->GetVariable(
      CriticalImagesFinder::kCriticalImagesExpiredCount)->Get());
  EXPECT_EQ(not_found, statistics()->GetVariable(
      CriticalImagesFinder::kCriticalImagesNotFoundCount)->Get());
}

bool CriticalImagesFinderTestBase::IsHtmlCriticalImage(StringPiece url) {
  return finder()->IsHtmlCriticalImage(url, rewrite_driver());
}

bool CriticalImagesFinderTestBase::IsCssCriticalImage(StringPiece url) {
  return finder()->IsCssCriticalImage(url, rewrite_driver());
}

TestCriticalImagesFinder::~TestCriticalImagesFinder() {}

}  // namespace net_instaweb
