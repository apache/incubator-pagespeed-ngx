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

  // Return the value currently stored.  value() has acquire semantics, meaning
  // that after reading the value the reading thread can see writes performed by
  // another thread prior to the point where it wrote the value to set_value or
  // CompareAndSwap.
  int32 value() const {
    return base::subtle::Acquire_Load(&value_);
  }

  // Add atomically amount to the value currently stored, return the new value.
  // Has no ordering semantics with respect to other operations.
  int32 increment(int32 amount) {
    return base::subtle::NoBarrier_AtomicIncrement(&value_, amount);
  }

  // Store value.  Has release semantics, meaning prior writes made by this
  // thread will be visible to subsequent callers of value() that read the value
  // we stored.
  void set_value(int32 value) {
    base::subtle::Release_Store(&value_, value);
  }

  // atomic compare and swap.  If value_ = old_value, atomically replace it with
  // new_value.  Return the initial contents of value_ regardless of whether the
  // swap occurred.  Has release semantics as with set_value().  NOTE: does NOT
  // have acquire semantics, so the value returned may not appear to be ordered
  // with respect to subsequent reads of other memory locations.  We can't
  // expect to see changes made by prior writers using CompareAndSwap alone.
  int32 CompareAndSwap(int32 old_value, int32 new_value) {
    return
        base::subtle::Release_CompareAndSwap(&value_, old_value, new_value);
  }

 private:
  base::subtle::AtomicWord value_;
  DISALLOW_COPY_AND_ASSIGN(AtomicInt32);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ATOMIC_INT32_H_
