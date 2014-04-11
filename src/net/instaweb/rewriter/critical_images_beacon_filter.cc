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

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"

namespace net_instaweb {

// Counters.
const char CriticalImagesBeaconFilter::kCriticalImagesBeaconAddedCount[] =
    "critical_images_beacon_filter_script_added_count";

// Onload code for img elements to detect whether they are critical or not.
const char* CriticalImagesBeaconFilter::kImageOnloadCode =
    "pagespeed.CriticalImages.checkImageForCriticality(this);";

CriticalImagesBeaconFilter::CriticalImagesBeaconFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      added_beacon_js_(false) {
  Clear();
  Statistics* stats = driver->server_context()->statistics();
  critical_images_beacon_added_count_ = stats->GetVariable(
      kCriticalImagesBeaconAddedCount);
}

CriticalImagesBeaconFilter::~CriticalImagesBeaconFilter() {}

bool CriticalImagesBeaconFilter::ShouldApply(RewriteDriver* driver) {
  // Default to not enabled.
  if (!driver->request_properties()->SupportsCriticalImagesBeacon()) {
    return false;
  }
  CriticalImagesFinder* finder =
      driver->server_context()->critical_images_finder();
  return finder->ShouldBeacon(driver);
}

void CriticalImagesBeaconFilter::DetermineEnabled() {
  // We need the filter to be enabled to track the candidate images on the page,
  // even if we aren't actually inserting the beacon JS.
  set_is_enabled(true);
  // Make sure we don't have stray unused beacon metadata from a previous
  // document.  This has caught bugs in tests / during code modification where
  // the whole filter chain isn't run and cleaned up properly.
  DCHECK_EQ(kDoNotBeacon, beacon_metadata_.status);
  DCHECK(beacon_metadata_.nonce.empty());
  DCHECK(!insert_beacon_js_);

  if (driver()->request_properties()->SupportsCriticalImagesBeacon()) {
    // Check whether we need to beacon, and store the nonce we get.
    CriticalImagesFinder* finder =
        driver()->server_context()->critical_images_finder();
    beacon_metadata_ = finder->PrepareForBeaconInsertion(driver());
    insert_beacon_js_ = (beacon_metadata_.status != kDoNotBeacon);
  }
}

void CriticalImagesBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCriticalImagesBeaconAddedCount);
}

void CriticalImagesBeaconFilter::EndDocument() {
  CriticalImagesFinder* finder =
      driver()->server_context()->critical_images_finder();
  finder->UpdateCandidateImagesForBeaconing(image_url_hashes_, driver(),
                                            insert_beacon_js_);
  Clear();
}

void CriticalImagesBeaconFilter::MaybeAddBeaconJavascript(
    HtmlElement* element) {
  if (!insert_beacon_js_ || added_beacon_js_) {
    return;
  }
  added_beacon_js_ = true;
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  GoogleString js = static_asset_manager->GetAsset(
      StaticAssetManager::kCriticalImagesBeaconJs, driver()->options());

  // Create the init string to append at the end of the static JS.
  const RewriteOptions::BeaconUrl& beacons = driver()->options()->beacon_url();
  const GoogleString* beacon_url =
      driver()->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString html_url;
  EscapeToJsStringLiteral(driver()->google_url().Spec(),
                          false, /* no quotes */
                          &html_url);
  GoogleString options_signature_hash =
      driver()->server_context()->hasher()->Hash(
          driver()->options()->signature());
  // If lazyload is enabled, it will run the beacon after it has loaded all the
  // images. Otherwise, run it at page onload.
  bool lazyload_will_beacon =
      driver()->options()->Enabled(RewriteOptions::kLazyloadImages) &&
      LazyloadImagesFilter::ShouldApply(driver()) ==
          RewriterHtmlApplication::ACTIVE;
  GoogleString send_beacon_at_onload = BoolToString(!lazyload_will_beacon);
  GoogleString resize_rendered_image_dimensions_enabled =
      BoolToString(driver()->options()->Enabled(
          RewriteOptions::kResizeToRenderedImageDimensions));
  StrAppend(&js,
            "\npagespeed.CriticalImages.Run('",
            *beacon_url, "','", html_url, "','",
            options_signature_hash, "',");
  StrAppend(&js, send_beacon_at_onload, ",",
            resize_rendered_image_dimensions_enabled, ",'",
            beacon_metadata_.nonce, "');");
  HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
  driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  // Always add the beacon js before the current node, because the current node
  // might be an img node that needs the beacon js for its
  // checkImageForCriticality onload handler.
  driver()->InsertNodeBeforeNode(element, script);
  static_asset_manager->AddJsToElement(js, script, driver());
  critical_images_beacon_added_count_->Add(1);
}

void CriticalImagesBeaconFilter::Clear() {
  beacon_metadata_.status = kDoNotBeacon;
  beacon_metadata_.nonce.clear();
  image_url_hashes_.clear();
  insert_beacon_js_ = false;
  added_beacon_js_ = false;
}

void CriticalImagesBeaconFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg &&
      element->keyword() != HtmlName::kInput) {
    return;
  }
  // TODO(jud): Verify this logic works correctly with input tags, then remove
  // the check for img tag here.
  if (element->keyword() == HtmlName::kImg && driver()->IsRewritable(element)) {
    // Add a pagespeed_url_hash attribute to the image with the hash of the
    // original URL. This is what the beacon will send back as the identifier
    // for critical images.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL && src->DecodedValueOrNull() != NULL) {
      StringPiece url(src->DecodedValueOrNull());
      GoogleUrl gurl(driver()->base_url(), url);
      if (gurl.IsAnyValid()) {
        unsigned int hash_val = HashString<CasePreserve, unsigned int>(
            gurl.spec_c_str(), strlen(gurl.spec_c_str()));
        GoogleString hash_str = UintToString(hash_val);
        image_url_hashes_.insert(hash_str);
        if (insert_beacon_js_) {
          driver()->AddAttribute(
              element, HtmlName::kPagespeedUrlHash, hash_str);
          if (element->keyword() == HtmlName::kImg &&
              CanAddPagespeedOnloadToImage(*element)) {
            // Add an onload handler only if one is not already specified on the
            // non-rewritten page.
            driver()->AddAttribute(
                element, HtmlName::kOnload, kImageOnloadCode);
            // If beacon javascript has not been added yet, we need to add it
            // before the current node because we are going to use the js for
            // the image criticality check on image-onload.
            MaybeAddBeaconJavascript(element);
          }
        }
      }
    }
  }
}

}  // namespace net_instaweb
