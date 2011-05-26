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
    "*Wget*"
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
}

UserAgentMatcher::UserAgentMatcher() {
  // Initialize WildcardGroup for image inlining whitelist & blacklist.
  for (int i = 0, n = arraysize(kImageInliningWhitelist); i < n; ++i) {
    supports_image_inlining_.Allow(kImageInliningWhitelist[i]);
  }
  for (int i = 0, n = arraysize(kImageInliningBlacklist); i < n; ++i) {
    supports_image_inlining_.Disallow(kImageInliningBlacklist[i]);
  }
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

}  // namespace net_instaweb
