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
#include "net/instaweb/util/public/wildcard_group.h"

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

  UserAgentMatcher();
  virtual ~UserAgentMatcher();

  bool IsIe(const StringPiece& user_agent) const;
  bool IsIe6(const StringPiece& user_agent) const;
  bool IsIe7(const StringPiece& user_agent) const;
  bool IsIe6or7(const StringPiece& user_agent) const {
    return IsIe6(user_agent) || IsIe7(user_agent);
  };

  bool SupportsImageInlining(const StringPiece& user_agent) const;

  // Returns the request type for the given request. The return type currently
  // supports desktop, mobile and not supported.
  BlinkRequestType GetBlinkRequestType(
      const char* user_agent, const RequestHeaders* request_headers,
      bool allow_mobile) const;

  bool SupportsJsDefer(const StringPiece& user_agent) const;
  bool SupportsWebp(const StringPiece& user_agent) const;

  // The following two functions have similar names, but different
  // functionality. The first one implements a simple restricted wildcard based
  // check of whether user_agent corresponds to a mobile. It is not exhaustive.
  // The second one is meant to check if user_agent matches all currently known
  // mobile user agent pattern.
  // TODO(sriharis): Remove the need for these two separate functions, and
  // refactor the names.
  bool IsMobileUserAgent(const StringPiece& user_agent) const;
  virtual bool IsAnyMobileUserAgent(const char* user_agent) const;
  virtual bool IsMobileRequest(
      const StringPiece& user_agent,
      const RequestHeaders* request_headers) const;

 private:
  WildcardGroup supports_image_inlining_;
  WildcardGroup supports_blink_desktop_;
  WildcardGroup supports_webp_;
  WildcardGroup mobile_user_agents_;

  DISALLOW_COPY_AND_ASSIGN(UserAgentMatcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_H_
