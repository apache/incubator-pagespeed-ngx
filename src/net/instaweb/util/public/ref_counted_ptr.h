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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
#define NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_

#include "base/basictypes.h"

#include "base/ref_counted.h"

namespace net_instaweb {


template<class T>
class RefCounted : public base::RefCountedThreadSafe<T> {
};

// Template class to help make reference-counted pointers.  You can use
// a typedef or subclass RefCountedPtr<YourClass>.  YourClass has to inherit
// off RefCounted<T>.
template<class T>
class RefCountedPtr : public scoped_refptr<T> {
 public:
  RefCountedPtr() : scoped_refptr<T>(NULL) {}
  explicit RefCountedPtr(T* t) : scoped_refptr<T>(t) {}

  template<class U>
  explicit RefCountedPtr(const RefCountedPtr<U>& src)
      : scoped_refptr<T>(src) {
  }

  // Determines whether any other RefCountedPtr objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return !this->ptr_ || this->ptr_->HasOneRef(); }

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
  operator void*() const;  // don't compare directly to NULL; use get()
  operator T*() const;     // don't assign directly to pointer; use get()

  // Note that copy and assign of RefCountedPtr is allowed -- that
  // is how the reference counts are updated.
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

// Helper macro to allow declaration of user-visible macro
// REFCOUNT_DISALLOW_EXPLICIT_DESTROY which is used to generate
// compile-time errors for code that deletes ref-counted objects
// explicitly.
#define REFCOUNT_SHARED_MEM_IMPL_CLASS base::RefCountedThreadSafe


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
  friend class REFCOUNT_SHARED_MEM_IMPL_CLASS<class_name>

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
