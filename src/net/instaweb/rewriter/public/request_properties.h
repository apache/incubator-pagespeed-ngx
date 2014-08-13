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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REQUEST_PROPERTIES_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REQUEST_PROPERTIES_H_

#include <vector>

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/device_properties.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"            // for scoped_ptr

namespace net_instaweb {

class DownstreamCachingDirectives;
class AbstractLogRecord;
class RequestHeaders;

// This class keeps track of the request properties of the client, which are for
// the most part learned from the UserAgent string and specific request headers
// that indicate what optimizations are supported; most properties are described
// in device_properties.h.  It relies on DeviceProperties and
// DownstreamCachingDirectives objects for deciding on support for a given
// capability.
class RequestProperties {
 public:
  explicit RequestProperties(UserAgentMatcher* matcher);
  virtual ~RequestProperties();

  // Sets the user agent string on the underlying DeviceProperties object.
  void SetUserAgent(const StringPiece& user_agent_string);
  // Calls ParseCapabilityListFromRequestHeaders on the underlying
  // DownstreamCachingDirectives object.
  void ParseRequestHeaders(const RequestHeaders& request_headers);

  bool SupportsImageInlining() const;
  bool SupportsLazyloadImages() const;
  bool SupportsCriticalCss() const;
  bool SupportsCriticalCssBeacon() const;
  bool SupportsCriticalImagesBeacon() const;
  bool SupportsJsDefer(bool enable_mobile) const;
  // Note that it's assumed that if the proxy cache SupportsWebp it also
  // supports the Accept: image/webp header (since this represents a strict
  // subset of the user agents for which SupportsWebpRewrittenUrls holds).
  bool SupportsWebpInPlace() const;
  bool SupportsWebpRewrittenUrls() const;
  bool SupportsWebpLosslessAlpha() const;
  bool IsBot() const;
  bool SupportsSplitHtml(bool enable_mobile) const;
  bool CanPreloadResources() const;
  bool GetScreenResolution(int* width, int* height) const;
  UserAgentMatcher::DeviceType GetDeviceType() const;
  bool IsMobile() const;
  bool ForbidWebpInlining() const;

  // Does not own the vectors. Callers must ensure the lifetime of vectors
  // exceeds that of the RequestProperties.
  void SetPreferredImageQualities(
      const std::vector<int>* webp,  const std::vector<int>* jpeg);
  // Returns true iff WebP and Jpeg image quality are set for the preference.
  bool GetPreferredImageQualities(
      DeviceProperties::ImageQualityPreference preference, int* webp, int* jpeg)
      const;
  static int GetPreferredImageQualityCount();

  void LogDeviceInfo(AbstractLogRecord* log_record,
                     bool enable_aggressive_rewriters_for_mobile);

 private:
  friend class ImageRewriteTest;
  FRIEND_TEST(ImageRewriteTest, SquashImagesForMobileScreen);
  FRIEND_TEST(RequestPropertiesTest, GetScreenGroupIndex);

  void SetScreenResolution(int width, int height) const;

  scoped_ptr<DeviceProperties> device_properties_;
  scoped_ptr<DownstreamCachingDirectives> downstream_caching_directives_;

  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool supports_webp_in_place_;
  mutable LazyBool supports_webp_rewritten_urls_;
  mutable LazyBool supports_webp_lossless_alpha_;

  DISALLOW_COPY_AND_ASSIGN(RequestProperties);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REQUEST_PROPERTIES_H_
