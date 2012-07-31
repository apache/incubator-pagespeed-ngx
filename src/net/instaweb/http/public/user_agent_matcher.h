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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/fast_wildcard_group.h"

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
    kSupportsBlinkDesktop,
    kSupportsBlinkMobile,
    kDoesNotSupportBlink,
  };

  enum PrefetchMechanism {
    kPrefetchNotSupported,
    kPrefetchLinkRelSubresource,
    kPrefetchImageTag,
    kPrefetchObjectTag,
  };

  UserAgentMatcher();
  virtual ~UserAgentMatcher();

  bool IsIe(const StringPiece& user_agent) const;
  bool IsIe6(const StringPiece& user_agent) const;
  bool IsIe7(const StringPiece& user_agent) const;
  bool IsIe6or7(const StringPiece& user_agent) const {
    return IsIe6(user_agent) || IsIe7(user_agent);
  };
  bool IsIe9(const StringPiece& user_agent) const;

  bool SupportsImageInlining(const StringPiece& user_agent) const;

  // Returns the request type for the given request. The return type currently
  // supports desktop, mobile and not supported.
  virtual BlinkRequestType GetBlinkRequestType(
      const char* user_agent, const RequestHeaders* request_headers,
      bool allow_mobile) const;

  // Returns the supported prefetch mechanism depending upon the user agent.
  PrefetchMechanism GetPrefetchMechanism(const StringPiece& user_agent) const;

  bool SupportsJsDefer(const StringPiece& user_agent) const;
  bool SupportsWebp(const StringPiece& user_agent) const;

  // IE9 does not implement <link rel=dns-prefetch ...>. Instead it does DNS
  // preresolution when it sees <link rel=prefetch ...>. This method returns
  // true if the browser support DNS prefetch using rel=prefetch.
  // Refer: http://blogs.msdn.com/b/ie/archive/2011/03/17/internet-explorer-9-network-performance-improvements.aspx NOLINT
  bool SupportsDnsPrefetchUsingRelPrefetch(const StringPiece& user_agent) const;

  virtual bool IsMobileUserAgent(const StringPiece& user_agent) const;
  virtual bool IsMobileRequest(
      const StringPiece& user_agent,
      const RequestHeaders* request_headers) const;

 private:
  FastWildcardGroup supports_image_inlining_;
  FastWildcardGroup supports_blink_desktop_;
  FastWildcardGroup supports_webp_;
  FastWildcardGroup mobile_user_agents_;
  FastWildcardGroup supports_prefetch_link_rel_subresource_;
  FastWildcardGroup supports_prefetch_image_tag_;

  DISALLOW_COPY_AND_ASSIGN(UserAgentMatcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_H_
