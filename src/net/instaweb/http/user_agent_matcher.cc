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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/wildcard_group.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
// These are the user-agents of browsers/mobile devices which support
// image-inlining. The data is from "Latest WURFL Repository"(mobile devices)
// and "Web Patch"(browsers) on http://wurfl.sourceforge.net
// The user-agent string for Opera could be in the form of "Opera 7" or
// "Opera/7", we use regualr-expression "Opera?7" for this case.
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
};
// For webp rewriting, we whitelist Android, Chrome and Opera, but blacklist
// older versions of the browsers that are not webp capable.  As other browsers
// roll out webp support we will need to update this list to include them.
const char* kWebpWhitelist[] = {
  "*Android *",
  "*Chrome/*",
  "*Opera/9.80*Version/??.*",
  "*Opera???.*",
  // User agent used only for internal testing
  "webp",
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
  "*Opera/9.80*Version/10.*",
  "*Opera?10.*",
  "*Opera/9.80*Version/11.0*",
  "*Opera?11.0*",
};

// Only a few user agents are supported at this point.
// This is currently used only by kResizeMobileImages to deliver low resolution
// images to mobile devices. We treat ipads like desktops as they have big
// enough screen (relative to phones). But we treat android tablets like
// phones. If we could distinguish android tablets from phones easily using
// user agent string we would do so for the same reason we do so for ipads.
// TODO(bolian): Add more mobile user agents.
const char* kMobileUserAgentWhitelist[] = {
  "*Android*",
  "*iPhone OS*",
  "*BlackBerry88*",
};
}

UserAgentMatcher::UserAgentMatcher() {
  // Initialize WildcardGroup for image inlining whitelist & blacklist.
  for (int i = 0, n = arraysize(kImageInliningWhitelist); i < n; ++i) {
    supports_image_inlining_.Allow(kImageInliningWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kImageInliningBlacklist); i < n; ++i) {
    supports_image_inlining_.Disallow(kImageInliningBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopWhitelist); i < n; ++i) {
    supports_blink_desktop_.Allow(kPanelSupportDesktopWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kPanelSupportDesktopBlacklist); i < n; ++i) {
    supports_blink_desktop_.Disallow(kPanelSupportDesktopBlacklist[i]);
  }
  // Do the same for webp support.
  for (int i = 0, n = arraysize(kWebpWhitelist); i < n; ++i) {
    supports_webp_.Allow(kWebpWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kWebpBlacklist); i < n; ++i) {
    supports_webp_.Disallow(kWebpBlacklist[i]);
  }
  for (int i = 0, n = arraysize(kMobileUserAgentWhitelist); i < n; ++i) {
    mobile_user_agents_.Allow(kMobileUserAgentWhitelist[i]);
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

bool UserAgentMatcher::SupportsImageInlining(
    const StringPiece& user_agent) const {
  if (user_agent.empty()) {
    return true;
  }
  return supports_image_inlining_.Match(user_agent, false);
}

UserAgentMatcher::BlinkUserAgentType UserAgentMatcher::GetBlinkUserAgentType(
    const char* user_agent) const {
  if (user_agent == NULL) {
    // We were allowing empty user agents earlier in SupportsBlink.
    // TODO(sriharis):  But we should not do so now.
    return kSupportsBlinkDesktop;
  }
  if (IsAnyMobileUserAgent(user_agent)) {
    // TODO(srihari):  When blink supports mobile, we need to add a mobile
    // whitelist check here.
    return kDoesNotSupportBlink;
  }
  if (supports_blink_desktop_.Match(user_agent, false)) {
    return kSupportsBlinkDesktop;
  }
  return kDoesNotSupportBlink;
}

bool UserAgentMatcher::SupportsJsDefer(const StringPiece& user_agent) const {
  // TODO(ksimbili): Have js_defer it's own wildcard group.
  return user_agent.empty() || supports_blink_desktop_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsWebp(const StringPiece& user_agent) const {
  // TODO(jmaessen): this is a stub for regression testing purposes.
  // Put in real detection without treading on fengfei's toes.
  return supports_webp_.Match(user_agent, false);
}

bool UserAgentMatcher::IsMobileUserAgent(const StringPiece& user_agent) const {
  return mobile_user_agents_.Match(user_agent, false);
}

// The DEFAULT implementation is the same as IsMobileUserAgent but subclasses
// will override this IsAny method as required.
bool UserAgentMatcher::IsAnyMobileUserAgent(
    const char* user_agent) const {
  return IsMobileUserAgent(user_agent);
}

}  // namespace net_instaweb
