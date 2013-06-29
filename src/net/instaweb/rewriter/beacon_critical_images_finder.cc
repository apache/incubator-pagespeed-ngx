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

#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"

namespace net_instaweb {

BeaconCriticalImagesFinder::BeaconCriticalImagesFinder(
    const PropertyCache::Cohort* cohort,
    NonceGenerator* nonce_generator,
    Statistics* stats)
    : CriticalImagesFinder(stats),
      cohort_(cohort),
      nonce_generator_(nonce_generator) { }

BeaconCriticalImagesFinder::~BeaconCriticalImagesFinder() {
}

bool BeaconCriticalImagesFinder::UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page) {
  return CriticalImagesFinder::UpdateCriticalImagesCacheEntry(
      html_critical_images_set,
      css_critical_images_set,
      kBeaconNumSetsToKeep,
      kBeaconPercentSeenForCritical,
      cohort, page);
}

GoogleString BeaconCriticalImagesFinder::GetKeyForUrl(const GoogleString& url) {
  unsigned int hash_val = HashString<CasePreserve, unsigned int>(
      url.c_str(), url.size());
  return UintToString(hash_val);
}

bool BeaconCriticalImagesFinder::IsMeaningful(
    const RewriteDriver* driver) const {
  CriticalImagesInfo* info = driver->critical_images_info();
  // The finder is meaningful if the critical images info was set by the split
  // html helper.
  if (info != NULL && info->is_set_from_split_html) {
    return true;
  }
  return driver->options()->critical_images_beacon_enabled() &&
      driver->server_context()->factory()->UseBeaconResultsInFilters();
}

}  // namespace net_instaweb
