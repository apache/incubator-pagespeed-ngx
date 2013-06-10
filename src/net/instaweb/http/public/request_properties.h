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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_REQUEST_PROPERTIES_H_
#define NET_INSTAWEB_HTTP_PUBLIC_REQUEST_PROPERTIES_H_

#include <vector>

#include "net/instaweb/http/public/device_properties.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// This class keeps track of the request properties of the client, which are
// for the most part learned from the UserAgent string and specific request
// headers that indicate what optimizations are supported.
class RequestProperties {
 public:
  explicit RequestProperties(UserAgentMatcher* matcher);
  virtual ~RequestProperties();

  void set_user_agent(const StringPiece& user_agent_string);
  bool SupportsImageInlining() const;
  bool SupportsLazyloadImages() const;
  bool SupportsCriticalImagesBeacon() const;
  bool SupportsJsDefer(bool enable_mobile) const;
  bool SupportsWebp() const;
  bool SupportsWebpLosslessAlpha() const;
  bool IsBot() const;
  bool SupportsSplitHtml(bool enable_mobile) const;
  bool CanPreloadResources() const;
  bool GetScreenResolution(int* width, int* height) const;
  UserAgentMatcher::DeviceType GetDeviceType() const;
  bool IsMobile() const;

  // Does not own the vectors. Callers must ensure the lifetime of vectors
  // exceeds that of the RequestProperties.
  void SetPreferredImageQualities(
      const std::vector<int>* webp,  const std::vector<int>* jpeg);
  // Returns true iff WebP and Jpeg image quality are set for the preference.
  bool GetPreferredImageQualities(
      DeviceProperties::ImageQualityPreference preference, int* webp, int* jpeg)
      const;
  static int GetPreferredImageQualityCount();

 private:
  friend class ImageRewriteTest;
  FRIEND_TEST(ImageRewriteTest, SquashImagesForMobileScreen);
  FRIEND_TEST(RequestPropertiesTest, GetScreenGroupIndex);

  void SetScreenResolution(int width, int height) const;

  scoped_ptr<DeviceProperties> device_properties_;

  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool supports_webp_;
  mutable LazyBool supports_webp_lossless_alpha_;

  DISALLOW_COPY_AND_ASSIGN(RequestProperties);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_PROPERTIES_H_
