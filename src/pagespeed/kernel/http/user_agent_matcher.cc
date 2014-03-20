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

#include <map>
#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

const char UserAgentMatcher::kTestUserAgentWebP[] = "test-user-agent-webp";
// Note that this must not contain the substring "webp".
const char UserAgentMatcher::kTestUserAgentNoWebP[] = "test-user-agent-no";

class RequestHeaders;

// These are the user-agents of browsers/mobile devices which support
// image-inlining. The data is from "Latest WURFL Repository"(mobile devices)
// and "Web Patch"(browsers) on http://wurfl.sourceforge.net
// The user-agent string for Opera could be in the form of "Opera 7" or
// "Opera/7", we use the wildcard pattern "Opera?7" for this case.
namespace {

const char kGooglePlusUserAgent[] =
    "*Google (+https://developers.google.com/+/web/snippet/)*";

const char* kImageInliningWhitelist[] = {
  "*Android*",
  "*Chrome/*",
  "*Firefox/*",
  "*iPad*",
  "*iPhone*",
  "*iPod*",
  "*itouch*",
  "*Opera*",
  "*Safari*",
  "*Wget*",
  // Plus IE, see use in the code.
  // The following user agents are used only for internal testing
  "google command line rewriter",
  "webp",
  "webp-la",
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
  "*Opera?6*",
  kGooglePlusUserAgent
};

// Exclude BlackBerry OS 5.0 and older. See
// http://supportforums.blackberry.com/t5/Web-and-WebWorks-Development/How-to-detect-the-BlackBerry-Browser/ta-p/559862
// for details on BlackBerry UAs.
const char* kLazyloadImagesBlacklist[] = {
  "BlackBerry*CLDC*",
  kGooglePlusUserAgent
};

// For Panels and deferJs the list is same as of now.
// we only allow Firefox4+, IE8+, safari and Chrome
// We'll be updating this as and when required.
// The blacklist is checked first, then if not in there, the whitelist is
// checked.
// Note: None of the following should match a mobile UA.
const char* kPanelSupportDesktopWhitelist[] = {
  "*Chrome/*",
  "*Firefox/*",
  "*Safari*",
  // Plus IE, see code below.
  "*Wget*",
  // The following user agents are used only for internal testing
  "prefetch_link_script_tag",
};
// Note that these are combined with kPanelSupportDesktopWhitelist, which
// imply defer_javascript support.
const char* kDeferJSWhitelist[] = {
  "*Googlebot*",
  "*Mediapartners-Google*"
};
const char* kPanelSupportDesktopBlacklist[] = {
  "*Firefox/1.*",
  "*Firefox/2.*",
  "*Firefox/3.*",
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
  // Plus IE, see code below.
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

// Whitelist used for doing the tablet-user-agent check, which also feeds
// into the device type used for storing properties in the property cache.
const char* kTabletUserAgentWhitelist[] = {
  "*Android*",  // Android tablet has "Android" but not "Mobile". Regexp
                // checks for UserAgents should first check the mobile
                // whitelists and blacklists and only then check the tablet
                // whitelist for correct results.
  "*iPad*",
  "*TouchPad*",
  "*Silk-Accelerated*",
  "*Kindle Fire*"
};

// Whitelist used for doing the mobile-user-agent check, which also feeds
// into the device type used for storing properties in the property cache.
const char* kMobileUserAgentWhitelist[] = {
  "*Mozilla*Android*Mobile*",
  "*iPhone*",
  "*BlackBerry*",
  "*Opera Mobi*",
  "*Opera Mini*",
  "*SymbianOS*",
  "*UP.Browser*",
  "*J-PHONE*",
  "*Profile/MIDP*",
  "*profile/MIDP*",
  "*portalmmm*",
  "*DoCoMo*",
  "*Obigo*"
};

// Blacklist used for doing the mobile-user-agent check.
const char* kMobileUserAgentBlacklist[] = {
  "*Mozilla*Android*Silk*Mobile*",
  "*Mozilla*Android*Kindle Fire*Mobile*"
};

// TODO(mmohabey): Tune this to include more browsers.
const char* kSupportsPrefetchImageTag[] = {
  "*Chrome/*",
  "*Safari/*",
  // User agent used only for internal testing
  "prefetch_image_tag",
};

const char* kSupportsPrefetchLinkScriptTag[] = {
  "*Firefox/*",
  // Plus IE, see code below
  // User agent used only for internal testing
  "prefetch_link_script_tag",
};

// IE 11 and later user agent strings are deliberately difficult.  That would be
// great if random pages never put the browser into backward compatibility mode,
// and all the outstanding caching bugs were fixed, but neither is true and so
// we need to be able to spot IE 11 and treat it as IE even though we're not
// supposed to need to do so ever again.  See
// http://blogs.msdn.com/b/ieinternals/archive/2013/09/21/internet-explorer-11-user-agent-string-ua-string-sniffing-compatibility-with-gecko-webkit.aspx
const char* kIeUserAgents[] = {
  "*MSIE *",                // Should match any IE before 11.
  "*rv:11.?) like Gecko*",  // Other revisions (eg 12.0) are FireFox
  "*IE 1*",                 // Initial numeral avoids Samsung UA
  "*Trident/7*",            // Opera sometimes pretends to be earlier Trident
};

// Match either 'CriOS' (iOS Chrome) or 'Chrome'. ':?' marks a non-capturing
// group.
const char* kChromeVersionPattern =
    "(?:Chrome|CriOS)/(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)";

// Device strings must not include wildcards.
struct Dimension {
  const char* device_name;
  int width;
  int height;
};

const Dimension kKnownScreenDimensions[] = {
  {"Galaxy Nexus", 720, 1280},
  {"GT-I9300", 720, 1280},
  {"GT-N7100", 720, 1280},
  {"HTC One", 720, 1280},
  {"Nexus 4", 768, 1280},
  {"Nexus 7", 800, 1280},
  {"Nexus 10", 1600, 2560},
  {"Nexus S", 480, 800},
  {"Xoom", 800, 1280},
  {"XT907", 540, 960},
};

}  // namespace

// Note that "blink" here does not mean the new Chrome rendering
// engine.  It refers to a pre-existing internal name for the
// technology behind partial HTML caching:
// https://developers.google.com/speed/pagespeed/service/CacheHtml

UserAgentMatcher::UserAgentMatcher()
    : chrome_version_pattern_(kChromeVersionPattern) {
  // Initialize FastWildcardGroup for image inlining whitelist & blacklist.
  for (int i = 0, n = arraysize(kImageInliningWhitelist); i < n; ++i) {
    supports_image_inlining_.Allow(kImageInliningWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    supports_image_inlining_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kImageInliningBlacklist); i < n; ++i) {
    supports_image_inlining_.Disallow(kImageInliningBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kLazyloadImagesBlacklist); i < n; ++i) {
    supports_lazyload_images_.Disallow(kLazyloadImagesBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopWhitelist); i < n; ++i) {
    blink_desktop_whitelist_.Allow(kPanelSupportDesktopWhitelist[i]);

    // Explicitly allowed blink UAs should also allow defer_javascript.
    defer_js_whitelist_.Allow(kPanelSupportDesktopWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    blink_desktop_whitelist_.Allow(kIeUserAgents[i]);
    defer_js_whitelist_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kDeferJSWhitelist); i < n; ++i) {
    defer_js_whitelist_.Allow(kDeferJSWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopBlacklist); i < n; ++i) {
    blink_desktop_blacklist_.Allow(kPanelSupportDesktopBlacklist[i]);

    // Explicitly disallowed blink UAs should also disable defer_javascript.
    defer_js_whitelist_.Disallow(kPanelSupportDesktopBlacklist[i]);
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
  for (int i = 0, n = arraysize(kSupportsPrefetchImageTag); i < n; ++i) {
    supports_prefetch_image_tag_.Allow(kSupportsPrefetchImageTag[i]);
  }
  for (int i = 0, n = arraysize(kSupportsPrefetchLinkScriptTag); i < n; ++i) {
    supports_prefetch_link_script_tag_.Allow(kSupportsPrefetchLinkScriptTag[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    supports_prefetch_link_script_tag_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchWhitelist); i < n; ++i) {
    supports_dns_prefetch_.Allow(kInsertDnsPrefetchWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    supports_dns_prefetch_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchBlacklist); i < n; ++i) {
    supports_dns_prefetch_.Disallow(kInsertDnsPrefetchBlacklist[i]);
  }

  for (int i = 0, n = arraysize(kMobileUserAgentWhitelist); i < n; ++i) {
    mobile_user_agents_.Allow(kMobileUserAgentWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kMobileUserAgentBlacklist); i < n; ++i) {
    mobile_user_agents_.Disallow(kMobileUserAgentBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kTabletUserAgentWhitelist); i < n; ++i) {
    tablet_user_agents_.Allow(kTabletUserAgentWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    ie_user_agents_.Allow(kIeUserAgents[i]);
  }
  GoogleString known_devices_pattern_string = "(";
  for (int i = 0, n = arraysize(kKnownScreenDimensions); i < n; ++i) {
    const Dimension& dim = kKnownScreenDimensions[i];
    screen_dimensions_map_[dim.device_name] = make_pair(dim.width, dim.height);
    if (i != 0) {
      StrAppend(&known_devices_pattern_string, "|");
    }
    StrAppend(&known_devices_pattern_string, dim.device_name);
  }
  StrAppend(&known_devices_pattern_string, ")");
  known_devices_pattern_.reset(new RE2(known_devices_pattern_string));
}

UserAgentMatcher::~UserAgentMatcher() {
}

bool UserAgentMatcher::IsIe(const StringPiece& user_agent) const {
  return ie_user_agents_.Match(user_agent, false);
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

bool UserAgentMatcher::SupportsLazyloadImages(StringPiece user_agent) const {
  return supports_lazyload_images_.Match(user_agent, true);
}

UserAgentMatcher::BlinkRequestType UserAgentMatcher::GetBlinkRequestType(
    const char* user_agent, const RequestHeaders* request_headers) const {
  if (user_agent == NULL || user_agent[0] == '\0') {
    return kNullOrEmpty;
  }
  if (GetDeviceTypeForUAAndHeaders(user_agent, request_headers) != kDesktop) {
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
    const StringPiece& user_agent) const {
  if (supports_prefetch_image_tag_.Match(user_agent, false)) {
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
  if (GetDeviceTypeForUA(user_agent) != kDesktop) {
    return allow_mobile && blink_mobile_whitelist_.Match(user_agent, false);
  }
  return user_agent.empty() || defer_js_whitelist_.Match(user_agent, false);
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

UserAgentMatcher::DeviceType UserAgentMatcher::GetDeviceTypeForUAAndHeaders(
    const StringPiece& user_agent,
    const RequestHeaders* request_headers) const {
  return GetDeviceTypeForUA(user_agent);
}

bool UserAgentMatcher::IsAndroidUserAgent(const StringPiece& user_agent) const {
  return user_agent.find("Android") != GoogleString::npos;
}

bool UserAgentMatcher::IsiOSUserAgent(const StringPiece& user_agent) const {
  return user_agent.find("iPhone") != GoogleString::npos ||
      user_agent.find("iPad") != GoogleString::npos;
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

// TODO(bharathbhushan): Make sure GetDeviceTypeForUA is called only once per
// http request.
UserAgentMatcher::DeviceType UserAgentMatcher::GetDeviceTypeForUA(
    const StringPiece& user_agent) const {
  if (mobile_user_agents_.Match(user_agent, false)) {
    return kMobile;
  }
  if (tablet_user_agents_.Match(user_agent, false)) {
    return kTablet;
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

bool UserAgentMatcher::GetScreenResolution(
    const StringPiece& user_agent, int* width, int* height) {
  DCHECK(width != NULL);
  DCHECK(height != NULL);
  GoogleString match;
  if (RE2::PartialMatch(
      StringPieceToRe2(user_agent), *known_devices_pattern_.get(), &match)) {
    pair<int, int> dims = screen_dimensions_map_[match];
    *width = dims.first;
    *height = dims.second;
    return true;
  }
  return false;
}

bool UserAgentMatcher::UserAgentExceedsChromeiOSBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // Verify if this is an iOS user agent.
  if (!IsiOSUserAgent(user_agent)) {
    return false;
  }
  return UserAgentExceedsChromeBuildAndPatch(
      user_agent, required_build, required_patch);
}

bool UserAgentMatcher::UserAgentExceedsChromeAndroidBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // Verify if this is an Android user agent.
  if (!IsAndroidUserAgent(user_agent)) {
    return false;
  }
  return UserAgentExceedsChromeBuildAndPatch(
      user_agent, required_build, required_patch);
}

bool UserAgentMatcher::UserAgentExceedsChromeBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // By default user agent sniffing is disabled.
  if (required_build == -1 && required_patch == -1) {
    return false;
  }
  int major = -1;
  int minor = -1;
  int parsed_build = -1;
  int parsed_patch = -1;
  if (!GetChromeBuildNumber(user_agent, &major, &minor,
                            &parsed_build, &parsed_patch)) {
    return false;
  }

  if (parsed_build < required_build) {
    return false;
  } else if (parsed_build == required_build && parsed_patch < required_patch) {
    return false;
  }

  return true;
}
}  // namespace net_instaweb
