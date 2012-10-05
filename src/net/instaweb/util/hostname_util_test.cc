/*
 * Copyright 2012 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/util/public/hostname_util.h"

#include <unistd.h>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace {

class HostnameUtilTest : public testing::Test {
 public:
  HostnameUtilTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HostnameUtilTest);
};

TEST_F(HostnameUtilTest, GetHostname) {
  // Highly questionable test since it reimplements GetHostname, however it's
  // here in an attempt at a black-box test of GetHostname, in case it changes.
  char our_hostname[1024];
  gethostname(our_hostname, sizeof(our_hostname) - 1);

  GoogleString hostname(GetHostname());
  EXPECT_STREQ(our_hostname, hostname);
  EXPECT_STRNE("www.example.com", hostname);
}

TEST_F(HostnameUtilTest, IsLocalhost) {
  GoogleString hostname = GetHostname();

  EXPECT_TRUE(IsLocalhost("localhost", hostname));
  EXPECT_TRUE(IsLocalhost("127.0.0.1", hostname));
  EXPECT_TRUE(IsLocalhost("::1", hostname));
  EXPECT_TRUE(IsLocalhost(hostname, hostname));

  EXPECT_FALSE(IsLocalhost("localhost:8080", hostname));
  EXPECT_FALSE(IsLocalhost("localhost.example.com", hostname));
  EXPECT_FALSE(IsLocalhost("127.0.0.2", hostname));
  EXPECT_FALSE(IsLocalhost("example.com", hostname));
  EXPECT_FALSE(IsLocalhost(StrCat(hostname, ".example.com"), hostname));
  EXPECT_FALSE(IsLocalhost(hostname, ""));
  EXPECT_FALSE(IsLocalhost("http://locahost/", hostname));
  EXPECT_FALSE(IsLocalhost(StrCat("http://", hostname), hostname));
  EXPECT_FALSE(IsLocalhost(StrCat("http://", hostname, ".x.com/"), hostname));
  EXPECT_FALSE(IsLocalhost(StrCat("http://www.", hostname, "/"), hostname));
  EXPECT_FALSE(IsLocalhost(StrCat("www.", hostname), hostname));
}

}  // namespace
}  // namespace net_instaweb
