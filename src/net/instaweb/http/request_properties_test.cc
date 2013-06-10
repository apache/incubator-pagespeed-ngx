// Copyright 2013 Google Inc. All Rights Reserved.
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

#include "net/instaweb/http/public/request_properties.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class RequestPropertiesTest: public testing::Test {
 protected:
  UserAgentMatcher user_agent_matcher_;
};

TEST_F(RequestPropertiesTest, SupportsWebp) {
  RequestProperties request_properties(&user_agent_matcher_);
  request_properties.set_user_agent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  EXPECT_TRUE(request_properties.SupportsWebp());
}

}  // namespace net_instaweb
