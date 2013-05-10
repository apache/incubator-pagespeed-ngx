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
// Author: slamm@google.com (Stephen Lamm)

#include "net/instaweb/rewriter/public/critical_css_finder.h"

#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

const char CriticalCssFinder::kCriticalCssPropertyName[] =
    "critical_css";

const char CriticalCssFinder::kCriticalCssValidCount[] =
    "critical_css_valid_count";

const char CriticalCssFinder::kCriticalCssExpiredCount[] =
    "critical_css_expired_count";

const char CriticalCssFinder::kCriticalCssNotFoundCount[] =
    "critical_css_not_found_count";

CriticalCssFinder::CriticalCssFinder(Statistics* statistics) {
  critical_css_valid_count_ = statistics->GetTimedVariable(
      kCriticalCssValidCount);
  critical_css_expired_count_ = statistics->GetTimedVariable(
      kCriticalCssExpiredCount);
  critical_css_not_found_count_ = statistics->GetTimedVariable(
      kCriticalCssNotFoundCount);
}

CriticalCssFinder::~CriticalCssFinder() {
}

void CriticalCssFinder::InitStats(Statistics* statistics) {
  statistics->AddTimedVariable(kCriticalCssValidCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kCriticalCssExpiredCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kCriticalCssNotFoundCount,
                               ServerContext::kStatisticsGroup);
}

void CriticalCssFinder::UpdateCriticalCssInfoInDriver(RewriteDriver* driver) {
  if (driver->critical_css_result() != NULL) {
    return;
  }

  driver->set_critical_css_result(GetCriticalCssFromCache(driver));
}

CriticalCssResult* CriticalCssFinder::GetCriticalCss(RewriteDriver* driver) {
  if (driver->critical_css_result() != NULL) {
    return driver->critical_css_result();
  }

  UpdateCriticalCssInfoInDriver(driver);
  return driver->critical_css_result();
}

// Copy critical CSS from property cache.
CriticalCssResult* CriticalCssFinder::GetCriticalCssFromCache(
    RewriteDriver* driver) {
  PropertyCacheDecodeResult pcache_status;
  scoped_ptr<CriticalCssResult> result(
      DecodeFromPropertyCache<CriticalCssResult>(
          driver->server_context()->page_property_cache(),
          driver->fallback_property_page(),
          GetCohort(),
          kCriticalCssPropertyName,
          driver->options()->finder_properties_cache_expiration_time_ms(),
          &pcache_status));
  switch (pcache_status) {
    case kPropertyCacheDecodeNotFound:
      critical_css_not_found_count_->IncBy(1);
      driver->InfoHere("Critical CSS not found in cache");
      break;
    case kPropertyCacheDecodeExpired:
      critical_css_expired_count_->IncBy(1);
      driver->InfoHere("Critical CSS cache entry expired");
      break;
    case kPropertyCacheDecodeParseError:
      driver->WarningHere("Unable to parse Critical Css PropertyValue");
      break;
    case kPropertyCacheDecodeOk:
      critical_css_valid_count_->IncBy(1);
  }
  return result.release();
}

// Copy |critical_css_map| into property cache. Returns true on success.
bool CriticalCssFinder::UpdateCache(
    RewriteDriver* driver, const CriticalCssResult& result) {
  PropertyCacheUpdateResult status =
      UpdateInPropertyCache(
          result,
          GetCohort(),
          kCriticalCssPropertyName,
          false /* don't write cohort */,
          driver->fallback_property_page());
  switch (status) {
    case kPropertyCacheUpdateOk:
      driver->InfoHere("Critical CSS written to cache");
      return true;
    case kPropertyCacheUpdateNotFound:
      driver->WarningHere(
          "Unable to update Critical CSS PropertyValue");
      return false;
    case kPropertyCacheUpdateEncodeError:
      driver->WarningHere("Unable to serialize Critical CSS result");
      return false;
  }
  return false;
}

}  // namespace net_instaweb
