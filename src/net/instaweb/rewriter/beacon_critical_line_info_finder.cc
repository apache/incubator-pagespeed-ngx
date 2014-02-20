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

// Author: jud@google.com (Jud Porter)

#include "net/instaweb/rewriter/public/beacon_critical_line_info_finder.h"

#include <set>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class Timer;

const char BeaconCriticalLineInfoFinder::kBeaconCriticalLineInfoPropertyName[] =
    "beacon_critical_line_info";

BeaconCriticalLineInfoFinder::BeaconCriticalLineInfoFinder(
    const PropertyCache::Cohort* cohort, NonceGenerator* nonce_generator)
    : CriticalLineInfoFinder(cohort), nonce_generator_(nonce_generator) {}

BeaconCriticalLineInfoFinder::~BeaconCriticalLineInfoFinder() {}

void BeaconCriticalLineInfoFinder::WriteXPathsToPropertyCacheFromBeacon(
    const StringSet& xpaths_set, StringPiece nonce, const PropertyCache* cache,
    const PropertyCache::Cohort* cohort, AbstractPropertyPage* page,
    MessageHandler* message_handler, Timer* timer) {
  WriteCriticalKeysToPropertyCache(xpaths_set, nonce, kDefaultSupportInterval,
                                   false /* should_replace_prior_result */,
                                   false /* require_prior_support */,
                                   kBeaconCriticalLineInfoPropertyName, cache,
                                   cohort, page, message_handler, timer);
}

BeaconMetadata BeaconCriticalLineInfoFinder::PrepareForBeaconInsertion(
    RewriteDriver* driver) {
  BeaconMetadata metadata;
  UpdateInDriver(driver);
  const StringSet empty;
  CriticalKeys* proto = driver->beacon_critical_line_info();
  // If an explicit xpath config has been set, we don't need to beacon. In that
  // case, beacon_critical_line_info() will be NULL.
  if (proto == NULL) {
    metadata.status = kDoNotBeacon;
    return metadata;
  }
  // Call the generic version of PrepareForBeaconInsertion in
  // critical_finder_support_util to decide if we should beacon.
  net_instaweb::PrepareForBeaconInsertionHelper(
      proto, nonce_generator_, driver,
      false /* using_candidate_key_detection */, &metadata);
  if (metadata.status != kDoNotBeacon) {
    DCHECK(cohort() != NULL);
    UpdateInPropertyCache(*proto, cohort(), kBeaconCriticalLineInfoPropertyName,
                          true /* write_cohort */,
                          driver->fallback_property_page());
  }
  return metadata;
}

void BeaconCriticalLineInfoFinder::UpdateInDriver(RewriteDriver* driver) {
  CHECK(driver != NULL);
  // The parent class's UpdateInDriver will populate critical_line_info if it
  // was configured explicitly, through a ModPagespeedCriticalLineConfig option
  // for instance.
  CriticalLineInfoFinder::UpdateInDriver(driver);

  // Don't recompute the critical line info if it has already been determined.
  if (driver->critical_line_info() != NULL ||
      driver->beacon_critical_line_info() != NULL) {
    return;
  }

  // Setup default values if the pcache isn't configured.
  if (driver->property_page() == NULL || cohort() == NULL) {
    driver->set_critical_line_info(NULL);
    driver->set_beacon_critical_line_info(new CriticalKeys);
    return;
  }

  // The split config was not explicitly set, so check to see if we have a
  // property cache entry from a beacon, and if so, populate critical_line_info
  // with it.
  PropertyCacheDecodeResult result;

  scoped_ptr<CriticalKeys> critical_keys(DecodeFromPropertyCache<CriticalKeys>(
      driver, cohort(), kBeaconCriticalLineInfoPropertyName,
      driver->options()->finder_properties_cache_expiration_time_ms(),
      &result));
  switch (result) {
    case kPropertyCacheDecodeNotFound:
      driver->InfoHere("Beacon critical line info not found in cache");
      break;
    case kPropertyCacheDecodeExpired:
      driver->InfoHere("Beacon critical line info cache entry expired");
      break;
    case kPropertyCacheDecodeParseError:
      driver->WarningHere(
          "Unable to parse beacon critical line info PropertyValue");
      break;
    case kPropertyCacheDecodeOk:
      break;
  }

  if (critical_keys == NULL) {
    critical_keys.reset(new CriticalKeys);
  }

  StringSet keys;
  GetCriticalKeysFromProto(0 /* support_percentage */, *critical_keys, &keys);

  // If there were critical keys in the pcache, populate the critical_line_info
  // with them.
  if (!keys.empty()) {
    scoped_ptr<CriticalLineInfo> critical_line_info(new CriticalLineInfo);
    for (StringSet::const_iterator it = keys.begin(); it != keys.end(); ++it) {
      Panel* panel = critical_line_info->add_panels();
      panel->set_start_xpath(*it);
    }
    driver->set_critical_line_info(critical_line_info.release());
  } else {
    driver->set_critical_line_info(NULL);
  }

  driver->set_beacon_critical_line_info(critical_keys.release());
}

}  // namespace net_instaweb
