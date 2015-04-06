/*
 * Copyright 2010 Google Inc.
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
//
// Implements a generic ref-counted class, with full sharing.  This
// class does *not* implement copy-on-write semantics, but it provides
// 'unique()', which helps implement COW at a higher level.
//
// There are two pointer templates here:
// - RefCountedPtr<T> --- requires T to inherit off RefCounted<T>,
//   stores it by pointer to supports full polymorphism.
// - RefCountedObj<T> --- no requirements on T besides default and copy
//   construction, but stores T by value so it must always store exactly T.
//
// TODO(jmaessen): explore adding C++x0 shared_ptr support

#ifndef PAGESPEED_KERNEL_UTIL_REF_COUNTED_PTR_H_
#define PAGESPEED_KERNEL_UTIL_REF_COUNTED_PTR_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/atomic_int32.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

template<class T>
class RefCounted {
 public:
  RefCounted() : ref_count_(0) {}
  ~RefCounted() { DCHECK_EQ(0, ref_count_.value()); }

  void Release() {
    if (ref_count_.BarrierIncrement(-1) == 0) {
      delete static_cast<T*>(this);
    }
  }

  void AddRef() {
    ref_count_.NoBarrierIncrement(1);
  }

  bool HasOneRef() {
    return (ref_count_.value() == 1);
  }

 private:
  AtomicInt32 ref_count_;
  DISALLOW_COPY_AND_ASSIGN(RefCounted<T>);
};

// Template class to help make reference-counted pointers.  You can use
// a typedef or subclass RefCountedPtr<YourClass>.  YourClass has to inherit
// off RefCounted<T>.
template<class T>
class RefCountedPtr {
 public:
  RefCountedPtr() : ptr_(NULL) {}
  explicit RefCountedPtr(T* t) : ptr_(t) {
    if (t != NULL) {
      t->AddRef();
    }
  }

  RefCountedPtr(const RefCountedPtr<T>& src)
      : ptr_(src.ptr_) {
    if (ptr_ != NULL) {
      ptr_->AddRef();
    }
  }

  template<class U>
  explicit RefCountedPtr(const RefCountedPtr<U>& src)
      : ptr_(static_cast<T*>(src.ptr_)) {
    if (ptr_ != NULL) {
      ptr_->AddRef();
    }
  }

  ~RefCountedPtr() {
    if (ptr_ != NULL) {
      ptr_->Release();
    }
  }

  RefCountedPtr<T>& operator=(const RefCountedPtr<T>& other) {
    // ref before deref to deal with self-assignment.
    if (other.ptr_ != NULL) {
      other.ptr_->AddRef();
    }
    if (ptr_ != NULL) {
      ptr_->Release();
    }
    ptr_ = other.ptr_;
    return *this;
  }

  template<class U>
  RefCountedPtr<T>& operator=(const RefCountedPtr<U>& other) {
    if (other.ptr_ != NULL) {
      other.ptr_->AddRef();
    }
    if (ptr_ != NULL) {
      ptr_->Release();
    }
    ptr_ = static_cast<T*>(other.ptr_);
    return *this;
  }

  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }

  // Determines whether any other RefCountedPtr objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return !this->ptr_ || this->ptr_->HasOneRef(); }

  template<typename U>
  RefCountedPtr<U> StaticCast() const {
    return RefCountedPtr<U>(static_cast<U*>(this->get()));
  }

  void clear() {
    *this = RefCountedPtr();
  }
  void reset(T* ptr) {
    *this = RefCountedPtr(ptr);
  }
  void reset(const RefCountedPtr& src) {
    *this = src;
  }

 private:
  template <class U> friend class RefCountedPtr;
  operator void*() const;  // don't compare directly to NULL; use get()
  operator T*() const;     // don't assign directly to pointer; use get()

  // Note that copy and assign of RefCountedPtr is allowed -- that
  // is how the reference counts are updated.
  T* ptr_;
};

// If you can't inherit off RefCounted due to using a pre-existing
// class, you can use RefCountedObj instead. This however is limited to
// having a single type (so no polymorphism). It also has slightly
// different semantics in that it initializes to a default-constructed object
// and not NULL.
template<class T>
class RefCountedObj {
 public:
  RefCountedObj() : data_ptr_(new Data()) {}
  explicit RefCountedObj(const T& val) : data_ptr_(new Data(val)) {}

  // Determines whether any other RefCountedObj objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return data_ptr_.unique(); }

  T* get() { return &data_ptr_->value; }
  const T* get() const { return &data_ptr_->value; }
  T* operator->() { return &data_ptr_->value; }
  const T* operator->() const { return &data_ptr_->value; }
  T& operator*() { return data_ptr_->value; }
  const T& operator*() const { return data_ptr_->value; }

  // Sets the object to contain a new value, detaching it
  // from any other RefCountedObj instances that were previously
  // sharing data.
  void reset(const T& val) { data_ptr_.reset(new Data(val)); }

 protected:
  struct Data : public RefCounted<Data> {
    Data() {}
    explicit Data(const T& val) : value(val) {}
    T value;
  };

  RefCountedPtr<Data> data_ptr_;

 private:
  operator void*() const;  // don't compare directly to NULL; use get()
  operator T*() const;     // don't assign directly to pointer; use get()

  // Copying, etc., are OK thanks to data_ptr_.
};

// Macro for users implementing C++ ref-counted classes to prevent
// explicit destruction.  Once a class is reference counted, it
// should never be stack-allocated or explicitly deleted.  It should
// only be deleted by the reference count object.  Put this declaration
// in the 'protected:' or 'private:' section, and group it with
// a destructor declaration.
//
// This is only required for RefCountedPtr<T>, not RefCountedObj<T>.
//
#define REFCOUNT_FRIEND_DECLARATION(class_name) \
  friend class net_instaweb::RefCounted<class_name>

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_REF_COUNTED_PTR_H_
