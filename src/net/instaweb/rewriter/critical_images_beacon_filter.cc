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
    : driver_(driver),
      added_script_(false) {
  Clear();
  Statistics* stats = driver->server_context()->statistics();
  critical_images_beacon_added_count_ = stats->GetVariable(
      kCriticalImagesBeaconAddedCount);
}

CriticalImagesBeaconFilter::~CriticalImagesBeaconFilter() {}

void CriticalImagesBeaconFilter::DetermineEnabled() {
  // Default to not enabled.
  set_is_enabled(false);

  if (!driver_->request_properties()->SupportsCriticalImagesBeacon()) {
    return;
  }

  const CriticalImagesFinder* finder =
      driver_->server_context()->critical_images_finder();
  // Instrument if we don't have any critical image information, or the critical
  // image information in the pcache is close to expiring or older than
  // RewriteOptions::beacon_reinstrument_time().
  const PropertyCache* page_property_cache =
      driver_->server_context()->page_property_cache();
  const PropertyCache::Cohort* cohort = finder->GetCriticalImagesCohort();
  PropertyPage* page = driver_->property_page();
  if (!page_property_cache->enabled() || (page == NULL) || (cohort == NULL)) {
    return;
  }
  const PropertyValue* property_value = page->GetProperty(
      cohort, CriticalImagesFinder::kCriticalImagesPropertyName);
  if (!property_value->has_value()) {
    set_is_enabled(true);
    return;
  }

  // Check if the value is expired, or past our deadline for reinstrumenting.
  int64 cache_ttl_ms =
      driver_->options()->finder_properties_cache_expiration_time_ms();
  int64 reinstrument_time =
      driver_->options()->beacon_reinstrument_time_sec() * Timer::kSecondMs;
  // TODO(jud): Add some randomness to when we reinstrument, so that
  // instrumentation is not predictable.
  int64 expiration_time = std::min(cache_ttl_ms, reinstrument_time);
  if (page_property_cache->IsExpired(property_value, expiration_time)) {
    set_is_enabled(true);
  }
}

void CriticalImagesBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCriticalImagesBeaconAddedCount);
}

void CriticalImagesBeaconFilter::StartDocument() {
  Clear();
}

void CriticalImagesBeaconFilter::Clear() {
  added_script_ = false;
}

void CriticalImagesBeaconFilter::EndElement(HtmlElement* element) {
  if (!added_script_ && element->keyword() == HtmlName::kBody) {
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
    StrAppend(&js, "\npagespeed.criticalImagesBeaconInit(");
    StrAppend(&js, "'", *beacon_url, "', ");
    StrAppend(&js, "'", html_url, "', ");
    StrAppend(&js, "'", options_signature_hash, "');");

    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->InsertNodeBeforeCurrent(script);
    static_asset_manager->AddJsToElement(js, script, driver_);

    added_script_ = true;
    critical_images_beacon_added_count_->Add(1);
  } else if (element->keyword() == HtmlName::kImg &&
             driver_->IsRewritable(element)) {
    // Add a pagespeed_url_hash attribute to the image with the hash of the
    // original URL. This is what the beacon will send back as the identifier
    // for critical images.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL && src->DecodedValueOrNull() != NULL &&
        (element->keyword() == HtmlName::kImg ||
         element->keyword() == HtmlName::kInput)) {
      StringPiece url(src->DecodedValueOrNull());
      GoogleUrl gurl(driver_->base_url(), url);
      if (gurl.is_valid()) {
        unsigned int hash_val = HashString<CasePreserve, unsigned int>(
            gurl.spec_c_str(), strlen(gurl.spec_c_str()));
        GoogleString hash_str = UintToString(hash_val);
        driver_->AddAttribute(element, HtmlName::kPagespeedUrlHash, hash_str);
      }
    }
  }
}

}  // namespace net_instaweb
