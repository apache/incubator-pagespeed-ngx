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

#include <set>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char CriticalSelectorFinder::kCriticalSelectorsPropertyName[] =
    "critical_selectors";

const char CriticalSelectorFinder::kCriticalSelectorsValidCount[] =
    "critical_selectors_valid_count";

const char CriticalSelectorFinder::kCriticalSelectorsExpiredCount[] =
    "critical_selectors_expired_count";

const char CriticalSelectorFinder::kCriticalSelectorsNotFoundCount[] =
    "critical_selectors_not_found_count";

CriticalSelectorFinder::CriticalSelectorFinder(StringPiece cohort,
                                               Statistics* statistics) {
  cohort.CopyToString(&cohort_);

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
      if (cache->GetCohort(cohort_) == NULL) {
        return;
      }
      FALLTHROUGH_INTENDED;
    case kPropertyCacheDecodeExpired:
    case kPropertyCacheDecodeParseError:
      // We can proceed here, but we need to create a new CriticalSelectorSet.
      critical_selectors.reset(new CriticalSelectorSet);
      break;
  }

  UpdateCriticalSelectorSet(selector_set, critical_selectors.get());

  PropertyCacheUpdateResult result =
      UpdateInPropertyCache(
          *critical_selectors, cache, cohort_, kCriticalSelectorsPropertyName,
          false /* don't write cohort*/, page);
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

void CriticalSelectorFinder::UpdateCriticalSelectorSet(
    const StringSet& new_set, CriticalSelectorSet* critical_selector_set) {
  DCHECK(critical_selector_set != NULL);

  // Update the selector_sets field first, which contains the history of up to
  // NumSetsToKeep() critical selector responses. If we already have
  // NumSetsToKeep(), drop the first (oldest) response, and append the new
  // response to the end.
  CriticalSelectorSet::BeaconResponse* new_selector_set;
  if (critical_selector_set->selector_set_history_size() >= NumSetsToKeep()) {
    DCHECK_EQ(critical_selector_set->selector_set_history_size(),
              NumSetsToKeep());
    protobuf::RepeatedPtrField<CriticalSelectorSet::BeaconResponse>*
        selector_history =
            critical_selector_set->mutable_selector_set_history();
    for (int i = 1; i < selector_history->size(); ++i) {
      selector_history->SwapElements(i - 1, i);
    }
    new_selector_set = selector_history->Mutable(selector_history->size() - 1);
    new_selector_set->Clear();
  } else {
    new_selector_set = critical_selector_set->add_selector_set_history();
  }

  for (StringSet::const_iterator it = new_set.begin();
       it != new_set.end(); ++it) {
    new_selector_set->add_selectors(*it);
  }

  // Now recalculate the critical_selectors field as the union of all selectors
  // reported by beacons. Aggregate all the selectors into a StringSet first to
  // remove duplicates.
  StringSet new_critical_selectors;
  for (int i = 0; i < critical_selector_set->selector_set_history_size(); ++i) {
    const CriticalSelectorSet::BeaconResponse& curr_set =
        critical_selector_set->selector_set_history(i);
    for (int j = 0; j < curr_set.selectors_size(); ++j) {
      new_critical_selectors.insert(curr_set.selectors(j));
    }
  }

  critical_selector_set->clear_critical_selectors();
  for (StringSet::const_iterator it = new_critical_selectors.begin();
       it != new_critical_selectors.end(); ++it) {
    critical_selector_set->add_critical_selectors(*it);
  }
}

}  // namespace net_instaweb
