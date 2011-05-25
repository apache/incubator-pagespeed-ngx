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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_USER_AGENT_H_
#define NET_INSTAWEB_UTIL_PUBLIC_USER_AGENT_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard_group.h"

namespace net_instaweb {

class UserAgent {
// TODO(fangfei): rename this class to UserAgentMatcher or something better.
 public:
  UserAgent();

  bool IsIe(const StringPiece& user_agent) const;
  bool IsIe6(const StringPiece& user_agent) const;
  bool IsIe7(const StringPiece& user_agent) const;
  bool IsIe6or7(const StringPiece& user_agent) const {
    return IsIe6(user_agent) || IsIe7(user_agent);
  };
  bool SupportsImageInlining(const StringPiece& user_agent) const;
 private:
  WildcardGroup supports_image_inlining_;

  DISALLOW_COPY_AND_ASSIGN(UserAgent);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_USER_AGENT_H_
