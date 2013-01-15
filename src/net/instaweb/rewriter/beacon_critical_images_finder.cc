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
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char BeaconCriticalImagesFinder::kBeaconCohort[] = "beacon_cohort";

BeaconCriticalImagesFinder::BeaconCriticalImagesFinder(Statistics* stats)
    : CriticalImagesFinder(stats) {}

BeaconCriticalImagesFinder::~BeaconCriticalImagesFinder() {
}

bool BeaconCriticalImagesFinder::IsCriticalImage(
    const GoogleString& image_url, const RewriteDriver* driver) const {
  unsigned int hash_val = HashString<CasePreserve, unsigned int>(
      image_url.c_str(), image_url.size());
  GoogleString hash_str = UintToString(hash_val);
  return CriticalImagesFinder::IsCriticalImage(hash_str, driver);
}

void BeaconCriticalImagesFinder::ComputeCriticalImages(
    StringPiece url, RewriteDriver* driver) {
}

}  // namespace net_instaweb
