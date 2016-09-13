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

#include "net/instaweb/rewriter/public/device_properties.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

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
  void SetUserAgent(StringPiece user_agent_string);
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
  bool SupportsWebpAnimated() const;
  bool IsBot() const;
  UserAgentMatcher::DeviceType GetDeviceType() const;
  bool IsMobile() const;
  bool IsTablet() const;
  bool ForbidWebpInlining() const;
  bool AcceptsGzip() const;
  void LogDeviceInfo(AbstractLogRecord* log_record,
                     bool enable_aggressive_rewriters_for_mobile);
  bool RequestsSaveData() const;
  bool HasViaHeader() const;

 private:
  friend class ImageRewriteTest;
  FRIEND_TEST(RequestPropertiesTest, GetScreenGroupIndex);

  scoped_ptr<DeviceProperties> device_properties_;
  scoped_ptr<DownstreamCachingDirectives> downstream_caching_directives_;

  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool supports_webp_in_place_;
  mutable LazyBool supports_webp_rewritten_urls_;
  mutable LazyBool supports_webp_lossless_alpha_;
  mutable LazyBool supports_webp_animated_;

  DISALLOW_COPY_AND_ASSIGN(RequestProperties);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REQUEST_PROPERTIES_H_
