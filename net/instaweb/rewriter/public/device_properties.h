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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DEVICE_PROPERTIES_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DEVICE_PROPERTIES_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

class RequestHeaders;

// This class keeps track of the device properties of the client, which are
// for the most part learned from the UserAgent string.
class DeviceProperties {
 public:
  explicit DeviceProperties(UserAgentMatcher* matcher);
  virtual ~DeviceProperties();

  void SetUserAgent(const StringPiece& user_agent_string);
  // Set device-based properties that are capture in the request headers
  // (eg. the Accept: header).
  void ParseRequestHeaders(const RequestHeaders& request_headers);
  bool SupportsImageInlining() const;
  bool SupportsLazyloadImages() const;
  bool SupportsCriticalCss() const;
  bool SupportsCriticalImagesBeacon() const;
  bool SupportsJsDefer(bool enable_mobile) const;
  // SupportsWebpInPlace indicates we saw an Accept: image/webp header, and can
  // rewrite the request in place (using Vary: accept in the result headers,
  // etc.).
  bool SupportsWebpInPlace() const;
  // SupportsWebpRewrittenUrls indicates that the device can handle webp so long
  // as the url changes - either we know this based on user agent, or we got an
  // Accept header.  We can't tell a proxy cache to distinguish this case using
  // Vary: accept in the result headers, as we can't guarantee we'll see such a
  // header, ever.  So we need to Vary: user-agent or cache-control: private,
  // and thus restrict it to rewritten urls.
  bool SupportsWebpRewrittenUrls() const;
  bool SupportsWebpLosslessAlpha() const;
  bool SupportsWebpAnimated() const;
  bool IsBot() const;
  bool AcceptsGzip() const;
  UserAgentMatcher::DeviceType GetDeviceType() const;
  bool IsMobile() const {
    return GetDeviceType() == UserAgentMatcher::kMobile;
  }
  bool IsTablet() const {
    return GetDeviceType() == UserAgentMatcher::kTablet;
  }

  enum ImageQualityPreference {
    // Server uses its own default image quality.
    kImageQualityDefault,
    // The request asks for low image quality.
    kImageQualityLow,
    // The request asks for medium image quality.
    kImageQualityMedium,
    // The request asks for high image quality.
    kImageQualityHigh,
  };
  static const int kMediumScreenWidthThreshold = 720;
  static const int kLargeScreenWidthThreshold = 1500;
  bool ForbidWebpInlining() const;

  bool RequestsSaveData() const;
  bool HasViaHeader() const;

 private:
  friend class ImageRewriteTest;
  friend class RequestProperties;

  GoogleString user_agent_;
  GoogleString accept_header_;
  UserAgentMatcher* ua_matcher_;

  mutable LazyBool supports_critical_css_;
  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool requests_save_data_;
  mutable LazyBool accepts_webp_;
  mutable LazyBool accepts_gzip_;
  mutable LazyBool supports_webp_rewritten_urls_;
  mutable LazyBool supports_webp_lossless_alpha_;
  mutable LazyBool supports_webp_animated_;
  mutable LazyBool is_bot_;
  mutable LazyBool is_mobile_user_agent_;
  mutable LazyBool supports_split_html_;
  mutable LazyBool supports_flush_early_;
  const std::vector<int>* preferred_webp_qualities_;
  const std::vector<int>* preferred_jpeg_qualities_;
  // Used to lazily set device_type_.
  mutable LazyBool device_type_set_;
  mutable UserAgentMatcher::DeviceType device_type_;
  mutable LazyBool has_via_header_;

  DISALLOW_COPY_AND_ASSIGN(DeviceProperties);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DEVICE_PROPERTIES_H_
