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

// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MANUALLY_REF_COUNTED_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MANUALLY_REF_COUNTED_H_

#include "base/logging.h"
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// Class that assists with manual reference counting.  Other classes should
// inherit from this one, and users of those classes should call IncrementRefs
// when making copies and DecrementRefs when they finish with those copies.
class ManuallyRefCounted {
 public:
  ManuallyRefCounted() : n_active(1) {}

  // Call this when duplicating a pointer to subclass instances:
  //   Example* e = other->GetExample();
  //   e->IncrementRefs();
  void IncrementRefs() {
    n_active.increment(1);
  }

  // Call this when finished with a pointer to a subclass instance:
  //   class ExampleHolder {
  //     Example* e;
  //     ~ExampleHolder() {
  //       if (e != NULL) {
  //         // If e is the last pointer to *e then *e will be deleted.
  //         e->DecrementRefs();
  //       }
  //     }
  //   }
  void DecrementRefs() {
    int n_others_active = n_active.increment(-1);
    if (n_others_active == 0) {
      delete this;
    }
  }

 protected:
  // The destructor isn't public because instances of ManuallyRefCounted
  // subclasses should not be directly deleted.  They should be released via
  // DecrementRefs().
  virtual ~ManuallyRefCounted() {
    DCHECK_EQ(0, n_active.value());
  }

 private:
  AtomicInt32 n_active;

  DISALLOW_COPY_AND_ASSIGN(ManuallyRefCounted);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MANUALLY_REF_COUNTED_H_
