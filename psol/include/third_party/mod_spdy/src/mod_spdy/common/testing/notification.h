// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MOD_SPDY_COMMON_TESTING_NOTIFICATION_H_
#define MOD_SPDY_COMMON_TESTING_NOTIFICATION_H_

#include "base/basictypes.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"

namespace base { class TimeDelta; }

namespace mod_spdy {

namespace testing {

// A Notification allows one thread to Wait() until another thread calls Set()
// at least once.  In order to help avoid deadlock, Set() is also called by the
// destructor.
class Notification {
 public:
  Notification();
  ~Notification();

  // Set the notification.
  void Set();
  // Block until the notification is set.
  void Wait();
  // In a unit test, expect that the notification has not yet been set.
  void ExpectNotSet();
  // In a unit test, expect that the notification is currently set, or becomes
  // set by another thread within the give time delta.
  void ExpectSetWithin(const base::TimeDelta& delay);
  void ExpectSetWithinMillis(int millis);

 private:
  base::Lock lock_;
  base::ConditionVariable condvar_;
  bool is_set_;

  DISALLOW_COPY_AND_ASSIGN(Notification);
};

}  // namespace testing

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_TESTING_NOTIFICATION_H_
