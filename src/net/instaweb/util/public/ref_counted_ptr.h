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
//
// TODO(jmaessen): explore adding C++x0 shared_ptr support
// TODO(jmarantz): Refactor these two blocks of template magic for
// RefCountedPtr and RefCountedObj.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
#define NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_

#include "base/basictypes.h"

#include "base/ref_counted.h"

namespace net_instaweb {


// Predeclare these templates so they can be friended by helpers.
template<class T> class RefCountedPtr;
template<class T> class RefCountedObj;

// Helper class for RefCountedPtr<T>.  Do not instantiate directly.
template<class T>
class RefCountedPtrHelper
    : public base::RefCountedThreadSafe<RefCountedPtrHelper<T> > {
 private:
  friend class base::RefCountedThreadSafe<RefCountedPtrHelper>;
  friend class RefCountedPtr<T>;

  RefCountedPtrHelper() : object_(NULL) {}
  ~RefCountedPtrHelper() { delete object_; }
  explicit RefCountedPtrHelper(T* t) : object_(t) {}
  T* get() { return object_; }
  const T* get() const { return object_; }

  T* object_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedPtrHelper);
};

// Template class to help make reference-counted pointers.  You can use
// a typedef or subclass RefCountedPtr<YourClass>.  YourClass does not
// have to implement any helper methods, and does not require a
// copy-constructor.
//
// Use this class rather than RefCountedPtr if you require polymorphism
// or the ability to represent NULL.  The cost is one level of indirection.
template<class T>
class RefCountedPtr : public scoped_refptr<RefCountedPtrHelper<T> > {
 public:
  typedef RefCountedPtrHelper<T> Helper;

  RefCountedPtr() : scoped_refptr<Helper>(new Helper) {}
  explicit RefCountedPtr(T* t) : scoped_refptr<Helper>(new Helper(t)) {}

  // Determines whether any other RefCountedPtr objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return this->ptr_->HasOneRef(); }

  T* get() { return this->ptr_->get(); }
  const T* get() const { return this->ptr_->get(); }
  T* operator->() { return this->ptr_->get(); }
  const T* operator->() const { return this->ptr_->get(); }
  T& operator*() { return this->ptr_->get(); }
  const T& operator*() const { return this->ptr_->get(); }

  // Note that copy and assign of RefCountedPtr is allowed -- that
  // is how the reference counts are updated.
};

// Helper class for RefCountedObj<T>.  Do not instantiate directly.
template<class T>
class RefCountedObjHelper
    : public base::RefCountedThreadSafe<RefCountedObjHelper<T> > {
 private:
  friend class base::RefCountedThreadSafe<RefCountedObjHelper>;
  friend class RefCountedObj<T>;

  RefCountedObjHelper() {}
  ~RefCountedObjHelper() {}
  explicit RefCountedObjHelper(const T& t) : object_(t) {}
  T* get() { return &object_; }
  const T* get() const { return &object_; }

  T object_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedObjHelper);
};

// Template class to help make reference-counted objects.  You can use
// a typedef or subclass RefCountedObj<YourClass>.  YourClass does not
// have to implement any helper methods, and does not require a
// copy-constructor.
//
// Use this class rather than RefCountedObj if do not need polymorphism:
// this class embeds the object directly so has one less level of
// indirection compared to RefCountedPtr.
template<class T>
class RefCountedObj : public scoped_refptr<RefCountedObjHelper<T> > {
 public:
  RefCountedObj()
      : scoped_refptr<RefCountedObjHelper<T> >(new RefCountedObjHelper<T>) {}
  explicit RefCountedObj(const T& t)
      : scoped_refptr<RefCountedObjHelper<T> >(new RefCountedObjHelper<T>(t)) {
  }

  // Determines whether any other RefCountedObj objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return this->ptr_->HasOneRef(); }

  T* get() { return this->ptr_->get(); }
  const T* get() const { return this->ptr_->get(); }
  T* operator->() { return this->ptr_->get(); }
  const T* operator->() const { return this->ptr_->get(); }
  T& operator*() { return *this->ptr_->get(); }
  const T& operator*() const { return *this->ptr_->get(); }

  // Note that copy and assign of RefCountedObj is allowed -- that
  // is how the reference counts are updated.
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
