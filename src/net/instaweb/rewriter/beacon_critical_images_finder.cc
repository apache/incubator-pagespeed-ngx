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

bool BeaconCriticalImagesFinder::IsHtmlCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  unsigned int hash_val = HashString<CasePreserve, unsigned int>(
      image_url.c_str(), image_url.size());
  GoogleString hash_str = UintToString(hash_val);
  return CriticalImagesFinder::IsHtmlCriticalImage(hash_str, driver);
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

}  // namespace net_instaweb
