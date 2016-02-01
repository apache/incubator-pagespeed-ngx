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

#include "net/instaweb/rewriter/public/device_properties.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/bot_checker.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

DeviceProperties::DeviceProperties(UserAgentMatcher* matcher)
    : ua_matcher_(matcher),
      supports_critical_css_(kNotSet),
      supports_image_inlining_(kNotSet),
      supports_js_defer_(kNotSet),
      supports_lazyload_images_(kNotSet),
      accepts_webp_(kNotSet),
      supports_webp_rewritten_urls_(kNotSet),
      supports_webp_lossless_alpha_(kNotSet),
      supports_webp_animated_(kNotSet),
      is_bot_(kNotSet),
      is_mobile_user_agent_(kNotSet),
      supports_split_html_(kNotSet),
      supports_flush_early_(kNotSet),
      device_type_set_(kNotSet),
      device_type_(UserAgentMatcher::kDesktop) {
}

DeviceProperties::~DeviceProperties() {
}

void DeviceProperties::SetUserAgent(const StringPiece& user_agent_string) {
  user_agent_string.CopyToString(&user_agent_);

  // Reset everything determined by user agent.
  supports_critical_css_ = kNotSet;
  supports_image_inlining_ = kNotSet;
  supports_js_defer_ = kNotSet;
  supports_lazyload_images_ = kNotSet;
  supports_webp_rewritten_urls_ = kNotSet;
  supports_webp_lossless_alpha_ = kNotSet;
  supports_webp_animated_ = kNotSet;
  is_bot_ = kNotSet;
  is_mobile_user_agent_ = kNotSet;
  supports_split_html_ = kNotSet;
  supports_flush_early_ = kNotSet;
}

void DeviceProperties::ParseRequestHeaders(
    const RequestHeaders& request_headers) {
  DCHECK_EQ(kNotSet, accepts_webp_) << "Double call to ParseRequestHeaders";
  accepts_webp_ =
      request_headers.HasValue(HttpAttributes::kAccept,
                               kContentTypeWebp.mime_type()) ?
      kTrue : kFalse;
  accepts_gzip_ =
      request_headers.HasValue(HttpAttributes::kAcceptEncoding,
                               HttpAttributes::kGzip) ?
      kTrue : kFalse;
}

bool DeviceProperties::AcceptsGzip() const {
  if (accepts_gzip_ == kNotSet) {
    LOG(DFATAL) << "Check of AcceptsGzip before value is set.";
    accepts_gzip_ = kFalse;
  }
  return (accepts_gzip_ == kTrue);
}

bool DeviceProperties::SupportsImageInlining() const {
  if (supports_image_inlining_ == kNotSet) {
    supports_image_inlining_ =
        ua_matcher_->SupportsImageInlining(user_agent_) ? kTrue : kFalse;
  }
  return (supports_image_inlining_ == kTrue);
}

bool DeviceProperties::SupportsLazyloadImages() const {
  if (supports_lazyload_images_ == kNotSet) {
    supports_lazyload_images_ =
        (!IsBot() && ua_matcher_->SupportsLazyloadImages(user_agent_)) ?
        kTrue : kFalse;
  }
  return (supports_lazyload_images_ == kTrue);
}

bool DeviceProperties::SupportsCriticalCss() const {
  // Currently CriticalSelectorFilter can't deal with IE conditional comments,
  // so we disable ourselves for IE.
  // TODO(morlovich): IE10 in strict mode disables the conditional comments
  // feature; but the strict mode is determined by combination of doctype and
  // X-UA-Compatible, which can come in both meta and header flavors. Once we
  // have a good way of detecting this case, we can enable us for strict IE10.
  if (supports_critical_css_ == kNotSet) {
    supports_critical_css_ = !ua_matcher_->IsIe(user_agent_) ? kTrue : kFalse;
  }
  return (supports_critical_css_ == kTrue);
}

bool DeviceProperties::SupportsCriticalImagesBeacon() const {
  // For now this script has the same user agent requirements as image inlining,
  // however that could change in the future if more advanced JS is used by the
  // beacon. Also disable for bots. See
  // https://code.google.com/p/modpagespeed/issues/detail?id=813.
  return SupportsImageInlining() && !IsBot();
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

bool DeviceProperties::SupportsWebpInPlace() const {
  // We used to check accepts_webp_ == kNotSet here, but many tests don't bother
  // setting request headers.  So we simply use kNotSet to detect
  // double-initialization above.
  return (accepts_webp_ == kTrue);
}

// TODO(huibao): Only use "accept: image/webp" header to determine whether and
// which format of WebP is supported. Currently there are some browsers which
// have "accept: image/webp" but only support lossy/lossless format, and some
// browsers which don't have "accept" header but support lossy format. Once
// the market share of these browsers is small enough, we can simplify the logic
// by only checking the "accept" header.
bool DeviceProperties::SupportsWebpRewrittenUrls() const {
  if (supports_webp_rewritten_urls_ == kNotSet) {
    if ((accepts_webp_ == kTrue) || ua_matcher_->LegacyWebp(user_agent_)) {
      supports_webp_rewritten_urls_ = kTrue;
    } else {
      supports_webp_rewritten_urls_ = kFalse;
    }
  }
  return (supports_webp_rewritten_urls_ == kTrue);
}

bool DeviceProperties::SupportsWebpLosslessAlpha() const {
  if (supports_webp_lossless_alpha_ == kNotSet) {
    if ((accepts_webp_ == kTrue) &&
        ua_matcher_->SupportsWebpLosslessAlpha(user_agent_)) {
      supports_webp_lossless_alpha_ = kTrue;
    } else {
      supports_webp_lossless_alpha_ = kFalse;
    }
  }
  return (supports_webp_lossless_alpha_ == kTrue);
}

bool DeviceProperties::SupportsWebpAnimated() const {
  if (supports_webp_animated_ == kNotSet) {
    if ((accepts_webp_ == kTrue) &&
        ua_matcher_->SupportsWebpAnimated(user_agent_)) {
      supports_webp_animated_ = kTrue;
    } else {
      supports_webp_animated_ = kFalse;
    }
  }
  return (supports_webp_animated_ == kTrue);
}

bool DeviceProperties::IsBot() const {
  if (is_bot_ == kNotSet) {
    is_bot_ = BotChecker::Lookup(user_agent_) ? kTrue : kFalse;
  }
  return (is_bot_ == kTrue);
}

bool DeviceProperties::SupportsSplitHtml(bool allow_mobile) const {
  if (supports_split_html_ == kNotSet) {
    supports_split_html_ =
        ua_matcher_->SupportsSplitHtml(user_agent_, allow_mobile) ?
        kTrue : kFalse;
  }
  return (supports_split_html_ == kTrue);
}

bool DeviceProperties::CanPreloadResources() const {
  return ua_matcher_->GetPrefetchMechanism(user_agent_) !=
      UserAgentMatcher::kPrefetchNotSupported;
}

UserAgentMatcher::DeviceType DeviceProperties::GetDeviceType() const {
  if (device_type_set_ == kNotSet) {
    device_type_ = ua_matcher_->GetDeviceTypeForUA(user_agent_);
    device_type_set_ = kTrue;
  }
  return device_type_;
}

// Chrome 36 on iOS devices failed to display inlined WebP image, so inlining
// WebP on these devices is forbidden.
// https://code.google.com/p/chromium/issues/detail?id=402514
bool DeviceProperties::ForbidWebpInlining() const {
  if (ua_matcher_->IsiOSUserAgent(user_agent_)) {
    int major = kNotSet;
    int minor = kNotSet;
    int build = kNotSet;
    int patch = kNotSet;
    if (ua_matcher_->GetChromeBuildNumber(user_agent_, &major, &minor, &build,
                                          &patch) &&
        (major == 36 || major == 37)) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
