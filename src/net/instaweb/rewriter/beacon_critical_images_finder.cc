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

namespace net_instaweb {

const char BeaconCriticalImagesFinder::kBeaconCohort[] = "beacon_cohort";

// Note: This class is not yet implemented.
BeaconCriticalImagesFinder::BeaconCriticalImagesFinder() {
}

BeaconCriticalImagesFinder::~BeaconCriticalImagesFinder() {
}

void BeaconCriticalImagesFinder::ComputeCriticalImages(
    StringPiece url, RewriteDriver* driver, bool must_compute) {
}

}  // namespace net_instaweb
