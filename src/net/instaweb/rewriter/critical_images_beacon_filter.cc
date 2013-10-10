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

#include "net/instaweb/rewriter/public/critical_images_beacon_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Counters.
const char CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount[] =
    "critical_images_beacon_filter_script_added_count";

CriticalImagesBeaconFilter::CriticalImagesBeaconFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
  Clear();
  Statistics* stats = driver->server_context()->statistics();
  critical_images_beacon_added_count_ = stats->GetVariable(
      kCriticalImagesBeaconAddedCount);
}

CriticalImagesBeaconFilter::~CriticalImagesBeaconFilter() {}

bool CriticalImagesBeaconFilter::IncludeRenderedImagesInBeacon(
    RewriteDriver* driver) {
  if (!driver->request_properties()->SupportsCriticalImagesBeacon() ||
      !driver->options()->Enabled(
          RewriteOptions::kResizeToRenderedImageDimensions)) {
    return false;
  }

  // Instrument if we don't have any rendered image dimensions information or if
  // the pcache entry has expired.
  // TODO(poojatandon): These checks should be moved to property_cache_util, or
  // to CriticalImagesFinder.
  const PropertyCache* page_property_cache =
      driver->server_context()->page_property_cache();
  PropertyPage* page = driver->property_page();
  const PropertyCache::Cohort* cohort =
      driver->server_context()->critical_images_finder()->cohort();

  if (!page_property_cache->enabled() || (page == NULL) || (cohort == NULL)) {
    return false;
  }

  const PropertyValue* property_value = page->GetProperty(
      cohort, CriticalImagesFinder::kRenderedImageDimensionsProperty);
  if (!property_value->has_value()) {
    return true;
  }

  int64 cache_ttl_ms =
      driver->options()->finder_properties_cache_expiration_time_ms();
  int64 reinstrument_time =
      driver->options()->beacon_reinstrument_time_sec() * Timer::kSecondMs;
  int64 expiration_time = std::min(cache_ttl_ms, reinstrument_time);
  if (page_property_cache->IsExpired(property_value, expiration_time)) {
    return true;
  }
  return false;
}

void CriticalImagesBeaconFilter::DetermineEnabled() {
  // Make sure we don't have stray unused beacon metadata from a previous
  // document.  This has caught bugs in tests / during code modification where
  // the whole filter chain isn't run and cleaned up properly.
  DCHECK_EQ(kDoNotBeacon, beacon_metadata_.status);
  DCHECK(beacon_metadata_.nonce.empty());

  // Default to not enabled.
  bool enabled = false;
  if (driver_->request_properties()->SupportsCriticalImagesBeacon()) {
    // Check whether we need to beacon, and store the nonce we get.
    CriticalImagesFinder* finder =
        driver_->server_context()->critical_images_finder();
    beacon_metadata_ = finder->PrepareForBeaconInsertion(driver_);
    enabled = (beacon_metadata_.status != kDoNotBeacon);
  }
  set_is_enabled(enabled);
  return;
}

void CriticalImagesBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCriticalImagesBeaconAddedCount);
}

void CriticalImagesBeaconFilter::EndDocument() {
  StaticAssetManager* static_asset_manager =
      driver_->server_context()->static_asset_manager();
  GoogleString js = static_asset_manager->GetAsset(
      StaticAssetManager::kCriticalImagesBeaconJs, driver_->options());

  // Create the init string to append at the end of the static JS.
  const RewriteOptions::BeaconUrl& beacons = driver_->options()->beacon_url();
  const GoogleString* beacon_url =
      driver_->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString html_url;
  EscapeToJsStringLiteral(driver_->google_url().Spec(),
                          false, /* no quotes */
                          &html_url);
  GoogleString options_signature_hash =
      driver_->server_context()->hasher()->Hash(
          driver_->options()->signature());
  StrAppend(&js,
            "\npagespeed.criticalImagesBeaconInit('",
            *beacon_url, "','", html_url, "','",
            options_signature_hash, "',");
  StrAppend(&js,
            BoolToString(IncludeRenderedImagesInBeacon(driver_)), ",'",
            beacon_metadata_.nonce, "');");
  Clear();
  HtmlElement* script = driver_->NewElement(NULL, HtmlName::kScript);
  driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  InsertNodeAtBodyEnd(script);
  static_asset_manager->AddJsToElement(js, script, driver_);
  critical_images_beacon_added_count_->Add(1);
}

void CriticalImagesBeaconFilter::Clear() {
  beacon_metadata_.status = kDoNotBeacon;
  beacon_metadata_.nonce.clear();
}

void CriticalImagesBeaconFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kImg && driver_->IsRewritable(element)) {
    // Add a pagespeed_url_hash attribute to the image with the hash of the
    // original URL. This is what the beacon will send back as the identifier
    // for critical images.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL && src->DecodedValueOrNull() != NULL &&
        (element->keyword() == HtmlName::kImg ||
         element->keyword() == HtmlName::kInput)) {
      StringPiece url(src->DecodedValueOrNull());
      GoogleUrl gurl(driver_->base_url(), url);
      if (gurl.IsAnyValid()) {
        unsigned int hash_val = HashString<CasePreserve, unsigned int>(
            gurl.spec_c_str(), strlen(gurl.spec_c_str()));
        GoogleString hash_str = UintToString(hash_val);
        driver_->AddAttribute(element, HtmlName::kPagespeedUrlHash, hash_str);
      }
    }
  }
}

}  // namespace net_instaweb
