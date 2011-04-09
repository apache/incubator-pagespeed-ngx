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

#include "net/instaweb/util/public/user_agent.h"

namespace net_instaweb {

UserAgent::UserAgent() {
}

void UserAgent::set_user_agent(const char* user_agent) {
  if (user_agent) {
    user_agent_.assign(user_agent);
  } else {
    user_agent_.clear();
  }
}

bool UserAgent::IsIe() const {
  return user_agent_.find(" MSIE ") != GoogleString::npos;
}

bool UserAgent::IsIe6() const {
  return user_agent_.find(" MSIE 6.") != GoogleString::npos;
}

bool UserAgent::IsIe7() const {
  return user_agent_.find(" MSIE 7.") != GoogleString::npos;
}

}  // namespace net_instaweb
