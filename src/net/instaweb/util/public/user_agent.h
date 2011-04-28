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
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class UserAgent {
 public:
  UserAgent();
  void set_user_agent(const char* user_agent);

  bool IsIe() const;
  bool IsIe6() const;
  bool IsIe7() const;
  bool IsIe6or7() const {
    return IsIe6() || IsIe7();
  };

 private:
  GoogleString user_agent_;

  DISALLOW_COPY_AND_ASSIGN(UserAgent);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_USER_AGENT_H_
