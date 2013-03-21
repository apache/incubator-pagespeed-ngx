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

#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/message_handler.h"
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
    const CriticalSelectorSet& selectors, RewriteDriver* driver) {
  PropertyCacheUpdateResult result =
      UpdateInPropertyCache(
          selectors, driver, cohort_, kCriticalSelectorsPropertyName,
          false /* don't write cohort*/);
  switch (result) {
    case kPropertyCacheUpdateNotFound:
      driver->message_handler()->Message(
          kWarning, "Unable to get Critical css selector set for update; "
          "url: %s", driver->url());
      break;
    case kPropertyCacheUpdateEncodeError:
      driver->message_handler()->Message(
          kWarning, "Trouble marshaling CriticalSelectorSet!?");
      break;
    case kPropertyCacheUpdateOk:
      // Nothing more to do.
      break;
  }
}

}  // namespace net_instaweb
