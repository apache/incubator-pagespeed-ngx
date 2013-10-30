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

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

const char CriticalSelectorFinder::kCriticalSelectorsPropertyName[] =
    "critical_selectors";

const char CriticalSelectorFinder::kCriticalSelectorsValidCount[] =
    "critical_selectors_valid_count";

const char CriticalSelectorFinder::kCriticalSelectorsExpiredCount[] =
    "critical_selectors_expired_count";

const char CriticalSelectorFinder::kCriticalSelectorsNotFoundCount[] =
    "critical_selectors_not_found_count";

CriticalSelectorFinder::CriticalSelectorFinder(
    const PropertyCache::Cohort* cohort, NonceGenerator* nonce_generator,
    Statistics* statistics)
    : cohort_(cohort), nonce_generator_(nonce_generator) {
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

bool CriticalSelectorFinder::IsCriticalSelector(RewriteDriver* driver,
                                                const GoogleString& selector) {
  const StringSet& critical_selectors = GetCriticalSelectors(driver);
  return (critical_selectors.find(selector) != critical_selectors.end());
}

const StringSet& CriticalSelectorFinder::GetCriticalSelectors(
    RewriteDriver* driver) {
  UpdateCriticalSelectorInfoInDriver(driver);
  return driver->critical_selector_info()->critical_selectors;
}

void CriticalSelectorFinder::WriteCriticalSelectorsToPropertyCache(
    const StringSet& selector_set, StringPiece nonce, RewriteDriver* driver) {
  DCHECK(cohort_ != NULL);
  WriteCriticalSelectorsToPropertyCacheStatic(
      selector_set, nonce, SupportInterval(), ShouldReplacePriorResult(),
      driver->server_context()->page_property_cache(), cohort_,
      driver->property_page(), driver->message_handler(), driver->timer());
}

void CriticalSelectorFinder::WriteCriticalSelectorsToPropertyCacheStatic(
    const StringSet& selector_set, StringPiece nonce, int support_interval,
    bool should_replace_prior_result, const PropertyCache* cache,
    const PropertyCache::Cohort* cohort, AbstractPropertyPage* page,
    MessageHandler* message_handler, Timer* timer) {
  WriteCriticalKeysToPropertyCache(
      selector_set, nonce, support_interval, should_replace_prior_result,
      !should_replace_prior_result /* require_prior_support */,
      kCriticalSelectorsPropertyName, cache, cohort, page, message_handler,
      timer);
}

void CriticalSelectorFinder::UpdateCriticalSelectorInfoInDriver(
    RewriteDriver* driver) {
  if (driver->critical_selector_info() != NULL) {
    return;
  }

  PropertyCacheDecodeResult result;
  // NOTE: if any of these checks fail you probably didn't set up your test
  // environment carefully enough.  Figuring that out based on test failures
  // alone will drive you nuts and take hours out of your life, thus DCHECKs.
  DCHECK(driver != NULL);
  DCHECK(driver->property_page() != NULL);
  DCHECK(cohort_ != NULL);
  scoped_ptr<CriticalKeys> critical_keys(DecodeFromPropertyCache<CriticalKeys>(
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
  }

  // Create a placeholder CriticalKeys to use in case the call to
  // DecodeFromPropertyCache above returned NULL.
  CriticalKeys static_keys;
  CriticalKeys* keys_to_use =
      (critical_keys == NULL) ? &static_keys : critical_keys.get();

  CriticalSelectorInfo* critical_selector_info = new CriticalSelectorInfo;
  critical_selector_info->proto = *keys_to_use;
  GetCriticalKeysFromProto(0 /* support_percentage */, *keys_to_use,
                           &critical_selector_info->critical_selectors);
  driver->set_critical_selector_info(critical_selector_info);
}

BeaconMetadata CriticalSelectorFinder::PrepareForBeaconInsertion(
    const StringSet& selectors, RewriteDriver* driver) {
  UpdateCriticalSelectorInfoInDriver(driver);
  BeaconMetadata result;
  result.status = kDoNotBeacon;
  if (selectors.empty()) {
    return result;
  }
  if (ShouldReplacePriorResult()) {
    // The computed critical keys will not require a nonce as we trust all
    // beacon results.
    result.status = kBeaconNoNonce;
    return result;
  }
  // Avoid memory copy by capturing computed_nonce using RVA and swapping the
  // two strings.
  CriticalKeys& proto = driver->critical_selector_info()->proto;
  net_instaweb::PrepareForBeaconInsertion(
      selectors, &proto, SupportInterval(), nonce_generator_, driver->timer(),
      &result);
  if (result.status != kDoNotBeacon) {
    DCHECK(cohort_ != NULL);
    UpdateInPropertyCache(proto, cohort_, kCriticalSelectorsPropertyName,
                          true /* write_cohort */, driver->property_page());
  }
  return result;
}

void
BeaconCriticalSelectorFinder::WriteCriticalSelectorsToPropertyCacheFromBeacon(
    const StringSet& selector_set, StringPiece nonce,
    const PropertyCache* cache, const PropertyCache::Cohort* cohort,
    AbstractPropertyPage* page, MessageHandler* message_handler, Timer* timer) {
  return CriticalSelectorFinder::WriteCriticalSelectorsToPropertyCacheStatic(
      selector_set, nonce, kDefaultSupportInterval, false, cache, cohort, page,
      message_handler, timer);
}

}  // namespace net_instaweb
