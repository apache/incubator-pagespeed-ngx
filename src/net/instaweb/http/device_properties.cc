// Copyright 2012 Google Inc. All Rights Reserved.
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

#include "net/instaweb/http/public/device_properties.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"

namespace net_instaweb {

DeviceProperties::DeviceProperties(UserAgentMatcher* matcher)
    : ua_matcher_(matcher), supports_image_inlining_(kNotSet),
      supports_js_defer_(kNotSet), supports_webp_(kNotSet),
      supports_webp_lossless_alpha_(kNotSet), is_mobile_user_agent_(kNotSet),
      supports_split_html_(kNotSet), supports_flush_early_(kNotSet) {}

DeviceProperties::~DeviceProperties() {
}

void DeviceProperties::set_user_agent(const StringPiece& user_agent_string) {
  user_agent_string.CopyToString(&user_agent_);
}

bool DeviceProperties::SupportsImageInlining() const {
  if (supports_image_inlining_ == kNotSet) {
    supports_image_inlining_ =
        ua_matcher_->SupportsImageInlining(user_agent_) ? kTrue : kFalse;
  }
  return (supports_image_inlining_ == kTrue);
}

bool DeviceProperties::SupportsCriticalImagesBeacon() const {
  // For now this script has the same user agent requirements as image inlining,
  // however that could change in the future if more advanced JS is used by the
  // beacon.
  return SupportsImageInlining();
}

// Note that the result of the function is cached as supports_js_defer_. This
// must be cleared before calling the function a second time with a different
// value for allow_mobile.
bool DeviceProperties::SupportsJsDefer(bool allow_mobile) const {
  if (supports_js_defer_ == kNotSet) {
    supports_js_defer_ =
        ua_matcher_->SupportsJsDefer(user_agent_, allow_mobile) ?
        kTrue : kFalse;
  }
  return (supports_js_defer_ == kTrue);
}

bool DeviceProperties::SupportsWebp() const {
  if (supports_webp_ == kNotSet) {
    supports_webp_ =
        ua_matcher_->SupportsWebp(user_agent_) ? kTrue : kFalse;
  }
  return (supports_webp_ == kTrue);
}

bool DeviceProperties::SupportsWebpLosslessAlpha() const {
  if (supports_webp_lossless_alpha_ == kNotSet) {
    supports_webp_lossless_alpha_ =
        ua_matcher_->SupportsWebpLosslessAlpha(user_agent_) ?
        kTrue : kFalse;
  }
  return (supports_webp_lossless_alpha_ == kTrue);
}

bool DeviceProperties::IsMobileUserAgent() const {
  if (is_mobile_user_agent_ == kNotSet) {
    is_mobile_user_agent_ =
        ua_matcher_->IsMobileUserAgent(user_agent_) ? kTrue : kFalse;
  }
  return (is_mobile_user_agent_ == kTrue);
}

bool DeviceProperties::SupportsSplitHtml(bool allow_mobile) const {
  if (supports_split_html_ == kNotSet) {
    supports_split_html_ =
        ua_matcher_->SupportsSplitHtml(user_agent_, allow_mobile) ?
        kTrue : kFalse;
  }
  return (supports_split_html_ == kTrue);
}

bool DeviceProperties::CanPreloadResources(
    const RequestHeaders* req_hdrs) const {
  return ua_matcher_->GetPrefetchMechanism(user_agent_, req_hdrs) !=
      UserAgentMatcher::kPrefetchNotSupported;
}

}  // namespace net_instaweb
