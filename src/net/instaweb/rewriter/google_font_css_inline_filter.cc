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
//
// Filter that inlines small loader CSS files made by Google Font Service.

#include "net/instaweb/rewriter/public/google_font_css_inline_filter.h"

#include "net/instaweb/rewriter/public/google_font_service_input_resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

GoogleFontCssInlineFilter::GoogleFontCssInlineFilter(RewriteDriver* driver)
    : CssInlineFilter(driver) {
  set_id(RewriteOptions::kGoogleFontCssInlineId);
  driver->AddResourceUrlClaimant(
      NewPermanentCallback(
          this, &GoogleFontCssInlineFilter::CheckIfFontServiceUrl));
}

GoogleFontCssInlineFilter::~GoogleFontCssInlineFilter() {
}

void GoogleFontCssInlineFilter::InitStats(Statistics* statistics) {
  GoogleFontServiceInputResource::InitStats(statistics);
}

ResourcePtr GoogleFontCssInlineFilter::CreateResource(const char* url) {
  GoogleUrl abs_url;
  ResolveUrl(url, &abs_url);
  ResourcePtr resource(GoogleFontServiceInputResource::Make(abs_url, driver()));
  if (resource.get() != NULL) {
    // Unfortunately some options prevent us from doing anything, since they
    // can make the HTML cached in a way unaware of font UA dependencies.
    const RewriteOptions* options = driver()->options();
    if (!options->modify_caching_headers()) {
      ResetAndExplainReason(
          "Cannot inline font loader CSS when ModifyCachingHeaders is off",
          &resource);
    }

    if (!options->downstream_cache_purge_location_prefix().empty()) {
      ResetAndExplainReason(
          "Cannot inline font loader CSS when using downstream cache",
          &resource);
    }
  }
  return resource;
}

void GoogleFontCssInlineFilter::ResetAndExplainReason(
    const char* reason, ResourcePtr* resource) {
  resource->reset(NULL);
  if (DebugMode()) {
    // Note that since we only call this after a success of
    // GoogleFontServiceInputResource::Make, this will only be adding comments
    // near font links, and not anything else.
    driver()->InsertComment(reason);
  }
}

void GoogleFontCssInlineFilter::CheckIfFontServiceUrl(
    const GoogleUrl& url, bool* result) {
  *result = GoogleFontServiceInputResource::IsFontServiceUrl(url);
}

}  // namespace net_instaweb
