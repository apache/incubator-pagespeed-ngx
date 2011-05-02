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
// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_UTIL_THREAD_SYSTEM_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_THREAD_SYSTEM_TEST_BASE_H_

#include "net/instaweb/util/public/thread_system.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"

namespace net_instaweb {

class ThreadSystemTestBase : public testing::Test {
 public:
  // Data transfer between thread & main.
  void set_ok_flag(bool ok) { ok_flag_ = ok; }
  bool ok_flag() const { return ok_flag_; }

  ThreadSystem* thread_system() const { return thread_system_.get(); }

 protected:
  // Takes ownership of 'thread_system'
  explicit ThreadSystemTestBase(ThreadSystem* thread_system);

  // Test simple start & join.
  void TestStartJoin();

  // Very basic use of synchronization --- waiting for thread
  // to notify us. Also tests detached execution.
  void TestSync();

 private:
  bool ok_flag_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSystemTestBase);
};

// Passes in the appropriate ThreadSystem to ThreadSystemTestBase via a template
// param to help glue us to the test framework
template<typename ToTest>
class ThreadSystemTestTemplate : public ThreadSystemTestBase {
 public:
  ThreadSystemTestTemplate() : ThreadSystemTestBase(new ToTest) {}
};

TYPED_TEST_CASE_P(ThreadSystemTestTemplate);

TYPED_TEST_P(ThreadSystemTestTemplate, TestStartJoin) {
  ThreadSystemTestBase::TestStartJoin();
}

TYPED_TEST_P(ThreadSystemTestTemplate, TestSync) {
  ThreadSystemTestBase::TestSync();
}

REGISTER_TYPED_TEST_CASE_P(ThreadSystemTestTemplate, TestStartJoin, TestSync);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_THREAD_SYSTEM_TEST_BASE_H_
