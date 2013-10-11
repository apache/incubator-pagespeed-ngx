/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
// Unit-tests for various User Agent string normalizations

#include "pagespeed/kernel/http/user_agent_normalizer.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

const char kChrome[] =
    "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.31 "
    "(KHTML, like Gecko) Chrome/26.0.1410.64 Safari/537.31";

class UserAgentNormalizerTest : public ::testing::Test {
 protected:
  IEUserAgentNormalizer normalize_ie_;
  AndroidUserAgentNormalizer normalize_android_;
};

TEST_F(UserAgentNormalizerTest, IE) {
  // Shouldn't affect a totally different UA
  EXPECT_EQ(kChrome, normalize_ie_.Normalize(kChrome));

  // Various plugins get stripped
  EXPECT_EQ(
      "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0)",
      normalize_ie_.Normalize(
          "Mozilla/4.0 (compatible; MSIE 8.0; "
          "Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727;"
          " OfficeLiveConnector.1.3; OfficeLivePatch.0.0)"));

  // We do keep 'chromeframe', though, as it can mean a different renderer
  // might be in used.
  EXPECT_EQ(
    "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; "
        "chromeframe/26.0.1410.64)",
    normalize_ie_.Normalize(
      "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; "
      "SV1; chromeframe/26.0.1410.64)"));

  // Make sure Windows Phone doesn't get bundled in, too, since it may
  // be useful to tell it apart. Also makes sure we do handle Mozilla/5.0
  // strings variants as well.
  EXPECT_EQ(
      "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; "
          "IEMobile/9.0)",
      normalize_ie_.Normalize(
          "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; "
          "Trident/5.0; IEMobile/9.0; NOKIA; Lumia 800)"));
}

TEST_F(UserAgentNormalizerTest, NormalizeAndroidChromeUnaffected) {
  // Shouldn't affect a totally different UA
  EXPECT_EQ(kChrome, normalize_android_.Normalize(kChrome));
}

TEST_F(UserAgentNormalizerTest, NormalizeDalvik) {
  // Lots of fetches from android seem to be identified as being from the VM
  // rather than the browser.
  EXPECT_EQ(
      "Dalvik/1.4.0 (Linux; U; Android 2.3.4)",
      normalize_android_.Normalize(
          "Dalvik/1.4.0 (Linux; U; Android 2.3.4; GT-N7000 Build/GRJ22)"));

  EXPECT_EQ(
      "Dalvik/1.6.0 (Linux; U; Android 4.2.2)",
      normalize_android_.Normalize(
          "Dalvik/1.6.0 (Linux; U; Android 4.2.2; Nexus 7 Build/JDQ39)"));

  EXPECT_EQ(
      "Dalvik/1.4.0 (Linux; U; Android 2.3.4)",
      normalize_android_.Normalize(
          "Dalvik/1.4.0 (Linux; U; Android 2.3.4; "
          "BlueStacks-00000000-0000-0000-0000-000000000000 Build/GRJ22)"));
}

TEST_F(UserAgentNormalizerTest, NormalizeBrowser) {
  // Test for how we normalize the pre-Chrome Android Browser
  EXPECT_EQ(
      "Mozilla/5.0 (Linux; U; Android 2.2; )"
          " AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0"
          " Mobile Safari/533.1",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; U; Android 2.2; en-us; Desire_A8181 Build/FRF91)"
              " AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0"
              " Mobile Safari/533.1"));

  EXPECT_EQ(
      "Mozilla/5.0 (Linux; U; Android 4.1.2; )"
          " AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0"
          " Mobile Safari/534.30",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; U; Android 4.1.2; en-gb; GT-I9300 Build/JZO54K)"
          " AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0"
          " Mobile Safari/534.30"));

  EXPECT_EQ(
      "Mozilla/5.0 (Linux; U; Android 1.5; ) AppleWebKit/528.5+"
          " (KHTML, like Gecko) Version/3.1.2 Mobile Safari/525.20.1",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; U; Android 1.5; en-us) AppleWebKit/528.5+"
          " (KHTML, like Gecko) Version/3.1.2 Mobile Safari/525.20.1"));

  // Tablet UA string as well --- it lacks 'Mobile'.
  EXPECT_EQ(
      "Mozilla/5.0 (Linux; U; Android 4.0.4; )"
          " AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Safari/534.30",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; U; Android 4.0.4; en-gb; GT-N8000 Build/IMM76D)"
          " AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Safari/534.30"));
}

TEST_F(UserAgentNormalizerTest, NormalizeMobileChrome) {
  // Normalization of Android Chrome UA strings. (Desktop Chrome UA strings
  // don't need it)
  EXPECT_EQ(
      "Mozilla/5.0 (Linux; Android 4.1.2; )"
          " AppleWebKit/537.31 (KHTML, like Gecko)"
          " Chrome/26.0.1410.58 Mobile Safari/537.31",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; Android 4.1.2; GT-I9300 Build/JZO54K)"
            " AppleWebKit/537.31 (KHTML, like Gecko)"
            " Chrome/26.0.1410.58 Mobile Safari/537.31"));

  EXPECT_EQ(
      "Mozilla/5.0 (Linux; Android 4.2.2; )"
          " AppleWebKit/537.31 (KHTML, like Gecko)"
          " Chrome/26.0.1410.58 Safari/537.31",
      normalize_android_.Normalize(
            "Mozilla/5.0 (Linux; Android 4.2.2; Nexus 7 Build/JDQ39)"
                " AppleWebKit/537.31 (KHTML, like Gecko)"
                " Chrome/26.0.1410.58 Safari/537.31"));

  // Some Samsung devices also have an extra Version/1.0 thrown in for
  // some reason.
  EXPECT_EQ(
      "Mozilla/5.0 (Linux; Android 4.2.2; )"
          " AppleWebKit/535.19 (KHTML, like Gecko)"
          " Chrome/18.0.1025.308 Mobile Safari/535.19",
      normalize_android_.Normalize(
          "Mozilla/5.0 (Linux; Android 4.2.2; en-au;"
              " SAMSUNG GT-I9505 Build/JDQ39)"
              " AppleWebKit/535.19 (KHTML, like Gecko) Version/1.0"
              " Chrome/18.0.1025.308 Mobile Safari/535.19"));

  // For some reason Chrome 18 on tablets had a doubled space
  // between Chrome/ and Safari (perhaps in place of 'Mobile'), make sure
  // we properly drop the device/build name on these, too.
  EXPECT_EQ(
      "Mozilla/5.0 (Linux; Android 4.2.2; )"
          " AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166"
          "  Safari/535.19",
      normalize_android_.Normalize(
            "Mozilla/5.0 (Linux; Android 4.2.2; Nexus 7 Build/JDQ39)"
                " AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166"
                "  Safari/535.19"));
}

TEST_F(UserAgentNormalizerTest, NormalizeWithAll) {
  std::vector<const UserAgentNormalizer*> normalizers;
  normalizers.push_back(&normalize_ie_);
  normalizers.push_back(&normalize_android_);

  EXPECT_EQ(
      "Mozilla/5.0 (Linux; Android 4.1.2; )"
          " AppleWebKit/537.31 (KHTML, like Gecko)"
          " Chrome/26.0.1410.58 Mobile Safari/537.31",
      UserAgentNormalizer::NormalizeWithAll(
          normalizers,
          "Mozilla/5.0 (Linux; Android 4.1.2; GT-I9300 Build/JZO54K)"
            " AppleWebKit/537.31 (KHTML, like Gecko)"
            " Chrome/26.0.1410.58 Mobile Safari/537.31"));

  EXPECT_EQ(
      "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0)",
      UserAgentNormalizer::NormalizeWithAll(
          normalizers,
          "Mozilla/4.0 (compatible; MSIE 8.0; "
          "Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727;"
          " OfficeLiveConnector.1.3; OfficeLivePatch.0.0)"));
}

}  // namespace

}  // namespace net_instaweb
