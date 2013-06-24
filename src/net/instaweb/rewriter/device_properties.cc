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

#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/bot_checker.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {
// Decides image quality for a given user preference and a device screen size.
// Image quality is determined by a table with the preference as row,
// the screen group as column, and the image quality as value of each cell.
// We want to restrict the number of distinct image qualities as it affects
// cache fragmentation (it changes meta data cache key and rewritten content).
// At the same time, we want to have distinct values for each row and each.
// column. Imagine a table having each row (or column) shifted one
// position from its adjacent rows (or columns), like below
//
// ScreenGroupIndex-->   | 0  1  2
//     Preferences     ------------    __
//          |          L | A  B  C       |
//          |          M | B  C  D        > Image qualities
//          V          H | C  D  E     __|
//
// Therefore, we will use
//   (kImageQualityPreferenceCount + kScreenGroupCount - 1)
// distinct values.

// Number of preference client options can set for image qualities.
const int kImageQualityPreferenceCount = 3;
// Number of screen groups supported.
const int kScreenGroupCount = 3;
// Thresholds of device screen width. This is used by GetScreenGroupIndex
// to divide screens into kScreenGroupCount groups so that different
// image qualities could be applied for each group.
const int kScreenWidthThresholds[kScreenGroupCount] = {
    0,
    DeviceProperties::kMediumScreenWidthThreshold,
    DeviceProperties::kLargeScreenWidthThreshold
};
// Number of image qualities used for client options.
const int kPreferredImageQualityCount =
    kImageQualityPreferenceCount + kScreenGroupCount - 1;

#ifndef NDEBUG
void CheckPreferredImageQualities() {
  DCHECK_EQ(0, static_cast<int>(DeviceProperties::kImageQualityDefault));
  DCHECK_EQ(1, static_cast<int>(DeviceProperties::kImageQualityLow));
  DCHECK_EQ(2, static_cast<int>(DeviceProperties::kImageQualityMedium));
  DCHECK_EQ(kImageQualityPreferenceCount,
         static_cast<int>(DeviceProperties::kImageQualityHigh));
  DCHECK_EQ(
      kScreenGroupCount, static_cast<int>(arraysize(kScreenWidthThresholds)));

  for (int i = 1, n = kScreenGroupCount; i < n; ++i) {
    DCHECK_LE(kScreenWidthThresholds[i - 1], kScreenWidthThresholds[i]);
  }
}
#endif

}  // namespace

DeviceProperties::DeviceProperties(UserAgentMatcher* matcher)
    : ua_matcher_(matcher), supports_image_inlining_(kNotSet),
      supports_js_defer_(kNotSet),
      supports_lazyload_images_(kNotSet),
      supports_webp_(kNotSet),
      supports_webp_lossless_alpha_(kNotSet),
      is_bot_(kNotSet),
      is_mobile_user_agent_(kNotSet),
      supports_split_html_(kNotSet), supports_flush_early_(kNotSet),
      screen_dimensions_set_(kNotSet), screen_width_(0), screen_height_(0),
      preferred_webp_qualities_(NULL), preferred_jpeg_qualities_(NULL),
      device_type_set_(kNotSet), device_type_(UserAgentMatcher::kDesktop) {
#ifndef NDEBUG
  CheckPreferredImageQualities();
#endif
}

DeviceProperties::~DeviceProperties() {
}

void DeviceProperties::set_user_agent(const StringPiece& user_agent_string) {
  user_agent_string.CopyToString(&user_agent_);

  // Reset everything determined by user agent.
  supports_image_inlining_ = kNotSet;
  supports_js_defer_ = kNotSet;
  supports_lazyload_images_ = kNotSet;
  supports_webp_ = kNotSet;
  supports_webp_lossless_alpha_ = kNotSet;
  is_bot_ = kNotSet;
  is_mobile_user_agent_ = kNotSet;
  supports_split_html_ = kNotSet;
  supports_flush_early_ = kNotSet;
  screen_dimensions_set_ = kNotSet;
  screen_width_ = 0;
  screen_height_ = 0;
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

bool DeviceProperties::GetScreenResolution(int* width, int* height) const {
  if (screen_dimensions_set_ == kNotSet) {
    if (ua_matcher_->GetScreenResolution(user_agent_, width, height)) {
      SetScreenResolution(*width, *height);
    }
  }
  if (screen_dimensions_set_ == kTrue) {
    *width = screen_width_;
    *height = screen_height_;
  }
  return (screen_dimensions_set_ == kTrue);
}

void DeviceProperties::SetScreenResolution(int width, int height) const {
  screen_dimensions_set_ = kTrue;
  screen_width_ = width;
  screen_height_ = height;
}

UserAgentMatcher::DeviceType DeviceProperties::GetDeviceType() const {
  if (device_type_set_ == kNotSet) {
    device_type_ = ua_matcher_->GetDeviceTypeForUA(user_agent_);
    device_type_set_ = kTrue;
  }
  return device_type_;
}

void DeviceProperties::SetPreferredImageQualities(
    const std::vector<int>* webp, const std::vector<int>* jpeg) {
  preferred_webp_qualities_ = webp;
  preferred_jpeg_qualities_ = jpeg;
}

bool DeviceProperties::HasPreferredImageQualities() const {
  return  preferred_webp_qualities_ != NULL &&
      (static_cast<int>(preferred_webp_qualities_->size()) ==
          kPreferredImageQualityCount) &&
      preferred_jpeg_qualities_ != NULL &&
      (static_cast<int>(preferred_jpeg_qualities_->size()) ==
          kPreferredImageQualityCount);
}

bool DeviceProperties::GetPreferredImageQualities(
    ImageQualityPreference preference, int* webp, int* jpeg) const {
  int width = 0, height = 0;
  int screen_index = 0;
  if (preference != kImageQualityDefault &&
      HasPreferredImageQualities() &&
      GetScreenResolution(&width, &height) &&
      GetScreenGroupIndex(width, &screen_index)) {
    // See block comment for kPreferredImageQualityCount about the index.
    int quality_index = screen_index + static_cast<int>(preference) - 1;
    *webp = (*preferred_webp_qualities_)[quality_index];
    *jpeg = (*preferred_jpeg_qualities_)[quality_index];
    return true;
  }
  return false;
}

// Returns true if a valid screen_index is returned for the screen_width.
bool DeviceProperties::GetScreenGroupIndex(
    int screen_width, int* screen_index) {
  for (int i = kScreenGroupCount - 1; i >= 0; --i) {
    if (screen_width >= kScreenWidthThresholds[i]) {
      *screen_index = i;
      return true;
    }
  }
  return false;
}

int DeviceProperties::GetPreferredImageQualityCount() {
  return kPreferredImageQualityCount;
}

}  // namespace net_instaweb
