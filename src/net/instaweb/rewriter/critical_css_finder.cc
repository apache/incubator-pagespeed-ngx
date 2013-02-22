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

#include <map>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

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

// Copy critical CSS from property cache.
StringStringMap* CriticalCssFinder::CriticalCssMap(RewriteDriver* driver) {
  return DeserializeCacheData(driver, GetPropertyValue(driver));
}

// Copy |critical_css_map| into property cache. Returns true on success.
bool CriticalCssFinder::UpdateCache(
    RewriteDriver* driver, const StringStringMap& critical_css_map) {
  bool is_updated = false;

  // Update property cache if critical css is successfully determined.
  CriticalCssSet critical_css_set;
  for (StringStringMap::const_iterator it = critical_css_map.begin(),
           end = critical_css_map.end(); it != end; ++it) {
    CriticalCssSet_CriticalCss* critical_css =
        critical_css_set.add_critical_css();
    StringVector decoded_url;
    GoogleUrl gurl(it->first);
    // Decode the url if it is pagespeed encoded.
    if (driver->DecodeUrl(gurl, &decoded_url)) {
      // Combine css is turned off during compute_critical_css, so should never
      // have multiple Urls.
      DCHECK_EQ(decoded_url.size(), 1U)
          << "Found combined url " << it->first << " in compute flow for "
          << driver->base_url().Spec();
      critical_css->set_link_url(decoded_url.at(0));
    } else {
      critical_css->set_link_url(it->first);
    }
    critical_css->set_critical_rules(it->second);
  }
  GoogleString buf;
  if (critical_css_set.SerializeToString(&buf)) {
    PropertyValue* property_value = GetPropertyValue(driver);
    if (property_value) {
      PropertyCache* cache = driver->server_context()->page_property_cache();
      cache->UpdateValue(buf, property_value);
      is_updated = true;
    } else {
      LOG(WARNING) << "Unable to get Critical Css PropertyValue for update; "
                   << "url: " << driver->url();
    }
  }
  return is_updated;
}

PropertyValue* CriticalCssFinder::GetPropertyValue(RewriteDriver* driver) {
  const PropertyCache* cache = driver->server_context()->page_property_cache();
  const PropertyCache::Cohort* cohort = cache->GetCohort(GetCohort());
  PropertyPage* page = driver->property_page();
  if (cohort != NULL && page != NULL) {
    return page->GetProperty(cohort, kCriticalCssPropertyName);
  }
  return NULL;
}

// Return StringStringMap of critical CSS property cache data.
StringStringMap* CriticalCssFinder::DeserializeCacheData(
    RewriteDriver* driver, const PropertyValue* property_value) {
  StringStringMap* critical_css_map(new StringStringMap());
  if (property_value != NULL && property_value->has_value()) {
    const PropertyCache* cache =
        driver->server_context()->page_property_cache();
    int64 cache_ttl_ms =
        driver->options()->finder_properties_cache_expiration_time_ms();
    if (!cache->IsExpired(property_value, cache_ttl_ms)) {
      CriticalCssSet critical_css_set;
      ArrayInputStream input(property_value->value().data(),
                             property_value->value().size());
      if (critical_css_set.ParseFromZeroCopyStream(&input)) {
        for (int i = 0, n = critical_css_set.critical_css_size(); i < n; ++i) {
          const CriticalCssSet_CriticalCss& critical_css =
              critical_css_set.critical_css(i);
          critical_css_map->insert(make_pair(
              critical_css.link_url(),
              critical_css.critical_rules()));
        }
        critical_css_valid_count_->IncBy(1);
      } else {
        LOG(WARNING) << "Unable to parse Critical Css PropertyValue; "
                     << "url: " << driver->url();
      }
    } else {
      critical_css_expired_count_->IncBy(1);
    }
  } else {
    critical_css_not_found_count_->IncBy(1);
  }
  return critical_css_map;
}

}  // namespace net_instaweb
