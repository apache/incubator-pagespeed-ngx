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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)

#include "net/instaweb/rewriter/public/critical_line_info_finder.h"

#include <memory>

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

const char CriticalLineInfoFinder::kCriticalLineInfoPropertyName[] =
    "critical_line_info";

CriticalLineInfoFinder::CriticalLineInfoFinder(
    const PropertyCache::Cohort* cohort)
    : cohort_(cohort) {}

CriticalLineInfoFinder::~CriticalLineInfoFinder() {}

const CriticalLineInfo* CriticalLineInfoFinder::GetCriticalLine(
    RewriteDriver* driver) {
  UpdateInDriver(driver);
  return driver->critical_line_info();
}

// Critical line configuration can come from the following sources and will be
// given preference in order:
// - Http header kXPsaSplitConfig
// - property cache value
// - domain configuration options
void CriticalLineInfoFinder::UpdateInDriver(RewriteDriver* driver) {
  if (driver->critical_line_info() != NULL) {
    return;
  }
  GoogleString critical_line_config;
  const RequestHeaders* request_headers = driver->request_headers();
  if (request_headers != NULL) {
    const char* header = request_headers->Lookup1(
        HttpAttributes::kXPsaSplitConfig);
    if (header != NULL) {
      critical_line_config = header;
    }
  }

  if (critical_line_config.empty()) {
    PropertyCacheDecodeResult pcache_status;
    scoped_ptr<CriticalLineInfo> critical_line_info(
        DecodeFromPropertyCache<CriticalLineInfo>(
            driver, cohort_, kCriticalLineInfoPropertyName,
            driver->options()->finder_properties_cache_expiration_time_ms(),
            &pcache_status));
    switch (pcache_status) {
      case kPropertyCacheDecodeNotFound:
        driver->InfoHere("Critical line info not found in cache");
        break;
      case kPropertyCacheDecodeExpired:
        driver->InfoHere("Critical line info cache entry expired");
        break;
      case kPropertyCacheDecodeParseError:
        driver->WarningHere("Unable to parse Critical line info PropertyValue");
        break;
      case kPropertyCacheDecodeOk:
        break;
    }
    driver->set_critical_line_info(critical_line_info.release());
    if (driver->critical_line_info() != NULL) {
      return;
    }

    // Pcache does not have the config. Pick it up from the domain config.
    critical_line_config = driver->options()->critical_line_config();
  }

  if (!critical_line_config.empty()) {
    // The critical_line_config string has the following format:
    // xpath1_start:xpath1_end,xpath2_start:xpath2_end,...
    // The xpath_ends are optional.
    scoped_ptr<CriticalLineInfo> critical_line_info(new CriticalLineInfo);
    StringPieceVector xpaths;
    SplitStringPieceToVector(critical_line_config, ",", &xpaths, true);
    for (int i = 0, n = xpaths.size(); i < n; ++i) {
      StringPieceVector xpath_pair;
      SplitStringPieceToVector(xpaths[i], ":", &xpath_pair, true);
      // If a particular panel specification has less than one or more than two
      // parts, we ignore the entire config.
      if (!(xpath_pair.size() == 1 || xpath_pair.size() == 2)) {
        driver->WarningHere("Unable to parse Critical line config");
        return;
      }
      Panel* panel = critical_line_info->add_panels();
      panel->set_start_xpath(xpath_pair[0].data(), xpath_pair[0].length());
      if (xpath_pair.size() == 2) {
        panel->set_end_marker_xpath(
            xpath_pair[1].data(), xpath_pair[1].length());
      }
    }
    driver->set_critical_line_info(critical_line_info.release());
    return;
  }
}

}  // namespace net_instaweb
