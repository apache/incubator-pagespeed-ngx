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

#include <vector>

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/user_agent_normalizer.h"

namespace net_instaweb {

GoogleFontServiceInputResource::GoogleFontServiceInputResource(
    RewriteDriver* rewrite_driver,
    bool is_https,
    const StringPiece& url,
    const StringPiece& cache_key,
    const GoogleString& user_agent)
    : CacheableResourceBase("font_service_input_resource",
                            url, cache_key, &kContentTypeCss, rewrite_driver),
      user_agent_(user_agent),
      is_https_(is_https) {
}

GoogleFontServiceInputResource::~GoogleFontServiceInputResource() {
}

GoogleFontServiceInputResource* GoogleFontServiceInputResource::Make(
    const GoogleUrl& parsed_url, RewriteDriver* rewrite_driver) {
  if (!parsed_url.IsWebValid()) {
    return NULL;
  }

  if (parsed_url.Host() != "fonts.googleapis.com") {
    return NULL;
  }

  // Compute cache key, incorporating the UA string --- but normalize it first,
  // to cut down on irrelevant noise.
  const std::vector<const UserAgentNormalizer*>& ua_normalizers =
      rewrite_driver->server_context()->factory()->user_agent_normalizers();
  GoogleString ua = UserAgentNormalizer::NormalizeWithAll(
                        ua_normalizers, rewrite_driver->user_agent());

  StringPiece url_plus_ua_spec;
  scoped_ptr<GoogleUrl> url_plus_ua(
      parsed_url.CopyAndAddQueryParam("X-PS-UA", ua));
  url_plus_ua_spec = url_plus_ua->Spec();

  GoogleString cache_key;
  bool is_https = false;
  if (StringCaseStartsWith(url_plus_ua_spec, "http://")) {
    url_plus_ua_spec.remove_prefix(STATIC_STRLEN("http://"));
    cache_key = StrCat("gfnt://", url_plus_ua_spec);
    is_https = false;
  } else if (StringCaseStartsWith(url_plus_ua_spec, "https://")) {
    url_plus_ua_spec.remove_prefix(STATIC_STRLEN("https://"));
    cache_key = StrCat("gfnts://", url_plus_ua_spec);
    is_https = true;
  } else {
    // Huh?
    return NULL;
  }

  return new GoogleFontServiceInputResource(
      rewrite_driver, is_https, parsed_url.Spec(), cache_key,
      rewrite_driver->user_agent());
}

void GoogleFontServiceInputResource::InitStats(Statistics* stats) {
  CacheableResourceBase::InitStats("font_service_input_resource", stats);
}

void GoogleFontServiceInputResource::PrepareRequest(
    const RequestContextPtr& request_context, RequestHeaders* headers) {
  // We want to give the font service the UA the client used, so that it can
  // optimize for the visitor's browser, and not something like Serf/1.1
  // mod_pagespeed/x.y
  headers->Replace(HttpAttributes::kUserAgent, user_agent_);

  request_context->AddSessionAuthorizedFetchOrigin(
      is_https_ ?
          "https://fonts.googleapis.com" : "http://fonts.googleapis.com");
}

void GoogleFontServiceInputResource::PrepareResponseHeaders(
    ResponseHeaders* headers) {
  // Refuse to deal with anything but CSS.
  const ContentType* content_type = headers->DetermineContentType();
  if (content_type == NULL || !content_type->IsCss()) {
    headers->set_status_code(HttpStatus::kNotAcceptable);
  }

  // The resource is served with cache-control: private; we need to swizzle
  // that in order to save it in the cache.
  headers->Remove(HttpAttributes::kCacheControl, "private");

  // Remove cookies just in case.
  headers->Sanitize();
}

}  // namespace net_instaweb
