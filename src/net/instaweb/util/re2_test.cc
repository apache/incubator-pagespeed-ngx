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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/util/public/gtest.h"

#include "net/instaweb/util/public/re2.h"

namespace net_instaweb {

class Re2Test : public testing::Test {
};

TEST_F(Re2Test, FullMatch) {
  EXPECT_FALSE(RE2::FullMatch("helo", "h.*oo"));
  EXPECT_TRUE(RE2::FullMatch("helo", "h.*o"));
}

}  // namespace
