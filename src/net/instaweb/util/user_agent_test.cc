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
#include "net/instaweb/util/public/gtest.h"

namespace {

// User Agent strings are from http://www.useragentstring.com/.
// IE: http://www.useragentstring.com/pages/Internet%20Explorer/
// FireFox: http://www.useragentstring.com/pages/Firefox/
// Chrome: http://www.useragentstring.com/pages/Chrome/
// And there are many more.

const char kIe6UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 6.0; Windows NT 5.1; SV1;"
    " .NET CLR 2.0.50727)";
const char kIe7UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.0; en-US)";
const char kIe8UserAgent[] =
    "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64;"
    " Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729;"
    " .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.2;"
    " .NET4.0C; .NET4.0E; FDM)";
const char kIe9UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 9.0; WIndows NT 9.0; en-US))";
const char kFirefoxUserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
    "Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10";
const char kChromeUserAgent[] =
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) "
    "AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C Safari/525.13";

}  // namespace

namespace net_instaweb {

class UserAgentTest : public testing::Test {
 protected:
  void SetUserAgent(const char* agent) {
    user_agent_.set_user_agent(agent);
  }
  UserAgent user_agent_;
};

TEST_F(UserAgentTest, IsIeTest) {
  SetUserAgent(kIe6UserAgent);
  EXPECT_TRUE(user_agent_.IsIe());
  EXPECT_TRUE(user_agent_.IsIe6());
  EXPECT_FALSE(user_agent_.IsIe7());
  EXPECT_TRUE(user_agent_.IsIe6or7());

  SetUserAgent(kIe7UserAgent);
  EXPECT_TRUE(user_agent_.IsIe());
  EXPECT_TRUE(user_agent_.IsIe7());
  EXPECT_FALSE(user_agent_.IsIe6());
  EXPECT_TRUE(user_agent_.IsIe6or7());

  SetUserAgent(kIe8UserAgent);
  EXPECT_TRUE(user_agent_.IsIe());
  EXPECT_FALSE(user_agent_.IsIe6());
  EXPECT_FALSE(user_agent_.IsIe7());
  EXPECT_FALSE(user_agent_.IsIe6or7());


}

TEST_F(UserAgentTest, IsNotIeTest) {
  SetUserAgent(kFirefoxUserAgent);
  EXPECT_FALSE(user_agent_.IsIe());
  EXPECT_FALSE(user_agent_.IsIe6());
  EXPECT_FALSE(user_agent_.IsIe6or7());
}

}  // namespace net_instaweb
