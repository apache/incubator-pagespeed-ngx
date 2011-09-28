/*
 * Copyright 2011 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ATOMIC_INT32_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ATOMIC_INT32_H_

#include "net/instaweb/util/public/atomicops.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// An int32 flag that can be set atomically and be visible to other
// threads. Please be extra careful with this --- it can go wrong in
// incomprehensible  ways; most of the time, you probably want to use a mutex
// instead.

class AtomicInt32 {
 public:
  explicit AtomicInt32(int32 value) {
    set_value(value);
  }
  AtomicInt32() {
    set_value(0);
  }
  ~AtomicInt32() {}

  int32 value() const {
    return base::subtle::Acquire_Load(&value_);
  }

  void increment(int32 amount) {
    // TODO(jmaessen): Please review the semantics of this.
    base::subtle::NoBarrier_AtomicIncrement(&value_, amount);
  }

  void set_value(int32 value) {
    base::subtle::Release_Store(&value_, value);
  }

 private:
  base::subtle::AtomicWord value_;
  DISALLOW_COPY_AND_ASSIGN(AtomicInt32);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ATOMIC_INT32_H_
