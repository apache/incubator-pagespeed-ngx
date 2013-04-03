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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_H_

#include <map>
#include <utility>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "third_party/instaweb/util/fast_wildcard_group.h"

using std::pair;
using std::make_pair;
using std::map;

namespace net_instaweb {

class RequestHeaders;

// This class contains various user agent based checks.  Currently all of these
// are based on simple wildcard based white- and black-lists.
//
// TODO(sriharis):  Split the functionality here into two: a matcher that
// pulls out all relevent information from UA strings (browser-family, version,
// mobile/tablet/desktop, etc.), and a query interface that can be used by
// clients.
class UserAgentMatcher {
 public:
  enum BlinkRequestType {
    kBlinkWhiteListForDesktop,
    kBlinkBlackListForDesktop,
    kBlinkWhiteListForMobile,
    kDoesNotSupportBlinkForMobile,
    kNullOrEmpty,
    kDoesNotSupportBlink,
  };

  enum DeviceType {
    kDesktop,
    kTablet,
    kMobile,
    // This should always be the last type. This is used to mark the size of an
    // array containing various DeviceTypes.
    kEndOfDeviceType
  };

  enum PrefetchMechanism {
    kPrefetchNotSupported,
    kPrefetchLinkRelSubresource,
    kPrefetchImageTag,
    kPrefetchObjectTag,
    kPrefetchLinkScriptTag,
  };

  // Cohort descriptors for PropertyCache lookups of device objects.
  static const char kDevicePropertiesCohort[];
  static const char kScreenWidth[];
  static const char kScreenHeight[];

  UserAgentMatcher();
  virtual ~UserAgentMatcher();

  bool IsIe(const StringPiece& user_agent) const;
  bool IsIe6(const StringPiece& user_agent) const;
  bool IsIe7(const StringPiece& user_agent) const;
  bool IsIe6or7(const StringPiece& user_agent) const {
    return IsIe6(user_agent) || IsIe7(user_agent);
  };
  bool IsIe9(const StringPiece& user_agent) const;

  virtual bool SupportsImageInlining(const StringPiece& user_agent) const;
  bool SupportsLazyloadImages(StringPiece user_agent) const;

  // Returns the request type for the given request. The return type currently
  // supports desktop, mobile and not supported.
  virtual BlinkRequestType GetBlinkRequestType(
      const char* user_agent, const RequestHeaders* request_headers) const;

  // Returns the supported prefetch mechanism depending upon the user agent.
  PrefetchMechanism GetPrefetchMechanism(const StringPiece& user_agent) const;

  // Returns the DeviceType for the given user agent string.
  virtual DeviceType GetDeviceTypeForUA(const StringPiece& user_agent) const;

  // Returns the DeviceType using the given user agent string and request
  // headers.
  virtual DeviceType GetDeviceTypeForUAAndHeaders(
      const StringPiece& user_agent,
      const RequestHeaders* request_headers) const;

  // Returns the suffix for the given device_type.
  static StringPiece DeviceTypeSuffix(DeviceType device_type);

  bool SupportsJsDefer(const StringPiece& user_agent, bool allow_mobile) const;
  bool SupportsWebp(const StringPiece& user_agent) const;
  bool SupportsWebpLosslessAlpha(const StringPiece& user_agent) const;

  // IE9 does not implement <link rel=dns-prefetch ...>. Instead it does DNS
  // preresolution when it sees <link rel=prefetch ...>. This method returns
  // true if the browser support DNS prefetch using rel=prefetch.
  // Refer: http://blogs.msdn.com/b/ie/archive/2011/03/17/internet-explorer-9-network-performance-improvements.aspx NOLINT
  bool SupportsDnsPrefetchUsingRelPrefetch(const StringPiece& user_agent) const;
  bool SupportsDnsPrefetch(const StringPiece& user_agent) const;

  virtual bool IsAndroidUserAgent(const StringPiece& user_agent) const;

  // Returns false if this is not a Chrome user agent, or parsing the
  // string build number fails.
  virtual bool GetChromeBuildNumber(const StringPiece& user_agent, int* major,
                                    int* minor, int* build, int* patch) const;

  virtual bool SupportsSplitHtml(const StringPiece& user_agent,
                                 bool allow_mobile) const;

  // Returns true and sets width and height if we know them for the UA.
  virtual bool GetScreenResolution(
        const StringPiece& user_agent, int* width, int* height);

  bool UserAgentExceedsChromeAndroidBuildAndPatch(
      const StringPiece& user_agent, int required_build,
      int required_patch) const;

 private:
  FastWildcardGroup supports_image_inlining_;
  FastWildcardGroup supports_lazyload_images_;
  FastWildcardGroup blink_desktop_whitelist_;
  FastWildcardGroup blink_desktop_blacklist_;
  FastWildcardGroup blink_mobile_whitelist_;
  FastWildcardGroup supports_webp_;
  FastWildcardGroup supports_webp_lossless_alpha_;
  FastWildcardGroup mobile_user_agents_;
  FastWildcardGroup supports_prefetch_link_rel_subresource_;
  FastWildcardGroup supports_prefetch_image_tag_;
  FastWildcardGroup supports_prefetch_link_script_tag_;
  FastWildcardGroup supports_dns_prefetch_;

  const RE2 chrome_version_pattern_;
  scoped_ptr<RE2> known_devices_pattern_;
  mutable map <GoogleString, pair<int, int> > screen_dimensions_map_;

  DISALLOW_COPY_AND_ASSIGN(UserAgentMatcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_H_
