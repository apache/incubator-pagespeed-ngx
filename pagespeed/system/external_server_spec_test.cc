// Copyright 2016 Google Inc.
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
//
// Author: yeputons@google.com (Egor Suvorov)
#include "pagespeed/system/external_server_spec.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {
static const int kDefaultPortForTesting = 100;
}  // namespace

TEST(ExternalServerSpecTest, IsEmptyByDefault) {
  ExternalServerSpec spec;
  EXPECT_TRUE(spec.empty());
}

TEST(ExternalServerSpecTest, SpecifyHostAndPort) {
  GoogleString msg;
  ExternalServerSpec spec;
  EXPECT_TRUE(
      spec.SetFromString("example.com:1234", kDefaultPortForTesting, &msg));
  EXPECT_EQ("", msg);
  EXPECT_EQ("example.com", spec.host);
  EXPECT_EQ(1234, spec.port);
}

TEST(ExternalServerSpecTest, HostOnly) {
  GoogleString msg;
  ExternalServerSpec spec;
  EXPECT_TRUE(spec.SetFromString("example.com", kDefaultPortForTesting, &msg));
  EXPECT_EQ("", msg);
  EXPECT_FALSE(spec.empty());
  EXPECT_EQ("example.com", spec.host);
  EXPECT_EQ(kDefaultPortForTesting, spec.port);
}

TEST(ExternalServerSpecTest, ToString) {
  ExternalServerSpec spec("example.com", 1234);
  EXPECT_EQ("example.com:1234", spec.ToString());
}

class ExternalServerSpecTestInvalid : public ::testing::Test {
 protected:
  void TestInvalidSpec(const GoogleString &value) {
    GoogleString msg;
    ExternalServerSpec spec("old.com", 4321);
    EXPECT_FALSE(spec.SetFromString(value, kDefaultPortForTesting, &msg));
    EXPECT_NE("", msg);
    EXPECT_EQ("old.com", spec.host);
    EXPECT_EQ(4321, spec.port);
  }
};

TEST_F(ExternalServerSpecTestInvalid, NonNumericPort) {
  TestInvalidSpec("host:1port");
}

TEST_F(ExternalServerSpecTestInvalid, InvalidPortNumber1) {
  TestInvalidSpec("host:0");
}

TEST_F(ExternalServerSpecTestInvalid, InvalidPortNumber2) {
  TestInvalidSpec("host:100000");
}

TEST_F(ExternalServerSpecTestInvalid, Empty) {
  TestInvalidSpec("");
}

TEST_F(ExternalServerSpecTestInvalid, EmptyHostAndPort) {
  TestInvalidSpec(":");
}

TEST_F(ExternalServerSpecTestInvalid, EmptyHostWithPort) {
  TestInvalidSpec(":1234");
}

TEST_F(ExternalServerSpecTestInvalid, EmptyPortWithHost) {
  TestInvalidSpec("host:");
}

TEST_F(ExternalServerSpecTestInvalid, MultipleColons) {
  TestInvalidSpec("host:10:20");
}

TEST(ExternalClusterSpec, ParseEmptySpec) {
  ExternalClusterSpec spec = {{ExternalServerSpec("host", 10)}};
  GoogleString msg;
  EXPECT_TRUE(spec.SetFromString("", kDefaultPortForTesting, &msg));
  EXPECT_EQ("", msg);
  EXPECT_TRUE(spec.empty());
}

TEST(ExternalClusterSpec, SingleServer) {
  GoogleString msg;
  ExternalClusterSpec spec;
  EXPECT_TRUE(spec.SetFromString("host1", kDefaultPortForTesting, &msg));
  EXPECT_EQ("", msg);
  EXPECT_FALSE(spec.empty());
  ASSERT_EQ(1, spec.servers.size());
  EXPECT_EQ("host1", spec.servers[0].host);
  EXPECT_EQ(kDefaultPortForTesting, spec.servers[0].port);
}

TEST(ExternalClusterSpec, MultipleServers) {
  GoogleString msg;
  ExternalClusterSpec spec = {{ExternalServerSpec("invalid", 1)}};
  EXPECT_TRUE(spec.SetFromString("host1:10,host2,host3:20",
                                 kDefaultPortForTesting, &msg));
  EXPECT_EQ("", msg);
  EXPECT_FALSE(spec.empty());
  ASSERT_EQ(3, spec.servers.size());
  EXPECT_EQ("host1", spec.servers[0].host);
  EXPECT_EQ(10, spec.servers[0].port);
  EXPECT_EQ("host2", spec.servers[1].host);
  EXPECT_EQ(kDefaultPortForTesting, spec.servers[1].port);
  EXPECT_EQ("host3", spec.servers[2].host);
  EXPECT_EQ(20, spec.servers[2].port);
}

TEST(ExternalClusterSpec, InvalidStringDoesNotOverride) {
  GoogleString msg;
  ExternalClusterSpec spec = {{ExternalServerSpec("host1", 10),
                               ExternalServerSpec("host2", 20),
                               ExternalServerSpec("host3", 30)}};
  EXPECT_FALSE(
      spec.SetFromString("host4:40,host5:port", kDefaultPortForTesting, &msg));
  EXPECT_NE("", msg);
  EXPECT_FALSE(spec.empty());
  ASSERT_EQ(3, spec.servers.size());
  EXPECT_EQ("host1", spec.servers[0].host);
  EXPECT_EQ(10, spec.servers[0].port);
  EXPECT_EQ("host2", spec.servers[1].host);
  EXPECT_EQ(20, spec.servers[1].port);
  EXPECT_EQ("host3", spec.servers[2].host);
  EXPECT_EQ(30, spec.servers[2].port);
}

TEST(ExternalClusterSpec, InvalidWithEmptyServer) {
  GoogleString msg;
  ExternalClusterSpec spec;
  EXPECT_FALSE(
      spec.SetFromString("host1:40,,host3:50", kDefaultPortForTesting, &msg));
  EXPECT_NE("", msg);
  EXPECT_TRUE(spec.empty());
  EXPECT_EQ(0, spec.servers.size());
}

TEST(ExternalClusterSpec, ToStringEmpty) {
  ExternalClusterSpec spec;
  EXPECT_EQ("", spec.ToString());
}

TEST(ExternalClusterSpec, ToStringSingle) {
  ExternalClusterSpec spec = {{ExternalServerSpec("server", 1234)}};
  EXPECT_EQ("server:1234", spec.ToString());
}

TEST(ExternalClusterSpec, ToStringMultiple) {
  ExternalClusterSpec spec = {{ExternalServerSpec("server1", 1234),
                               ExternalServerSpec("server2", 4567)}};
  EXPECT_EQ("server1:1234,server2:4567", spec.ToString());
}

}  // namespace net_instaweb
