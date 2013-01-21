// Copyright 2011 Google Inc.
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
// Author: jhoch@google.com (Jason Hoch)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_TEST_BASE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics_test_base.h"

namespace net_instaweb {

class HashedRefererStatistics;
class SharedMemTestEnv;

class HashedRefererStatisticsTestBase
    : public SharedMemRefererStatisticsTestBase {
 protected:
  explicit HashedRefererStatisticsTestBase(SharedMemTestEnv* test_env)
      : SharedMemRefererStatisticsTestBase(test_env) {}

  void TestHashed();

  HashedRefererStatistics* ChildInit();
  HashedRefererStatistics* ParentInit();

 private:
  DISALLOW_COPY_AND_ASSIGN(HashedRefererStatisticsTestBase);
};

template<typename ConcreteTestEnv>
class HashedRefererStatisticsTestTemplate
    : public HashedRefererStatisticsTestBase {
 public:
  HashedRefererStatisticsTestTemplate()
      : HashedRefererStatisticsTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(HashedRefererStatisticsTestTemplate);

TYPED_TEST_P(HashedRefererStatisticsTestTemplate, TestHashed) {
  HashedRefererStatisticsTestBase::TestHashed();
}

REGISTER_TYPED_TEST_CASE_P(HashedRefererStatisticsTestTemplate, TestHashed);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_TEST_BASE_H_
