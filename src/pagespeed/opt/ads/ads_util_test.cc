/*
 * Copyright 2014 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: chenyu@google.com (Yu Chen)

#include "pagespeed/opt/ads/ads_util.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
namespace ads_util {
namespace {

TEST(GetPublisherIdWithoutProductPrefixTest, Match) {
  EXPECT_EQ("1221",
            GetPublisherIdWithoutProductPrefix("ca-pub-1221").as_string());
}

TEST(GetPublisherIdWithoutProductPrefixTest, NoMatch) {
  EXPECT_EQ("capub1221",
            GetPublisherIdWithoutProductPrefix("capub1221").as_string());
}

TEST(IsAdsByGoogleJsSrcTest, Match) {
  EXPECT_TRUE(IsShowAdsApiCallJsSrc(
      "//pagead2.googlesyndication.com/pagead/show_ads.js"));
}

TEST(IsAdsByGoogleJsSrcTest, MatchWithParameter) {
  EXPECT_TRUE(IsShowAdsApiCallJsSrc(
      "//pagead2.googlesyndication.com/pagead/show_ads.js?v=1"));
}

TEST(IsShowAdsApiCallJsSrcTest, NoMatch) {
  EXPECT_FALSE(IsShowAdsApiCallJsSrc(
      "//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js"));
}

TEST(IsShowAdsApiCallJsSrcTest, Match) {
  EXPECT_TRUE(IsAdsByGoogleJsSrc(
      "//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js"));
}

TEST(IsShowAdsApiCallJsSrcTest, MatchWithParameter) {
  EXPECT_TRUE(IsAdsByGoogleJsSrc(
      "//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js?a=1"));
}

TEST(IsAdsByGoogleJsSrcTest, NoMatch) {
  EXPECT_FALSE(IsAdsByGoogleJsSrc(
      "//pagead2.googlesyndication.com/pagead/showads.js"));
}

}  // namespace
}  // namespace ads_util
}  // namespace net_instaweb
