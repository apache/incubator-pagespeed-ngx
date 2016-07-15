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

// Do not use this file directly! Instead include proto_matcher.h

#ifndef PAGESPEED_KERNEL_BASE_PROTO_MATCHER_IMPL_H_
#define PAGESPEED_KERNEL_BASE_PROTO_MATCHER_IMPL_H_

#include <memory>

#include "base/logging.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/string.h"


#include "google/protobuf/util/message_differencer.h"
#include "gmock/gmock-matchers.h"

using google::protobuf::util::MessageDifferencer;


namespace net_instaweb {

class EqualsProtoMatcher {
 public:
  explicit EqualsProtoMatcher(const GoogleString& str)
      : expected_proto_str_(str) {}

  template <typename Proto>
  bool MatchAndExplain(const Proto& actual_proto,
                       testing::MatchResultListener* /* listener */) const {
    std::unique_ptr<Proto> expected_proto(actual_proto.New());
    CHECK(ParseTextFormatProtoFromString(expected_proto_str_,
                                         expected_proto.get()));
    return MessageDifferencer::Equals(*expected_proto, actual_proto);
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "matches proto: " << expected_proto_str_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not match proto: " << expected_proto_str_;
  }

 private:
  const GoogleString expected_proto_str_;
};

inline testing::PolymorphicMatcher<EqualsProtoMatcher>
EqualsProto(const GoogleString& x) {
  return testing::MakePolymorphicMatcher(EqualsProtoMatcher(x));
}

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_PROTO_MATCHER_IMPL_H_
