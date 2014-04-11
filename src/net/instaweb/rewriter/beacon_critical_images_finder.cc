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

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

BeaconCriticalImagesFinder::BeaconCriticalImagesFinder(
    const PropertyCache::Cohort* cohort, NonceGenerator* nonce_generator,
    Statistics* stats)
    : CriticalImagesFinder(cohort, stats), nonce_generator_(nonce_generator) {}

BeaconCriticalImagesFinder::~BeaconCriticalImagesFinder() {
}

bool BeaconCriticalImagesFinder::UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const RenderedImages* rendered_images_set,
      const StringPiece& nonce,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page, Timer* timer) {
  DCHECK(cohort != NULL);
  DCHECK(page != NULL);
  PropertyValue* property_value =
      page->GetProperty(cohort, kCriticalImagesPropertyName);
  if (property_value == NULL) {
    return false;
  }
  CriticalImages critical_images;
  if (!PopulateCriticalImagesFromPropertyValue(
          property_value, &critical_images)) {
    return false;
  }
  if (!ValidateAndExpireNonce(
          timer->NowMs(), nonce,
          critical_images.mutable_html_critical_image_support())) {
    return false;
  }
  return CriticalImagesFinder::UpdateAndWriteBackCriticalImagesCacheEntry(
      html_critical_images_set, css_critical_images_set, rendered_images_set,
      kBeaconImageSupportInterval, cohort, page, &critical_images);
}

GoogleString BeaconCriticalImagesFinder::GetKeyForUrl(const GoogleString& url) {
  unsigned int hash_val = HashString<CasePreserve, unsigned int>(
      url.c_str(), url.size());
  return UintToString(hash_val);
}

CriticalImagesFinder::Availability BeaconCriticalImagesFinder::Available(
    RewriteDriver* driver) {
  if (driver->options()->critical_images_beacon_enabled() &&
      driver->server_context()->factory()->UseBeaconResultsInFilters()) {
    return CriticalImagesFinder::Available(driver);
  } else {
    return CriticalImagesFinder::kDisabled;
  }
}

bool BeaconCriticalImagesFinder::ShouldBeacon(RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  return ::net_instaweb::ShouldBeacon(
      driver->critical_images_info()->proto.html_critical_image_support(),
      *driver);
}

BeaconMetadata BeaconCriticalImagesFinder::PrepareForBeaconInsertion(
    RewriteDriver* driver) {
  BeaconMetadata metadata;
  UpdateCriticalImagesSetInDriver(driver);
  CriticalImages& proto = driver->critical_images_info()->proto;
  // We store the metadata about last beacon time and nonce generation in the
  // html_critical_image_support field of the CriticalImages proto.
  net_instaweb::PrepareForBeaconInsertionHelper(
      proto.mutable_html_critical_image_support(), nonce_generator_, driver,
      true /* using_candidate_key_detection */, &metadata);
  if (metadata.status != kDoNotBeacon) {
    UpdateInPropertyCache(proto, cohort(), kCriticalImagesPropertyName,
                          true /* write_cohort */,
                          driver->fallback_property_page());
  }
  return metadata;
}

void BeaconCriticalImagesFinder::UpdateCandidateImagesForBeaconing(
    const StringSet& images, RewriteDriver* driver, bool beaconing) {
  UpdateCriticalImagesSetInDriver(driver);
  CriticalImages& proto = driver->critical_images_info()->proto;
  if (::net_instaweb::UpdateCandidateKeys(
          images, proto.mutable_html_critical_image_support(), !beaconing)) {
    UpdateInPropertyCache(proto, cohort(), kCriticalImagesPropertyName,
                          true /* write_cohort */,
                          driver->fallback_property_page());
  }
}

}  // namespace net_instaweb
