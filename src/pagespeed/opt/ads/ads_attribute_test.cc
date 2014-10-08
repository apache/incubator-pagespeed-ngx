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
// Author: morlovich@google.com (Maks Orlovich)

#include "pagespeed/opt/ads/ads_attribute.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
namespace ads_attribute {
namespace {

TEST(LookupAdsByGoogleAttributeNameTest, Basic) {
  EXPECT_EQ("data-ad-client",
            LookupAdsByGoogleAttributeName("google_ad_client"));

  EXPECT_EQ("data-ad-slot",
            LookupAdsByGoogleAttributeName("google_ad_slot"));

  EXPECT_EQ("data-ad-channel",
            LookupAdsByGoogleAttributeName("google_ad_channel"));

  EXPECT_EQ("data-language",
            LookupAdsByGoogleAttributeName("google_language"));

  EXPECT_EQ("data-color-border",
            LookupAdsByGoogleAttributeName("google_color_border"));
}

TEST(LookupAdsByGoogleAttributeNameTest, NotMatched) {
  // Things that we shouldn't map.
  EXPECT_EQ("", LookupAdsByGoogleAttributeName("elgoog_foo_bar"));
  EXPECT_EQ("", LookupAdsByGoogleAttributeName("google"));
}

}  // namespace
}  // namespace ads_attribute
}  // namespace net_instaweb
