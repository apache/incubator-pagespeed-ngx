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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/request_context.h"
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

void SplitHtmlBeaconFilter::DetermineEnabled() {
  set_is_enabled(ShouldApply(driver_));
}

void SplitHtmlBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kSplitHtmlBeaconAddedCount);
}

bool SplitHtmlBeaconFilter::ShouldApply(RewriteDriver* driver) {
  // TODO(jud): Default to not enabled and check if we have split HTML beacon
  // results in the property cache alreay to determine if we need to beacon once
  // the CriticalLineInfoFinder class exists.

  // Do not instrument if the x_split query param was set to request either the
  // above or below the fold content.
  bool is_split_request = driver->request_context()->split_request_type() !=
                          RequestContext::SPLIT_FULL;
  return (!is_split_request &&
          driver->server_context()->factory()->UseBeaconResultsInFilters() &&
          driver->options()->Enabled(RewriteOptions::kSplitHtml));
}

void SplitHtmlBeaconFilter::EndDocument() {
  StaticAssetManager* static_asset_manager =
      driver_->server_context()->static_asset_manager();
  GoogleString js = static_asset_manager->GetAsset(
      StaticAssetManager::kSplitHtmlBeaconJs, driver_->options());

  // Create the init string to append at the end of the static JS.
  const RewriteOptions::BeaconUrl& beacons = driver_->options()->beacon_url();
  const GoogleString* beacon_url =
      driver_->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString html_url;
  EscapeToJsStringLiteral(driver_->google_url().Spec(), false, /* no quotes */
                          &html_url);
  GoogleString options_signature_hash = driver_->server_context()->hasher()
      ->Hash(driver_->options()->signature());

  // TODO(jud): Add a call to CriticalLineInfoFinder::PrepareForBeaconInsertion
  // to get the nonce when that class has been created.
  GoogleString nonce;
  StrAppend(&js, "\npagespeed.splitHtmlBeaconInit(");
  StrAppend(&js, "'", *beacon_url, "', ");
  StrAppend(&js, "'", html_url, "', ");
  StrAppend(&js, "'", options_signature_hash, "', ");
  StrAppend(&js, "'", nonce, "');");

  HtmlElement* script = driver_->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script);
  static_asset_manager->AddJsToElement(js, script, driver_);
  driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  split_html_beacon_added_count_->Add(1);
}

}  // namespace net_instaweb
