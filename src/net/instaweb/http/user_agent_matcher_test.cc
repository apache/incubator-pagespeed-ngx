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
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class UserAgentMatcherTest : public testing::Test {
 protected:
  UserAgentMatcher user_agent_matcher_;
};

TEST_F(UserAgentMatcherTest, IsIeTest) {
  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe6UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe7UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe8UserAgent));
}

TEST_F(UserAgentMatcherTest, IsNotIeTest) {
  EXPECT_FALSE(user_agent_matcher_.IsIe(UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe(UserAgentStrings::kChromeUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsImageInlining) {
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kOpera8UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kSafariUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kIPhoneUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsImageInlining) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kPSPUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsBlinkDesktop) {
  EXPECT_EQ(UserAgentMatcher::kSupportsBlinkDesktop,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kFirefoxUserAgent));
  EXPECT_EQ(UserAgentMatcher::kSupportsBlinkDesktop,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kIe9UserAgent));
  EXPECT_EQ(UserAgentMatcher::kSupportsBlinkDesktop,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kChromeUserAgent));
  EXPECT_EQ(UserAgentMatcher::kSupportsBlinkDesktop,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsBlink) {
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kIe6UserAgent));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kIe8UserAgent));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kFirefox1UserAgent));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kNokiaUserAgent));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kOpera5UserAgent));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkUserAgentType(
                UserAgentStrings::kPSPUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsJsDefer) {
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDefer) {
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kPSPUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsWebp) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome12UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome18UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera1110UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebp) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kSafariUserAgent));
}

}  // namespace net_instaweb
