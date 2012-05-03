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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DEQUE_VECTOR_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DEQUE_VECTOR_H_

#include <cstddef>    // for size_t
#include <cstring>    // for memcpy

#include "net/instaweb/util/public/basictypes.h"

// Simple implementation of deque using a vector which we double in
// capacity whenever we need to make room.  This alternative to
// std::deque is perhaps a little more fragmentious to memory
// allocators, but will frequently allocate much less overall memory.
//
// In particular, I found, using top, that std::deque allocates 688
// bytes to construct a deque containing 4 pointers on a 64-bit
// system.  In this implementation the cost is 64 bytes plus malloc
// overhead: 3 size_t integers and a pointer to an allocated array,
// plus the the 4 pointers in the allocated array.
//
// This implementation lacks iterators, many std::deque methods, and the
// ability to work with value-semantics for the contained object.  These
// could all be added without changing the design.
//
// Please do not instantiate this class with an object that cannot be copied
// with memcpy.  Pointers, integers, and floats are fine, as well as simple
// structs of those.
template<class T> class DequeVector {
 public:
  // Constructor provides a small initial allocation, rather than constructing
  // with zero capacity, based on expected usage patterns.
  DequeVector()
      : start_position_(0),
        size_minus_1_(static_cast<size_t>(-1)),
        capacity_minus_1_(initial_capacity() - 1),
        data_(new T[initial_capacity()]) {
  }
  ~DequeVector() {
    delete [] data_;
    data_ = NULL;
  }

  static size_t initial_capacity() { return 4; }

  void push_back(T value) {
    ExpandIfNecessary();
    ++size_minus_1_;
    *PointerAt(size_minus_1_) = value;
  }

  // Special faster versions of PointerAt(0) that avoid some math.  This
  // seems like a clear small win, however, microbenchmarking suggests it's
  // a significant performance loss.  Investigation is required.
# define SPECIAL_CASE_POINTER_AT_0 1

  void push_front(T value) {
    ExpandIfNecessary();
    start_position_ = ModCapacity(start_position_ - 1);
# if SPECIAL_CASE_POINTER_AT_0
    *PointerAt0() = value;
# else
    *PointerAt(0) = value;
# endif
    ++size_minus_1_;
  }

  void pop_back() {
    --size_minus_1_;
  }

  void pop_front() {
    start_position_ = ModCapacity(start_position_ + 1);
    --size_minus_1_;
  }

  T back() const {
    return *PointerAt(size_minus_1_);
  }

  T front() const {
# if SPECIAL_CASE_POINTER_AT_0
    return *PointerAt0();
# else
    return *PointerAt(0);
# endif
  }

  size_t capacity() const { return capacity_minus_1_ + 1; }
  size_t size() const { return size_minus_1_ + 1; }
  bool empty() const { return size_minus_1_ == static_cast<size_t>(-1); }

 private:
  // Benchmarking shows that index & (capacity - 1) is significantly
  // faster than (index % capacity) on a Dell T3500,
  // Intel Xeon(R) CPU X5650  @ 2.67GHz.  Further, we know that capacity
  // is always a power of 2.
  size_t ModCapacity(size_t index) const { return index & capacity_minus_1_; }

  // Returns a pointer to the element at the specified position.
  T* PointerAt(size_t position) {
    return data_ + ModCapacity(start_position_ + position);
  }

  const T* PointerAt(size_t position) const {
    return data_ + ModCapacity(start_position_ + position);
  }

# if SPECIAL_CASE_POINTER_AT_0
  T* PointerAt0() {
    return data_ + start_position_;
  }

  const T* PointerAt0() const {
    return data_ + start_position_;
  }
# endif
# undef SPECIAL_CASE_POINTER_AT_0

  // Expands the deque to accomodate pushing an element onto the front
  // or back.
  void ExpandIfNecessary() {
    // TODO(jmarantz): consider shrinking if the size goes way down.
    if (size_minus_1_ == capacity_minus_1_) {
      // Consider a deque with:
      //     start_position_ == 5
      //     size() == 7
      //     capacity() == 8
      //     logical order:  [ 0 1 2 3 4 5 6 ]
      //     physical order: [ 3 4 5 6 _ 0 1 2 ]
      // The first time we push, either to the beginning or end, we don't
      // need to expand.  Let's say we push -1 to the beginning; we'll
      // decrement start_position to 4 and have:
      //     start_position_ == 4
      //     size() == 8
      //     capacity() == 8
      //     logical order:  [ -1 0 1 2 3 4 5 6 ]
      //     physical order: [ 3 4 5 6 -1 0 1 2 ]
      // Now we will need to expand before we push another element, getting:
      //     start_position_ == 12
      //     size() == 8
      //     capacity() == 16
      //     logical order:  [ -1 0 1 2 3 4 5 6 ]
      //     physical order: [ 3 4 5 6 _ _ _ _ _ _ _ _ -1 0 1 2 ]
      // Now we are ready to insert 8 more elements into the gap, whether they
      // are pushed to the back or front.
      capacity_minus_1_ = 2 * capacity() - 1;
      T* old_data = data_;
      data_ = new T[capacity()];
      size_t sz = size();
      if (start_position_ == 0) {
        memcpy(data_, old_data, sz * sizeof(*data_));
      } else {
        // TODO(jmarantz): jmaessen suggests rearranging the entries to
        // be contiguous on an expansion.
        memcpy(data_, old_data, start_position_ * sizeof(*data_));
        size_t size_of_right_chunk = sz - start_position_;
        size_t new_start_position = start_position_ + sz;
        memcpy(data_ + new_start_position,
               old_data + start_position_,
               size_of_right_chunk * sizeof(*data_));
        start_position_ = new_start_position;
      }
      delete [] old_data;
    }
  }

  size_t start_position_;
  size_t size_minus_1_;
  size_t capacity_minus_1_;  // capacity is constrained to a power of 2.
  T* data_;

  DISALLOW_COPY_AND_ASSIGN(DequeVector);
};

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DEQUE_VECTOR_H_
