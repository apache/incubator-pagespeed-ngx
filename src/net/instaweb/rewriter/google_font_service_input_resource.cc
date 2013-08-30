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

#include "net/instaweb/rewriter/public/google_font_service_input_resource.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

GoogleFontServiceInputResource::GoogleFontServiceInputResource(
    RewriteDriver* rewrite_driver,
    const StringPiece& url,
    const StringPiece& cache_key)
    : CacheableResourceBase("font_service_input_resource",
                            url, cache_key, &kContentTypeCss, rewrite_driver) {
}

GoogleFontServiceInputResource::~GoogleFontServiceInputResource() {
}

GoogleFontServiceInputResource* GoogleFontServiceInputResource::Make(
    const StringPiece& url, RewriteDriver* rewrite_driver) {
  GoogleUrl parsed_url(url);
  if (!parsed_url.IsWebValid()) {
    return NULL;
  }

  if (parsed_url.Host() != "fonts.googleapis.com") {
    return NULL;
  }

  // Compute cache key, incorporating the UA string.
  // TODO(morlovich): UA normalization goes here.
  StringPiece url_plus_ua_spec;
  scoped_ptr<GoogleUrl> url_plus_ua(parsed_url.CopyAndAddQueryParam(
      "X-PS-UA", rewrite_driver->user_agent()));
  url_plus_ua_spec = url_plus_ua->Spec();

  GoogleString cache_key;
  if (StringCaseStartsWith(url_plus_ua_spec, "http://")) {
    url_plus_ua_spec.remove_prefix(STATIC_STRLEN("http://"));
    cache_key = StrCat("gfnt://", url_plus_ua_spec);
  } else if (StringCaseStartsWith(url_plus_ua_spec, "https://")) {
    url_plus_ua_spec.remove_prefix(STATIC_STRLEN("https://"));
    cache_key = StrCat("gfnts://", url_plus_ua_spec);
  } else {
    // Huh?
    return NULL;
  }

  return new GoogleFontServiceInputResource(rewrite_driver, url, cache_key);
}

void GoogleFontServiceInputResource::InitStats(Statistics* stats) {
  CacheableResourceBase::InitStats("font_service_input_resource", stats);
}

void GoogleFontServiceInputResource::PrepareRequestHeaders(
    RequestHeaders* headers) {
  // We want to give the font service the exact UA the client used, so
  // that it can optimize for the visitor's browser, and not something
  // like Serf/1.1 mod_pagespeed/x.y
  headers->Replace(HttpAttributes::kUserAgent, rewrite_driver()->user_agent());
}

void GoogleFontServiceInputResource::PrepareResponseHeaders(
    ResponseHeaders* headers) {
  // TODO(morlovich): Check for anything suspicious, like the wrong type
  // and fail the request?

  // The resource is served with cache-control: private; we need to swizzle
  // that in order to save it in the cache.
  headers->Remove(HttpAttributes::kCacheControl, "private");

  // Remove cookies just in case.
  headers->Sanitize();
}

}  // namespace net_instaweb
