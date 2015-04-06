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
//
// This contains very basic smoke tests for ThreadSystem subclass operation

#include "pagespeed/kernel/thread/thread_system_test_base.h"

#include "pagespeed/kernel/base/abstract_mutex.h"  // for ScopedMutex, etc
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"

namespace net_instaweb {

namespace {

// Joinable thread that sets the OK flag.
class SuccessThread : public ThreadSystem::Thread {
 public:
  explicit SuccessThread(ThreadSystemTestBase* owner)
      : Thread(owner->thread_system(), "success", ThreadSystem::kJoinable),
        owner_(owner) {}

  virtual void Run() {
    owner_->set_ok_flag(true);
  }

 private:
  ThreadSystemTestBase* owner_;
};

// See TestSync below for what this does
class ToggleThread : public ThreadSystem::Thread {
 public:
  ToggleThread(ThreadSystemTestBase* owner,
               AbstractMutex* lock,
               ThreadSystem::Condvar* notify_true,
               ThreadSystem::Condvar* notify_false)
      : Thread(owner->thread_system(), "toggle", ThreadSystem::kDetached),
        owner_(owner),
        lock_(lock),
        notify_true_(notify_true),
        notify_false_(notify_false),
        parent_id_(owner->thread_system()->GetThreadId()) {
  }

  virtual void Run() {
    // Check whether our ID is not the same as our parent, and
    // vice versa.
    scoped_ptr<ThreadSystem::ThreadId> id(
        owner_->thread_system()->GetThreadId());
    EXPECT_FALSE(parent_id_->IsEqual(*id.get()));
    EXPECT_FALSE(id->IsEqual(*parent_id_.get()));

    EXPECT_FALSE(parent_id_->IsCurrentThread());
    EXPECT_TRUE(id->IsCurrentThread());

    // Check that if we strobe our ID a second time it matches.
    scoped_ptr<ThreadSystem::ThreadId> id_check(
        owner_->thread_system()->GetThreadId());
    EXPECT_TRUE(id_check->IsEqual(*id.get()));

    // Wait for parent to set it to true.
    {
      ScopedMutex hold_lock(lock_);
      while (!owner_->ok_flag()) {
        notify_true_->Wait();
      }
    }

    ASSERT_TRUE(owner_->ok_flag());  // if we exit here, all sorts of things
                                     // will fire, like leak warnings, etc.

    // Set it to false, and notify it.
    {
      ScopedMutex hold_lock(lock_);
      owner_->set_ok_flag(false);
      notify_false_->Signal();
    }

    delete this;
  }

 private:
  ThreadSystemTestBase* owner_;
  AbstractMutex* lock_;
  ThreadSystem::Condvar* notify_true_;
  ThreadSystem::Condvar* notify_false_;
  scoped_ptr<ThreadSystem::ThreadId> parent_id_;
};

}  // namespace

ThreadSystemTestBase::ThreadSystemTestBase(ThreadSystem* thread_system)
    : ok_flag_(false),
      thread_system_(thread_system),
      handler_(thread_system_->NewMutex()) {
}

void ThreadSystemTestBase::TestStartJoin() {
  SuccessThread test_thread(this);
  ASSERT_TRUE(test_thread.Start());
  test_thread.Join();
  EXPECT_TRUE(ok_flag_);
}

void ThreadSystemTestBase::TestSync() {
  scoped_ptr<ThreadSystem::CondvarCapableMutex> lock(
      thread_system_->NewMutex());
  scoped_ptr<ThreadSystem::Condvar> notify_true(lock->NewCondvar());
  scoped_ptr<ThreadSystem::Condvar> notify_false(lock->NewCondvar());

  ToggleThread* thread =
      new ToggleThread(this, lock.get(), notify_true.get(), notify_false.get());
  ASSERT_TRUE(thread->Start());

  // We first signal here -> kid that ok is true, then go in the other
  // direction; doing a normal conditional variable sleep in the meantime.
  //
  // this also tests a detached thread.
  {
    ScopedMutex hold_lock(lock.get());
    ok_flag_ = true;
    notify_true->Signal();
  }

  // Now wait for it to become false
  {
    ScopedMutex hold_lock(lock.get());
    while (ok_flag_) {
      notify_false->Wait();
    }
  }

  ASSERT_FALSE(ok_flag_);
}

}  // namespace net_instaweb
