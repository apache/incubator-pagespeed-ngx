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
// Author: cheesy@google.com (Steve Hill)

// Using the _impl header so we always get the open source version.
#include "pagespeed/kernel/base/proto_matcher_impl.h"

#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/proto_matcher_test.pb.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

// Unit test for EqualsProto.

using net_instaweb::EqualsProto;
using testing::Not;

TEST(ProtoMatcherTest, MatchesEmpty) {
  ProtoMatcherTestMessage proto;
  EXPECT_THAT(proto, EqualsProto(""));
}

TEST(ProtoMatcherTest, BasicMatch) {
  // Using the C++ api to populate the proto since EqualsProto depends on
  // ParseTextFormatProtoFromString.
  ProtoMatcherTestMessage proto;
  proto.set_a(2);
  proto.set_b(5);

  EXPECT_THAT(proto, EqualsProto("a:2 b:5"));
}

TEST(ProtoMatcherTest, NegativeMatch) {
  ProtoMatcherTestMessage proto;
  proto.set_a(2);
  proto.set_b(5);

  EXPECT_THAT(proto, Not(EqualsProto("a:1 b:2")));
}

TEST(ProtoMatcherTest, PartialMatch) {
  // MessageDifferencer, the underlying mechanism of EqualsProto(), has a
  // variety of matching methods. EqualsProto() is the most strict and should
  // require exact equality, ie: an explicitly set value must be explicitly set
  // and cannot match against a default value. Verify that's what actually
  // happens.
  ProtoMatcherTestMessage proto;
  proto.set_a(5);
  EXPECT_THAT(proto, Not(EqualsProto("a:5 b:0")));

  proto.set_b(0);
  EXPECT_THAT(proto, EqualsProto("a:5 b:0"));
}

}  // namespace

}  // namespace net_instaweb
