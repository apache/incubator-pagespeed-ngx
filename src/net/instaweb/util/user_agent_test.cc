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

const char kChromeUserAgent[] =
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) "
    "AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C Safari/525.13";
const char kFirefoxUserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
    "Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10";
const char kFirefox1UserAgent[] =
    "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.0.7) "
    "Gecko/20060909 Firefox/1.5.0.7 MG (Novarra-Vision/6.1)";
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
const char kIPhoneUserAgent[] =
    "Apple iPhone OS v2.1.1 CoreMedia v1.0.0.5F138";
const char kNokiaUserAgent[] =
    "Nokia2355/1.0 (JN100V0200.nep) UP.Browser/6.2.2.1.c.1.108 (GUI) MMP/2.0";
const char kOpera5UserAgent[] =
    "Opera/5.0 (SunOS 5.8 sun4u; U) [en]";
const char kOpera8UserAgent[] =
    "Opera/8.01 (J2ME/MIDP; Opera Mini/1.1.2666/1724; en; U; ssr)";
const char kPSPUserAgent[] =
    "Mozilla/4.0 (PSP (PlayStation Portable); 2.00)";
const char kSafariUserAgent[] =
    "Safari/525.20.1 Java/Jbed/7.0 Profile/MIDP-2.1 Configuration/"
    "CLDC-1.1 MMS/LG-Android-MMS-V1.0/1.2";
}  // namespace

namespace net_instaweb {

class UserAgentTest : public testing::Test {
 protected:
  UserAgent user_agent_matcher_;
};

TEST_F(UserAgentTest, IsIeTest) {
  EXPECT_TRUE(user_agent_matcher_.IsIe(kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6(kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(kIe6UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe7(kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(kIe7UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(kIe8UserAgent));
}

TEST_F(UserAgentTest, IsNotIeTest) {
  EXPECT_FALSE(user_agent_matcher_.IsIe(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(kFirefoxUserAgent));

  EXPECT_FALSE(user_agent_matcher_.IsIe(kChromeUserAgent));
}

TEST_F(UserAgentTest, NotSupportsImageInliningIe6) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kIe6UserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningIe9) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kIe9UserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningChrome) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kChromeUserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningFF3) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kFirefoxUserAgent));
}

TEST_F(UserAgentTest, NotSupportsImageInliningFF1) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kFirefox1UserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningOpera8) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kOpera8UserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningOpera5) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kOpera5UserAgent));
}

TEST_F(UserAgentTest, SupportsImageInliningSafari) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kSafariUserAgent));
}

TEST_F(UserAgentTest, NotSupportsImageInliningNokia) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kNokiaUserAgent));
}

TEST_F(UserAgentTest, SupportImageInliningiPhone) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kIPhoneUserAgent));
}

TEST_F(UserAgentTest, NotSupportImageInliningPSP) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kPSPUserAgent));
}
}  // namespace net_instaweb
