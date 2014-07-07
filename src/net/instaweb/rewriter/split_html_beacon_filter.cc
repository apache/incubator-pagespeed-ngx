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

#include "net/instaweb/rewriter/public/split_html_beacon_filter.h"

#include <algorithm>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/beacon_critical_line_info_finder.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_line_info_finder.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

// Counters.
const char SplitHtmlBeaconFilter::kSplitHtmlBeaconAddedCount[] =
    "split_html_beacon_filter_script_added_count";

SplitHtmlBeaconFilter::SplitHtmlBeaconFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
  Statistics* stats = driver->server_context()->statistics();
  split_html_beacon_added_count_ =
      stats->GetVariable(kSplitHtmlBeaconAddedCount);
}

void SplitHtmlBeaconFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(ShouldApply(driver()));
}

void SplitHtmlBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kSplitHtmlBeaconAddedCount);
}

bool SplitHtmlBeaconFilter::ShouldApply(RewriteDriver* driver) {
  if (driver->request_properties()->IsBot()) {
    return false;
  }
  // Do not instrument if the x_split query param was set to request either the
  // above or below the fold content.
  bool is_split_request = driver->request_context()->split_request_type() !=
                          RequestContext::SPLIT_FULL;
  if (is_split_request ||
      !driver->server_context()->factory()->UseBeaconResultsInFilters() ||
      !driver->options()->Enabled(RewriteOptions::kSplitHtml)) {
    return false;
  }

  const CriticalLineInfoFinder* finder =
      driver->server_context()->critical_line_info_finder();

  // Check if we have critical line info in the pcache, and only beacon if it
  // is missing or expired.
  // TODO(jud): We need a smarter reinstrumentation strategy here than just
  // waiting for the pcache to expire. To start, we need to collect an adequate
  // number of samples in the beginning until we reach a steady state, and then
  // back off our sampling rate. Then, we should detect when the page changes
  // substantially and increase beaconing rate again until we've collected
  // enough samples on the updated page. We also should detect the case where we
  // aren't receiving beacons correctly for whatever reason, and stop
  // instrumenting, since this beacon is more computationally expensive than say
  // the critical image beacon.
  int64 expiration_time_ms = std::min(
      driver->options()->finder_properties_cache_expiration_time_ms(),
      driver->options()->beacon_reinstrument_time_sec() * Timer::kSecondMs);
  PropertyCacheDecodeResult result;
  scoped_ptr<CriticalKeys> critical_keys(DecodeFromPropertyCache<CriticalKeys>(
      driver, finder->cohort(),
      BeaconCriticalLineInfoFinder::kBeaconCriticalLineInfoPropertyName,
      expiration_time_ms, &result));
  return result != kPropertyCacheDecodeOk;
}

void SplitHtmlBeaconFilter::EndDocument() {
  BeaconMetadata beacon_metadata = driver()->server_context()
                                       ->critical_line_info_finder()
                                       ->PrepareForBeaconInsertion(driver());
  if (beacon_metadata.status == kDoNotBeacon) {
    return;
  }
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  GoogleString js = static_asset_manager->GetAsset(
      StaticAssetManager::kSplitHtmlBeaconJs, driver()->options());

  // Create the init string to append at the end of the static JS.
  const RewriteOptions::BeaconUrl& beacons = driver()->options()->beacon_url();
  const GoogleString* beacon_url =
      driver()->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString html_url;
  EscapeToJsStringLiteral(driver()->google_url().Spec(), false, /* no quotes */
                          &html_url);
  GoogleString options_signature_hash = driver()->server_context()->hasher()
      ->Hash(driver()->options()->signature());

  StrAppend(&js, "\npagespeed.splitHtmlBeaconInit(");
  StrAppend(&js, "'", *beacon_url, "', ");
  StrAppend(&js, "'", html_url, "', ");
  StrAppend(&js, "'", options_signature_hash, "', ");
  StrAppend(&js, "'", beacon_metadata.nonce, "');");

  HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script);
  static_asset_manager->AddJsToElement(js, script, driver());
  driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  split_html_beacon_added_count_->Add(1);
}

}  // namespace net_instaweb
