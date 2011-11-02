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

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/gtest.h"

namespace {

// User Agent strings are from http://www.useragentstring.com/.
// IE: http://www.useragentstring.com/pages/Internet%20Explorer/
// FireFox: http://www.useragentstring.com/pages/Firefox/
// Chrome: http://www.useragentstring.com/pages/Chrome/
// And there are many more.

const char kAndroidHCUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 3.2; en-us; Sony Tablet S Build/THMAS11000) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Version/4.0 Safari/534.13";
const char kAndroidICSUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; Galaxy Nexus Build/ICL27) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30";
const char kChromeUserAgent[] =
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) "
    "AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C Safari/525.13";
const char kChrome9UserAgent[] =  // Not webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/9.0.597.19 Safari/534.13";
const char kChrome12UserAgent[] =  // webp capable
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_4) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Chrome/12.0.742.100 Safari/534.30";
const char kOpera1101UserAgent[] =  // Not webp capable
    "Opera/9.80 (Windows NT 5.2; U; ru) Presto/2.7.62 Version/11.01";
const char kOpera1110UserAgent[] =  // webp capable
    "Opera/9.80 (Windows NT 6.0; U; en) Presto/2.8.99 Version/11.10";
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
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/534.51.22 "
    "(KHTML, like Gecko) Version/5.1.1 Safari/534.51.22";
}  // namespace

namespace net_instaweb {

class UserAgentMatcherTest : public testing::Test {
 protected:
  UserAgentMatcher user_agent_matcher_;
};

TEST_F(UserAgentMatcherTest, IsIeTest) {
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

TEST_F(UserAgentMatcherTest, IsNotIeTest) {
  EXPECT_FALSE(user_agent_matcher_.IsIe(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe(kChromeUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsImageInlining) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kAndroidHCUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kOpera8UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kSafariUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(kIPhoneUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsImageInlining) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(kPSPUserAgent));
}


TEST_F(UserAgentMatcherTest, SupportsWebp) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(kChrome12UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(kOpera1110UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebp) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kIe9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(kSafariUserAgent));
}

}  // namespace net_instaweb
