// Copyright 2010 Google Inc. All Rights Reserved.
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

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fast_wildcard_group.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
// These are the user-agents of browsers/mobile devices which support
// image-inlining. The data is from "Latest WURFL Repository"(mobile devices)
// and "Web Patch"(browsers) on http://wurfl.sourceforge.net
// The user-agent string for Opera could be in the form of "Opera 7" or
// "Opera/7", we use the wildcard pattern "Opera?7" for this case.
namespace {
const char* kImageInliningWhitelist[] = {
  "*Android*",
  "*Chrome/*",
  "*Firefox/*",
  "*iPad*",
  "*iPhone*",
  "*iPod*",
  "*itouch*",
  "*MSIE *",
  "*Opera*",
  "*Safari*",
  "*Wget*",
  // The following user agents are used only for internal testing
  "google command line rewriter",
  "webp",
  "webp-la",
  "prefetch_link_rel_subresource",
  "prefetch_image_tag",
  "prefetch_link_script_tag",
};
const char* kImageInliningBlacklist[] = {
  "*Firefox/1.*",
  "*Firefox/2.*",
  "*MSIE 5.*",
  "*MSIE 6.*",
  "*MSIE 7.*",
  "*Opera?5*",
  "*Opera?6*"
};
// For Panels and deferJs the list is same as of now.
// we only allow Firefox3+, IE8+, safari and Chrome
// We'll be updating this as and when required.
// The blacklist is checked first, then if not in there, the whitelist is
// checked.
// Note: None of the following should match a mobile UA.
const char* kPanelSupportDesktopWhitelist[] = {
  "*Chrome/*",
  "*Firefox/*",
  "*MSIE *",
  "*Safari*",
  "*Wget*",
};
const char* kPanelSupportDesktopBlacklist[] = {
  "*Firefox/1.*",
  "*Firefox/2.*",
  "*MSIE 5.*",
  "*MSIE 6.*",
  "*MSIE 7.*",
  "*MSIE 8.*",
};
const char* kPanelSupportMobileWhitelist[] = {
  "*AppleWebKit/*",
};
// For webp rewriting, we whitelist Android, Chrome and Opera, but blacklist
// older versions of the browsers that are not webp capable.  As other browsers
// roll out webp support we will need to update this list to include them.
const char* kWebpWhitelist[] = {
  "*Android *",
  "*Chrome/*",
  "*Opera/9.80*Version/??.*",
  "*Opera???.*",
  // User agents used only for internal testing.
  "webp",
  "webp-la",  // webp with lossless and alpha encoding.
};
const char* kWebpBlacklist[] = {
  "*Android 0.*",
  "*Android 1.*",
  "*Android 2.*",
  "*Android 3.*",
  "*Chrome/0.*",
  "*Chrome/1.*",
  "*Chrome/2.*",
  "*Chrome/3.*",
  "*Chrome/4.*",
  "*Chrome/5.*",
  "*Chrome/6.*",
  "*Chrome/7.*",
  "*Chrome/8.*",
  "*Chrome/9.0.*",
  "*Chrome/14.*",
  "*Chrome/15.*",
  "*Chrome/16.*",
  "*Android *Chrome/1?.*",
  "*Android *Chrome/20.*",
  "*Opera/9.80*Version/10.*",
  "*Opera?10.*",
  "*Opera/9.80*Version/11.0*",
  "*Opera?11.0*",
};

const char* kWebpLosslessAlphaWhitelist[] = {
  "*Chrome/??.*",
  "*Chrome/???.*",
  // User agent used only for internal testing.
  "webp-la",
};

const char* kWebpLosslessAlphaBlacklist[] = {
  "*Chrome/?.*",
  "*Chrome/1?.*",
  "*Chrome/20.*",
  "*Chrome/21.*",
  "*Chrome/22.*",
};

// TODO(rahulbansal): We haven't added Safari here since it supports dns
// prefetch only from 5.0.1 which causes the wildcard to be a bit messy.
const char* kInsertDnsPrefetchWhitelist[] = {
  "*Chrome/*",
  "*Firefox/*",
  "*MSIE *",
  "*Wget*",
  // The following user agents are used only for internal testing
  "prefetch_image_tag",
};

const char* kInsertDnsPrefetchBlacklist[] = {
  "*Firefox/1.*",
  "*Firefox/2.*",
  "*Firefox/3.*",
  "*MSIE 5.*",
  "*MSIE 6.*",
  "*MSIE 7.*",
  "*MSIE 8.*",
};

// Only a few user agents are supported at this point.
// This is currently used only by kResizeMobileImages and
// kSquashImagesForMobileScreento deliver smaller images to mobile devices.
// We treat tablets like desktops as they have big enough screen (relative
// to phones).
// TODO(bolian): Add more mobile user agents.
const char* kMobileUserAgentWhitelist[] = {
  "*Android*Mobile Safari*",
  "*iPhone OS*",
  "*BlackBerry88*",
};

const char* kSupportsPrefetchLinkRelSubresource[] = {
  // User agent used only for internal testing
  "prefetch_link_rel_subresource",
};

// TODO(mmohabey): Tune this to include more browsers.
const char* kSupportsPrefetchImageTag[] = {
  "*Chrome/*",
  // User agent used only for internal testing
  "prefetch_image_tag",
};

const char* kSupportsPrefetchLinkScriptTag[] = {
  "*Firefox/*",
  "*MSIE *",
  // User agent used only for internal testing
  "prefetch_link_script_tag",
};

const char* kChromeVersionPattern = "Chrome/(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)";
}  // namespace

const char UserAgentMatcher::kDevicePropertiesCohort[] = "deviceproperties";
const char UserAgentMatcher::kScreenWidth[] = "screen_width";
const char UserAgentMatcher::kScreenHeight[] = "screen_height";

UserAgentMatcher::UserAgentMatcher()
    : device_cache_(NULL), device_page_(NULL),
      chrome_version_pattern_(kChromeVersionPattern) {
  // Initialize FastWildcardGroup for image inlining whitelist & blacklist.
  for (int i = 0, n = arraysize(kImageInliningWhitelist); i < n; ++i) {
    supports_image_inlining_.Allow(kImageInliningWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kImageInliningBlacklist); i < n; ++i) {
    supports_image_inlining_.Disallow(kImageInliningBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopWhitelist); i < n; ++i) {
    blink_desktop_whitelist_.Allow(kPanelSupportDesktopWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopBlacklist); i < n; ++i) {
    blink_desktop_blacklist_.Allow(kPanelSupportDesktopBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportMobileWhitelist); i < n; ++i) {
    blink_mobile_whitelist_.Allow(kPanelSupportMobileWhitelist[i]);
  }

  // Do the same for webp support.
  for (int i = 0, n = arraysize(kWebpWhitelist); i < n; ++i) {
    supports_webp_.Allow(kWebpWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kWebpBlacklist); i < n; ++i) {
    supports_webp_.Disallow(kWebpBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kWebpLosslessAlphaWhitelist); i < n; ++i) {
    supports_webp_lossless_alpha_.Allow(kWebpLosslessAlphaWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kWebpLosslessAlphaBlacklist); i < n; ++i) {
    supports_webp_lossless_alpha_.Disallow(kWebpLosslessAlphaBlacklist[i]);
  }

  for (int i = 0, n = arraysize(kMobileUserAgentWhitelist); i < n; ++i) {
    mobile_user_agents_.Allow(kMobileUserAgentWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kSupportsPrefetchLinkRelSubresource); i < n;
       ++i) {
    supports_prefetch_link_rel_subresource_.Allow(
        kSupportsPrefetchLinkRelSubresource[i]);
  }
  for (int i = 0, n = arraysize(kSupportsPrefetchImageTag); i < n; ++i) {
    supports_prefetch_image_tag_.Allow(kSupportsPrefetchImageTag[i]);
  }
  for (int i = 0, n = arraysize(kSupportsPrefetchLinkScriptTag); i < n; ++i) {
    supports_prefetch_link_script_tag_.Allow(kSupportsPrefetchLinkScriptTag[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchWhitelist); i < n; ++i) {
    supports_dns_prefetch_.Allow(kInsertDnsPrefetchWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchBlacklist); i < n; ++i) {
    supports_dns_prefetch_.Disallow(kInsertDnsPrefetchBlacklist[i]);
  }
}

UserAgentMatcher::~UserAgentMatcher() {
}

bool UserAgentMatcher::IsIe(const StringPiece& user_agent) const {
  return user_agent.find(" MSIE ") != GoogleString::npos;
}

bool UserAgentMatcher::IsIe6(const StringPiece& user_agent) const {
  return user_agent.find(" MSIE 6.") != GoogleString::npos;
}

bool UserAgentMatcher::IsIe7(const StringPiece& user_agent) const {
  return user_agent.find(" MSIE 7.") != GoogleString::npos;
}

bool UserAgentMatcher::IsIe9(const StringPiece& user_agent) const {
  return user_agent.find(" MSIE 9.") != GoogleString::npos;
}

bool UserAgentMatcher::SupportsImageInlining(
    const StringPiece& user_agent) const {
  if (user_agent.empty()) {
    return true;
  }
  return supports_image_inlining_.Match(user_agent, false);
}

UserAgentMatcher::BlinkRequestType UserAgentMatcher::GetBlinkRequestType(
    const char* user_agent, const RequestHeaders* request_headers) const {
  if (user_agent == NULL || user_agent[0] == '\0') {
    return kNullOrEmpty;
  }
  if (IsMobileRequest(user_agent, request_headers)) {
    if (blink_mobile_whitelist_.Match(user_agent, false)) {
      return kBlinkWhiteListForMobile;
    }
    return kDoesNotSupportBlinkForMobile;
  }
  if (blink_desktop_blacklist_.Match(user_agent, false)) {
    return kBlinkBlackListForDesktop;
  }
  if (blink_desktop_whitelist_.Match(user_agent, false)) {
    return kBlinkWhiteListForDesktop;
  }
  return kDoesNotSupportBlink;
}

UserAgentMatcher::PrefetchMechanism UserAgentMatcher::GetPrefetchMechanism(
    const StringPiece& user_agent,
    const RequestHeaders* request_headers) const {
  if (IsMobileRequest(user_agent, request_headers)) {
    return kPrefetchNotSupported;
  }
  if (supports_prefetch_link_rel_subresource_.Match(user_agent, false)) {
    return kPrefetchLinkRelSubresource;
  } else if (supports_prefetch_image_tag_.Match(user_agent, false)) {
    return kPrefetchImageTag;
  } else if (supports_prefetch_link_script_tag_.Match(user_agent, false)) {
    return kPrefetchLinkScriptTag;
  }
  return kPrefetchNotSupported;
}

bool UserAgentMatcher::SupportsDnsPrefetch(
    const StringPiece& user_agent) const {
  return supports_dns_prefetch_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsJsDefer(const StringPiece& user_agent,
                                       bool allow_mobile) const {
  // TODO(ksimbili): Use IsMobileRequest?
  if (IsMobileUserAgent(user_agent)) {
    return allow_mobile && blink_mobile_whitelist_.Match(user_agent, false);
  }
  return user_agent.empty() ||
      (blink_desktop_whitelist_.Match(user_agent, false) &&
       !blink_desktop_blacklist_.Match(user_agent, false));
}

bool UserAgentMatcher::SupportsWebp(const StringPiece& user_agent) const {
  // TODO(jmaessen): this is a stub for regression testing purposes.
  // Put in real detection without treading on fengfei's toes.
  return supports_webp_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsWebpLosslessAlpha(
    const StringPiece& user_agent) const {
  return supports_webp_lossless_alpha_.Match(user_agent, false);
}

bool UserAgentMatcher::IsMobileUserAgent(const StringPiece& user_agent) const {
  return mobile_user_agents_.Match(user_agent, false);
}

bool UserAgentMatcher::IsMobileRequest(
    const StringPiece& user_agent,
    const RequestHeaders* request_headers) const {
  return IsMobileUserAgent(user_agent);
}

bool UserAgentMatcher::IsAndroidUserAgent(const StringPiece& user_agent) const {
  return user_agent.find("Android") != GoogleString::npos;
}

bool UserAgentMatcher::GetChromeBuildNumber(const StringPiece& user_agent,
                                            int* major, int* minor, int* build,
                                            int* patch) const {
  return RE2::PartialMatch(StringPieceToRe2(user_agent),
                           chrome_version_pattern_, major, minor, build, patch);
}

bool UserAgentMatcher::SupportsDnsPrefetchUsingRelPrefetch(
    const StringPiece& user_agent) const {
  return IsIe9(user_agent);
}

bool UserAgentMatcher::SupportsSplitHtml(const StringPiece& user_agent,
                                         bool allow_mobile) const {
  return SupportsJsDefer(user_agent, allow_mobile);
}

void UserAgentMatcher::LookupDeviceProperties(
    const StringPiece& user_agent, PropertyPage* page) {
}

UserAgentMatcher::DeviceType UserAgentMatcher::GetDeviceTypeForUA(
    const StringPiece& user_agent) const {
  // TODO(ksimbili): Pass in device property page once changes related to device
  // property cache are submitted.
  if (IsMobileUserAgent(user_agent)) {
    return kMobile;
  }
  return kDesktop;
}

StringPiece UserAgentMatcher::DeviceTypeSuffix(DeviceType device_type) {
  StringPiece device_type_suffix = "";
  switch (device_type) {
    case kMobile:
      device_type_suffix = "@Mobile";
      break;
    case kTablet:
      device_type_suffix = "@Tablet";
      break;
    case kDesktop:
    case kEndOfDeviceType:
    default:
      device_type_suffix = "@Desktop";
      break;
  }
  return device_type_suffix;
}

}  // namespace net_instaweb
