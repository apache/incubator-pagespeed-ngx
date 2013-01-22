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

#include <cstddef>

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace UserAgentStrings {
const char kTestingWebp[] = "webp";
const char kTestingWebpLosslessAlpha[] = "webp-la";
}

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
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kAndroidChrome21UserAgent));
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
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kAndroidChrome18UserAgent));
}

TEST_F(UserAgentMatcherTest, BlinkWhitelistForDesktop) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kFirefoxUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe9UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kChromeUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kSafariUserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, BlinkBlackListForDesktop) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe6UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe8UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kFirefox1UserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, DoesNotSupportBlink) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kNokiaUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kOpera5UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kPSPUserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, PrefetchMechanism) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_image_tag", &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkScriptTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIe9UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkRelSubresource,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_link_rel_subresource", &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kSafariUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkScriptTag,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_link_script_tag", &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                NULL, &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism("", &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kAndroidICSUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIPhoneUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIPadUserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, SupportsJsDefer) {
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe9UserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kChromeUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefoxUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kSafariUserAgent, false));
}

TEST_F(UserAgentMatcherTest, SupportsJsDeferAllowMobile) {
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kAndroidHCUserAgent, true));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIPhone4Safari, true));
  // Desktop is also supported.
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kChromeUserAgent, true));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDefer) {
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe6UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe8UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefox1UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kNokiaUserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kOpera5UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kPSPUserAgent, false));
  // Mobile is not supported too.
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIPhone4Safari, false));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDeferAllowMobile) {
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kOperaMobi9, true));
}

TEST_F(UserAgentMatcherTest, SupportsWebp) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kTestingWebp));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kTestingWebpLosslessAlpha));

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
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIPhoneChrome21UserAgent));
}

TEST_F(UserAgentMatcherTest, IsAndroidUserAgentTest) {
  EXPECT_TRUE(user_agent_matcher_.IsAndroidUserAgent(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsAndroidUserAgent(
      UserAgentStrings::kIe6UserAgent));
}

TEST_F(UserAgentMatcherTest, ChromeBuildNumberTest) {
  int major = -1;
  int minor = -1;
  int build = -1;
  int patch = -1;
  EXPECT_TRUE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kChrome9UserAgent, &major, &minor, &build, &patch));
  EXPECT_EQ(major, 9);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(build, 597);
  EXPECT_EQ(patch, 19);

  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kAndroidHCUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kChromeUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      "Chrome/10.0", &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      "Chrome/10.0.1.", &major, &minor, &build, &patch));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetch) {
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kFirefox5UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportDnsPrefetch) {
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsWebpLosslessAlpha) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kTestingWebpLosslessAlpha));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebpLosslessAlpha) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kTestingWebp));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome12UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera1110UserAgent));

  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetchUsingRelPrefetch) {
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe9UserAgent));
}

TEST_F(UserAgentMatcherTest, SplitHtmlRelated) {
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kIe9UserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kChromeUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kFirefoxUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kSafariUserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kAndroidChrome21UserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kAndroidChrome21UserAgent, true));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kIe6UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kIe8UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kFirefox1UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kNokiaUserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kOpera5UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kPSPUserAgent, false));
}

TEST_F(UserAgentMatcherTest, IsMobileUserAgent) {
  EXPECT_TRUE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kAndroidNexusSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kAndroidChrome21UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kIPhoneChrome21UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kIPhoneUserAgent));

  EXPECT_FALSE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kNexus7ChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kIPadUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsMobileUserAgent(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, GetDeviceTypeForUA) {
  EXPECT_EQ(UserAgentMatcher::kDesktop, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_EQ(UserAgentMatcher::kMobile, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kIPhone4Safari));
  EXPECT_EQ(UserAgentMatcher::kDesktop, user_agent_matcher_.GetDeviceTypeForUA(
      NULL));
}

TEST_F(UserAgentMatcherTest, GetScreenDimensionsFromLocalRegex) {
  int width, height;

  // Unknown user agent.
  EXPECT_FALSE(user_agent_matcher_.GetScreenDimensionsFromLocalRegex(
      UserAgentStrings::kIPhoneChrome21UserAgent, &width, &height));

  // Galaxy Nexus, first in list
  EXPECT_TRUE(user_agent_matcher_.GetScreenDimensionsFromLocalRegex(
      UserAgentStrings::kAndroidICSUserAgent, &width, &height));
  EXPECT_EQ(720, width);
  EXPECT_EQ(1280, height);

  // Nexus S, middle of list.
  EXPECT_TRUE(user_agent_matcher_.GetScreenDimensionsFromLocalRegex(
      UserAgentStrings::kAndroidNexusSUserAgent, &width, &height));
  EXPECT_EQ(480, width);
  EXPECT_EQ(800, height);

  // XT907, last in list.
  EXPECT_TRUE(user_agent_matcher_.GetScreenDimensionsFromLocalRegex(
      UserAgentStrings::XT907UserAgent, &width, &height));
  EXPECT_EQ(540, width);
  EXPECT_EQ(960, height);
}

}  // namespace net_instaweb
