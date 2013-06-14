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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/critical_selector_finder.h"

#include <map>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

typedef map<GoogleString, int> SupportMap;

// *dest += addend, but capping at kint32max
inline void SaturatingAddTo(int32 addend, int32* dest) {
  int64 result = *dest;
  result += addend;
  *dest = (result > kint32max) ? kint32max : static_cast<int32>(result);
}

SupportMap ConvertCriticalSelectorsToSupportMap(
    const CriticalSelectorSet& selectors, int legacy_support_value) {
  SupportMap support_map;
  // Invariant: we have at most one of selectors data or evidence data.
  DCHECK(selectors.selector_evidence_size() == 0 ||
         selectors.critical_selectors_size() == 0);
  // Start by reading in the support data
  for (int i = 0; i < selectors.selector_evidence_size(); ++i) {
    const CriticalSelectorSet::SelectorEvidence& evidence =
        selectors.selector_evidence(i);
    if (!evidence.selector().empty()) {
      // We aggregate here just in case of a corrupt duplicate entry.
      SaturatingAddTo(evidence.support(), &support_map[evidence.selector()]);
    }
  }
  // Now migrate legacy data into support_map.  Start with the response history.
  for (int i = 0; i < selectors.selector_set_history_size(); ++i) {
    const CriticalSelectorSet::BeaconResponse& response =
        selectors.selector_set_history(i);
    for (int j = 0; j < response.selectors_size(); ++j) {
      SaturatingAddTo(legacy_support_value,
                      &support_map[response.selectors(j)]);
    }
  }
  // Sometimes we have critical_selectors with no response history (eg when only
  // a single legacy beacon result was computed).  Inject support for
  // critical_selectors if they weren't supported by the response history.  This
  // avoids double-counting beacon results.
  for (int i = 0; i < selectors.critical_selectors_size(); ++i) {
    int& map_value = support_map[selectors.critical_selectors(i)];
    if (map_value == 0) {
      SaturatingAddTo(legacy_support_value, &map_value);
    }
  }
  return support_map;
}

void WriteSupportMapToCriticalSelectors(
    const SupportMap& support_map, CriticalSelectorSet* critical_selector_set) {
  // Clean out the critical_selector_set and legacy data and inject the fresh
  // data.
  critical_selector_set->clear_critical_selectors();
  critical_selector_set->clear_selector_set_history();
  critical_selector_set->clear_selector_evidence();
  for (SupportMap::const_iterator entry = support_map.begin();
       entry != support_map.end(); ++entry) {
    CriticalSelectorSet::SelectorEvidence* evidence =
        critical_selector_set->add_selector_evidence();
    evidence->set_selector(entry->first);
    evidence->set_support(entry->second);
  }
}

// Merge the given set into the existing critical selector sets by adding
// support for new_set to existing support.
void UpdateCriticalSelectorSet(
    const StringSet& new_set, int support_value,
    CriticalSelectorSet* critical_selector_set) {
  DCHECK(critical_selector_set != NULL);
  SupportMap support_map = ConvertCriticalSelectorsToSupportMap(
      *critical_selector_set, support_value);
  // Actually add the new_set to the support_map.
  for (StringSet::const_iterator s = new_set.begin(); s != new_set.end(); ++s) {
    // Only add entries that are already in the support_map
    // (critical_css_beacon_filter initializes candidate entries to have support
    // 0).  This avoids a cache-fill DoS with spurious beacon data.
    SupportMap::iterator entry = support_map.find(*s);
    if (entry != support_map.end()) {
      SaturatingAddTo(support_value, &entry->second);
    }
  }
  WriteSupportMapToCriticalSelectors(support_map, critical_selector_set);
}

// Write selector_set to given cohort and page of pcache.  Used after
// selector_set update occurs.
void WriteCriticalSelectorSetToPropertyCache(
    const CriticalSelectorSet& selector_set,
    const PropertyCache::Cohort* cohort, PropertyPage* page,
    MessageHandler* message_handler) {
  PropertyCacheUpdateResult result =
      UpdateInPropertyCache(
          selector_set, cohort,
          CriticalSelectorFinder::kCriticalSelectorsPropertyName,
          false /* write_cohort */, page);
  switch (result) {
    case kPropertyCacheUpdateNotFound:
      message_handler->Message(
          kWarning, "Unable to get Critical css selector set for update.");
      break;
    case kPropertyCacheUpdateEncodeError:
      message_handler->Message(
          kWarning, "Trouble marshaling CriticalSelectorSet!?");
      break;
    case kPropertyCacheUpdateOk:
      // Nothing more to do.
      break;
  }
}

// Decay support, deleting elements whose support drops to 0.
void DecaySupportMap(int support_interval, SupportMap* support_map) {
  SupportMap::iterator next = support_map->begin(), end = support_map->end();
  while (next != end) {
    // We increment at the top of the loop so that we can erase curr
    // without invalidating next.  See:
    // http://stackoverflow.com/questions/2874441/deleting-elements-from-stl-set-while-iterating
    SupportMap::iterator curr = next++;
    // Multiply i->second by the fraction
    // support_interval / (support_interval + 1)
    // Using int64 to avoid overflow.
    int64 support_value = curr->second;
    support_value *= support_interval;
    support_value /= support_interval + 1;
    if (support_value == 0 && curr->second > 0) {
      // Remove entry when its support falls to 0 (this will expire entries that
      // should not be candidates; if curr should still be a candidate, we will
      // re-insert it as part of beaconing).
      support_map->erase(curr);
    } else {
      curr->second = static_cast<int32>(support_value);
    }
  }
}

}  // namespace

const char CriticalSelectorFinder::kCriticalSelectorsPropertyName[] =
    "critical_selectors";

const char CriticalSelectorFinder::kCriticalSelectorsValidCount[] =
    "critical_selectors_valid_count";

const char CriticalSelectorFinder::kCriticalSelectorsExpiredCount[] =
    "critical_selectors_expired_count";

const char CriticalSelectorFinder::kCriticalSelectorsNotFoundCount[] =
    "critical_selectors_not_found_count";

const int64 CriticalSelectorFinder::kMinBeaconIntervalMs = 5 * Timer::kSecondMs;

CriticalSelectorFinder::CriticalSelectorFinder(
    const PropertyCache::Cohort* cohort,
    NonceGenerator* nonce_generator, Statistics* statistics)
    : cohort_(cohort),
      nonce_generator_(nonce_generator) {
  critical_selectors_valid_count_ = statistics->GetTimedVariable(
      kCriticalSelectorsValidCount);
  critical_selectors_expired_count_ = statistics->GetTimedVariable(
      kCriticalSelectorsExpiredCount);
  critical_selectors_not_found_count_ = statistics->GetTimedVariable(
      kCriticalSelectorsNotFoundCount);
}

CriticalSelectorFinder::~CriticalSelectorFinder() {
}

void CriticalSelectorFinder::InitStats(Statistics* statistics) {
  statistics->AddTimedVariable(kCriticalSelectorsValidCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kCriticalSelectorsExpiredCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kCriticalSelectorsNotFoundCount,
                               ServerContext::kStatisticsGroup);
}

CriticalSelectorSet*
CriticalSelectorFinder::DecodeCriticalSelectorsFromPropertyCache(
    RewriteDriver* driver) {
  PropertyCacheDecodeResult result;
  // NOTE: if any of these checks fail you probably didn't set up your test
  // environment carefully enough.  Figuring that out based on test failures
  // alone will drive you nuts and take hours out of your life, thus DCHECKs.
  DCHECK(driver != NULL);
  DCHECK(driver->property_page() != NULL);
  DCHECK(cohort_ != NULL);
  scoped_ptr<CriticalSelectorSet> critical_selectors(
      DecodeFromPropertyCache<CriticalSelectorSet>(
          driver, cohort_, kCriticalSelectorsPropertyName,
          driver->options()->finder_properties_cache_expiration_time_ms(),
          &result));
  switch (result) {
    case kPropertyCacheDecodeNotFound:
      critical_selectors_not_found_count_->IncBy(1);
      break;
    case kPropertyCacheDecodeExpired:
      critical_selectors_expired_count_->IncBy(1);
      break;
    case kPropertyCacheDecodeParseError:
      driver->message_handler()->Message(
          kWarning, "Unable to parse Critical Selectors PropertyValue; "
          "url: %s", driver->url());
      break;
    case kPropertyCacheDecodeOk:
      critical_selectors_valid_count_->IncBy(1);
      return critical_selectors.release();
  }
  return NULL;
}

bool CriticalSelectorFinder::GetCriticalSelectorsFromPropertyCache(
    RewriteDriver* driver, StringSet* critical_selectors) {
  CriticalSelectorSet* pcache_selectors = driver->CriticalSelectors();
  critical_selectors->clear();
  if (pcache_selectors == NULL) {
    return false;
  }
  // Collect legacy beacon results
  for (int i = 0; i < pcache_selectors->critical_selectors_size(); ++i) {
    critical_selectors->insert(pcache_selectors->critical_selectors(i));
  }
  // Collect supported beacon results
  for (int i = 0; i < pcache_selectors->selector_evidence_size(); ++i) {
    const CriticalSelectorSet::SelectorEvidence& evidence =
        pcache_selectors->selector_evidence(i);
    if (evidence.support() > 0 && !evidence.selector().empty()) {
      critical_selectors->insert(evidence.selector());
    }
  }
  return true;
}

void CriticalSelectorFinder::WriteCriticalSelectorsToPropertyCache(
  const StringSet& selector_set, RewriteDriver* driver) {
  WriteCriticalSelectorsToPropertyCache(
      selector_set,
      driver->server_context()->page_property_cache(),
      driver->property_page(),
      driver->message_handler());
}

void CriticalSelectorFinder::WriteCriticalSelectorsToPropertyCache(
    const StringSet& selector_set,
    const PropertyCache* cache, PropertyPage* page,
    MessageHandler* message_handler) {
  // We can't do anything here if page is NULL, so bail out early.
  if (page == NULL) {
    return;
  }
  // We first need to read the current critical selectors in the property cache,
  // then update it with the new set if it exists, or create it if it doesn't.
  PropertyCacheDecodeResult decode_result;
  scoped_ptr<CriticalSelectorSet> critical_selectors(
      DecodeFromPropertyCache<CriticalSelectorSet>(
          cache, page, cohort_, kCriticalSelectorsPropertyName, -1,
          &decode_result));
  switch (decode_result) {
    case kPropertyCacheDecodeOk:
      // We successfully decoded the property cache value, so use the returned
      // CriticalSelectorSet.
      break;
    case kPropertyCacheDecodeNotFound:
      // We either got here because the property cache is not set up correctly
      // (the cohort doesn't exist), or we just don't have a value already. For
      // the former, bail out since there is no use trying to update the
      // property cache if it is not setup. For the later, create a new
      // CriticalSelectorSet, since we just haven't written a value before.
      if (cohort_ == NULL) {
        return;
      }
      FALLTHROUGH_INTENDED;
    case kPropertyCacheDecodeExpired:
    case kPropertyCacheDecodeParseError:
      // We can proceed here, but we need to create a new CriticalSelectorSet.
      critical_selectors.reset(new CriticalSelectorSet);
      break;
  }

  UpdateCriticalSelectorSet(
      selector_set, SupportInterval(), critical_selectors.get());

  WriteCriticalSelectorSetToPropertyCache(
      *critical_selectors.get(), cohort_, page, message_handler);
}

bool CriticalSelectorFinder::PrepareForBeaconInsertion(
    const StringSet& selectors, RewriteDriver* driver) {
  if (selectors.empty()) {
    // Never instrument when there's nothing to check.
    return false;
  }
  DCHECK(driver->property_page() != NULL);
  CriticalSelectorSet* critical_selector_set = driver->CriticalSelectors();
  if (critical_selector_set == NULL) {
    // No critical selectors yet, create them and tell the rewrite_driver about
    // it (to avoid forcing a round-trip to the cache, and also to simplify the
    // rest of the flow here).
    critical_selector_set = new CriticalSelectorSet;
    driver->SetCriticalSelectors(critical_selector_set);
  }
  SupportMap support_map = ConvertCriticalSelectorsToSupportMap(
      *critical_selector_set, SupportInterval());
  bool is_critical_selector_set_changed = false;
  int64 now = driver->timer()->NowMs();
  if (now >= critical_selector_set->next_beacon_timestamp_ms()) {
    // TODO(jmaessen): Add noise to inter-beacon interval.  How?
    // Currently first visit to page after next_beacon_timestamp_ms will beacon.
    critical_selector_set->set_next_beacon_timestamp_ms(
        now + kMinBeaconIntervalMs);
    DecaySupportMap(SupportInterval(), &support_map);
    is_critical_selector_set_changed = true;  // Timestamp definitely changed.
  }
  // Check to see if candidate selectors are already known to pcache.  Insert
  // previously-unknown candidates with a support of 0, to indicate that beacon
  // results for those selectors will be considered valid.  Other selectors
  // returned in a beacon result will simply be ignored, avoiding DoSing the
  // pcache.  New candidate selectors cause us to re-beacon.
  for (StringSet::const_iterator i = selectors.begin(), end = selectors.end();
       i != end; ++i) {
    if (support_map.insert(pair<GoogleString, int>(*i, 0)).second) {
      is_critical_selector_set_changed = true;
    }
  }
  if (is_critical_selector_set_changed) {
    WriteSupportMapToCriticalSelectors(support_map, critical_selector_set);
    WriteCriticalSelectorSetToPropertyCache(
        *critical_selector_set, cohort_, driver->property_page(),
        driver->message_handler());
    driver->property_page()->WriteCohort(cohort_);
  }
  return is_critical_selector_set_changed;
}

}  // namespace net_instaweb
