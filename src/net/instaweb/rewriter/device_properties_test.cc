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

#include "net/instaweb/rewriter/public/device_properties.h"

#include <vector>

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

const int kWebpArray[] = {11, 33, 55, 77, 99};
const int kJpegArray[] = {22, 44, 66, 88, 110};

const char* kSmallUserAgent = UserAgentMatcherTestBase::kAndroidNexusSUserAgent;
const char* kMediumUserAgent =
    UserAgentMatcherTestBase::kAndroidChrome21UserAgent;
const char* kLargeUserAgent =
    UserAgentMatcherTestBase::kNexus10ChromeUserAgent;

}  // namespace

class DevicePropertiesTest: public testing::Test {
 protected:
  UserAgentMatcher user_agent_matcher_;
};

TEST_F(DevicePropertiesTest, GetScreenGroupIndex) {
  int index = -1;

  EXPECT_FALSE(DeviceProperties::GetScreenGroupIndex(-1, &index));
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(0, &index));
  EXPECT_EQ(0, index);
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(1, &index));
  EXPECT_EQ(0, index);

  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
      DeviceProperties::kMediumScreenWidthThreshold - 1, &index));
  EXPECT_EQ(0, index);
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
      DeviceProperties::kMediumScreenWidthThreshold, &index));
  EXPECT_EQ(1, index);
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
      DeviceProperties::kMediumScreenWidthThreshold + 1, &index));
  EXPECT_EQ(1, index);

  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
          DeviceProperties::kLargeScreenWidthThreshold - 1, &index));
  EXPECT_EQ(1, index);
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
      DeviceProperties::kLargeScreenWidthThreshold, &index));
  EXPECT_EQ(2, index);
  EXPECT_TRUE(DeviceProperties::GetScreenGroupIndex(
      DeviceProperties::kLargeScreenWidthThreshold + 1, &index));
  EXPECT_EQ(2, index);
}

TEST_F(DevicePropertiesTest, GetPreferredImageQualitiesGood) {
  std::vector<int> webp_vector(kWebpArray, kWebpArray + arraysize(kWebpArray));
  std::vector<int> jpeg_vector(kJpegArray, kJpegArray + arraysize(kJpegArray));

  DeviceProperties device_properties(&user_agent_matcher_);
  device_properties.SetPreferredImageQualities(&webp_vector, &jpeg_vector);

  int webp, jpeg;
  EXPECT_FALSE(device_properties.GetPreferredImageQualities(
      DeviceProperties::kImageQualityDefault, &webp, &jpeg));

  device_properties.set_user_agent(kSmallUserAgent);
  int offset = -1;
  for (int i = 1; i <= DeviceProperties::kImageQualityHigh; ++i) {
    EXPECT_TRUE(device_properties.GetPreferredImageQualities(
        static_cast<DeviceProperties::ImageQualityPreference>(i),
        &webp, &jpeg));
    EXPECT_EQ(webp, kWebpArray[i + offset]);
    EXPECT_EQ(jpeg, kJpegArray[i + offset]);
  }

  device_properties.set_user_agent(kMediumUserAgent);
  offset = 0;
  for (int i = 1; i <= DeviceProperties::kImageQualityHigh; ++i) {
    EXPECT_TRUE(device_properties.GetPreferredImageQualities(
        static_cast<DeviceProperties::ImageQualityPreference>(i),
        &webp, &jpeg));
    EXPECT_EQ(webp, kWebpArray[i + offset]);
    EXPECT_EQ(jpeg, kJpegArray[i + offset]);
  }

  device_properties.set_user_agent(kLargeUserAgent);
  offset = 1;
  for (int i = 1; i <= DeviceProperties::kImageQualityHigh; ++i) {
    EXPECT_TRUE(device_properties.GetPreferredImageQualities(
        static_cast<DeviceProperties::ImageQualityPreference>(i),
        &webp, &jpeg));
    EXPECT_EQ(webp, kWebpArray[i + offset]);
    EXPECT_EQ(jpeg, kJpegArray[i + offset]);
  }
}

TEST_F(DevicePropertiesTest, GetPreferredImageQualitiesBad) {
  std::vector<int> webp_vector(kWebpArray,
                               kWebpArray + arraysize(kWebpArray) - 1);
  std::vector<int> jpeg_vector(kJpegArray,
                               kJpegArray + arraysize(kJpegArray) - 1);

  DeviceProperties device_properties(&user_agent_matcher_);
  device_properties.set_user_agent(kMediumUserAgent);
  device_properties.SetPreferredImageQualities(&webp_vector, &jpeg_vector);

  int webp, jpeg;
  EXPECT_FALSE(device_properties.GetPreferredImageQualities(
      DeviceProperties::kImageQualityMedium, &webp, &jpeg));

  device_properties.SetPreferredImageQualities(NULL, NULL);
  EXPECT_FALSE(device_properties.GetPreferredImageQualities(
      DeviceProperties::kImageQualityMedium, &webp, &jpeg));
}
}  // namespace net_instaweb
