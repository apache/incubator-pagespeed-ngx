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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_DEVICE_PROPERTIES_H_
#define NET_INSTAWEB_HTTP_PUBLIC_DEVICE_PROPERTIES_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class UserAgentMatcher;

// This class keeps track of the device properties of the client, which are
// for the most part learned from the UserAgent string.
class DeviceProperties {
 public:
  explicit DeviceProperties(UserAgentMatcher* matcher);
  virtual ~DeviceProperties();

  void set_user_agent(const StringPiece& user_agent_string);
  bool SupportsImageInlining() const;
  bool SupportsLazyloadImages() const;
  bool SupportsCriticalImagesBeacon() const;
  bool SupportsJsDefer(bool enable_mobile) const;
  bool SupportsWebp() const;
  bool SupportsWebpLosslessAlpha() const;
  bool IsBot() const;
  bool IsMobileUserAgent() const;
  bool SupportsSplitHtml(bool enable_mobile) const;
  bool CanPreloadResources() const;
  bool GetScreenResolution(int* width, int* height) const;

 private:
  friend class ImageRewriteTest;
  FRIEND_TEST(ImageRewriteTest, SquashImagesForMobileScreen);
  void SetScreenResolution(int width, int height) const;

  GoogleString user_agent_;
  UserAgentMatcher* ua_matcher_;

  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool supports_webp_;
  mutable LazyBool supports_webp_lossless_alpha_;
  mutable LazyBool is_bot_;
  mutable LazyBool is_mobile_user_agent_;
  mutable LazyBool supports_split_html_;
  mutable LazyBool supports_flush_early_;
  mutable LazyBool screen_dimensions_set_;
  mutable int screen_width_;
  mutable int screen_height_;

  DISALLOW_COPY_AND_ASSIGN(DeviceProperties);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_DEVICE_PROPERTIES_H_
