// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/rewriter/public/downstream_caching_directives.h"

#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const char DownstreamCachingDirectives::kNoCapabilitiesSpecified[] =
    "NoCapabilitiesSpecified";

DownstreamCachingDirectives::DownstreamCachingDirectives()
    : supports_image_inlining_(kNotSet),
      supports_js_defer_(kNotSet),
      supports_lazyload_images_(kNotSet),
      supports_webp_(kNotSet),
      supports_webp_lossless_alpha_(kNotSet),
      capabilities_to_be_supported_(kNoCapabilitiesSpecified) {
}

DownstreamCachingDirectives::~DownstreamCachingDirectives() {
}

void DownstreamCachingDirectives::ParseCapabilityListFromRequestHeaders(
    const RequestHeaders& request_headers) {
  const char* capabilities = request_headers.Lookup1(kPsaCapabilityList);
  if (capabilities != NULL) {
    capabilities_to_be_supported_ = capabilities;
  }
  // Reset everything.
  supports_image_inlining_ = kNotSet;
  supports_js_defer_ = kNotSet;
  supports_lazyload_images_ = kNotSet;
  supports_webp_ = kNotSet;
  supports_webp_lossless_alpha_ = kNotSet;
}

bool DownstreamCachingDirectives::IsPropertySupported(
    LazyBool* stored_property_support,
    const GoogleString& capability,
    const GoogleString& supported_capabilities) {
  if (*stored_property_support == kNotSet) {
    if (supported_capabilities ==
        DownstreamCachingDirectives::kNoCapabilitiesSpecified) {
      *stored_property_support = kTrue;
    } else if (supported_capabilities == capability) {
      // Matches "ii" exactly.
      *stored_property_support = kTrue;
    } else if (supported_capabilities.find(StrCat(capability, ":")) == 0) {
      // Matches "ii:" or "ii:abc".
      *stored_property_support = kTrue;
    } else if (supported_capabilities.find(StrCat(",", capability, ":")) !=
               GoogleString::npos) {
      // Matches "abc,ii:" or "abc,ii:xyz".
      *stored_property_support = kTrue;
    } else if (supported_capabilities.find(StrCat(capability, ",")) == 0) {
      // Matches "ii," or "ii,abc".
      *stored_property_support = kTrue;
    } else if (supported_capabilities.find(StrCat(",", capability, ",")) !=
               GoogleString::npos) {
      // Matches "abc,ii," or "abc,ii,xyz".
      *stored_property_support = kTrue;
    } else {
      *stored_property_support = kFalse;
    }
  }
  return (*stored_property_support == kTrue);
}

bool DownstreamCachingDirectives::SupportsImageInlining() const {
  return IsPropertySupported(
             &supports_image_inlining_,
             RewriteOptions::FilterId(RewriteOptions::kInlineImages),
             capabilities_to_be_supported_);
}

bool DownstreamCachingDirectives::SupportsLazyloadImages() const {
  return IsPropertySupported(
             &supports_lazyload_images_,
             RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
             capabilities_to_be_supported_);
}

bool DownstreamCachingDirectives::SupportsJsDefer() const {
  return IsPropertySupported(
             &supports_js_defer_,
             RewriteOptions::FilterId(RewriteOptions::kDeferJavascript),
             capabilities_to_be_supported_);
}

bool DownstreamCachingDirectives::SupportsWebp() const {
  return IsPropertySupported(
             &supports_webp_,
             RewriteOptions::FilterId(RewriteOptions::kConvertJpegToWebp),
             capabilities_to_be_supported_);
}

bool DownstreamCachingDirectives::SupportsWebpLosslessAlpha() const {
  return IsPropertySupported(
             &supports_webp_lossless_alpha_,
             RewriteOptions::FilterId(RewriteOptions::kConvertToWebpLossless),
             capabilities_to_be_supported_);
}

}  // namespace net_instaweb
