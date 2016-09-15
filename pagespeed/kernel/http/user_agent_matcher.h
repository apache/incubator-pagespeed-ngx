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

#ifndef PAGESPEED_KERNEL_HTTP_USER_AGENT_MATCHER_H_
#define PAGESPEED_KERNEL_HTTP_USER_AGENT_MATCHER_H_

#include <map>
#include <utility>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/re2.h"

using std::pair;
using std::make_pair;
using std::map;

namespace net_instaweb {

class RequestHeaders;

// This class contains various user agent based checks.  Currently all of these
// are based on simple wildcard based white- and black-lists.
//
// TODO(sriharis):  Split the functionality here into two: a matcher that
// pulls out all relevant information from UA strings (browser-family, version,
// mobile/tablet/desktop, etc.), and a query interface that can be used by
// clients.
class UserAgentMatcher {
 public:
  static const char kTestUserAgentWebP[];  // webp user agent
  // Note that this must not contain the substring "webp".
  static const char kTestUserAgentNoWebP[];  // non-webp user agent

  enum DeviceType {
    kDesktop,
    kTablet,
    kMobile,
    // This should always be the last type. This is used to mark the size of an
    // array containing various DeviceTypes.
    kEndOfDeviceType
  };

  UserAgentMatcher();
  virtual ~UserAgentMatcher();

  // Before calling IsIe, ask if you're doing the right thing: are you doing
  // something that will mess up IE 11 in standards mode?  Are you in a position
  // where you can't tell what compatibility mode IE 11 is in?  Right now we use
  // this only to force edge compatibility mode and to work around a persistent
  // IE Vary: caching bug.
  bool IsIe(const StringPiece& user_agent) const;
  bool IsIe9(const StringPiece& user_agent) const;

  virtual bool SupportsImageInlining(const StringPiece& user_agent) const;
  bool SupportsLazyloadImages(StringPiece user_agent) const;

  // Returns the DeviceType for the given user agent string.
  virtual DeviceType GetDeviceTypeForUA(const StringPiece& user_agent) const;

  // Returns the DeviceType using the given user agent string and request
  // headers.
  virtual DeviceType GetDeviceTypeForUAAndHeaders(
      const StringPiece& user_agent,
      const RequestHeaders* request_headers) const;

  // Returns a string representing the device_type ("desktop", "tablet", or
  // "mobile").
  static StringPiece DeviceTypeString(DeviceType device_type);

  // Returns the suffix for the given device_type.
  static StringPiece DeviceTypeSuffix(DeviceType device_type);

  bool SupportsJsDefer(const StringPiece& user_agent, bool allow_mobile) const;

  // Returns true if the user agent includes a legacy browser that supports
  // webp, but does not issue Accept:image/webp.  At the moment, this means
  // only Android 4.0+ (excluding Firefox).
  bool LegacyWebp(const StringPiece& user_agent) const;

  // Returns true if the user agent includes a browser that is Pagespeed
  // Insights. We send webp to PSI, although it doesn't advertise it.
  bool InsightsWebp(const StringPiece& user_agent) const;

  // Returns true if the user agent includes a string indicating WebP lossy
  // or WebP alpha support. If the browser does indeed support WebP, it also
  // needs to send out an "accept: webp" header.
  bool SupportsWebpLosslessAlpha(const StringPiece& user_agent) const;

  // Returns true if the user agent includes an animated WebP capable
  // sub-string. If the browser does indeed support WebP, it also needs to send
  // out an "accept: webp" header.
  bool SupportsWebpAnimated(const StringPiece& user_agent) const;

  // IE9 does not implement <link rel=dns-prefetch ...>. Instead it does DNS
  // preresolution when it sees <link rel=prefetch ...>. This method returns
  // true if the browser support DNS prefetch using rel=prefetch.
  // Refer: http://blogs.msdn.com/b/ie/archive/2011/03/17/internet-explorer-9-network-performance-improvements.aspx NOLINT
  bool SupportsDnsPrefetchUsingRelPrefetch(const StringPiece& user_agent) const;
  bool SupportsDnsPrefetch(const StringPiece& user_agent) const;

  virtual bool IsAndroidUserAgent(const StringPiece& user_agent) const;
  virtual bool IsiOSUserAgent(const StringPiece& user_agent) const;

  // Returns false if this is not a Chrome user agent, or parsing the
  // string build number fails.
  virtual bool GetChromeBuildNumber(const StringPiece& user_agent, int* major,
                                    int* minor, int* build, int* patch) const;

  bool UserAgentExceedsChromeAndroidBuildAndPatch(
      const StringPiece& user_agent, int required_build,
      int required_patch) const;

  bool UserAgentExceedsChromeiOSBuildAndPatch(
      const StringPiece& user_agent, int required_build,
      int required_patch) const;

  bool UserAgentExceedsChromeBuildAndPatch(
      const StringPiece& user_agent, int required_build,
      int required_patch) const;

  bool SupportsMobilization(StringPiece user_agent) const;

 private:
  FastWildcardGroup supports_image_inlining_;
  FastWildcardGroup supports_lazyload_images_;
  FastWildcardGroup defer_js_whitelist_;
  FastWildcardGroup defer_js_mobile_whitelist_;
  FastWildcardGroup legacy_webp_;
  FastWildcardGroup pagespeed_insights_;
  FastWildcardGroup supports_webp_lossless_alpha_;
  FastWildcardGroup supports_webp_animated_;
  FastWildcardGroup supports_dns_prefetch_;
  FastWildcardGroup mobile_user_agents_;
  FastWildcardGroup tablet_user_agents_;
  FastWildcardGroup ie_user_agents_;
  FastWildcardGroup mobilization_user_agents_;

  const RE2 chrome_version_pattern_;
  scoped_ptr<RE2> known_devices_pattern_;
  mutable map <GoogleString, pair<int, int> > screen_dimensions_map_;

  DISALLOW_COPY_AND_ASSIGN(UserAgentMatcher);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_USER_AGENT_MATCHER_H_
