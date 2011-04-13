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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
#define NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_

#include "base/basictypes.h"

#include "base/ref_counted.h"

namespace net_instaweb {


template<class T> class RefCountedPtr;

// Helper class for RefCountedPtr<T>.  Do not instantiate directly.
template<class T>
class RefCountedHelper
    : public base::RefCountedThreadSafe<RefCountedHelper<T> > {
 private:
  friend class base::RefCountedThreadSafe<RefCountedHelper>;
  friend class RefCountedPtr<T>;

  RefCountedHelper() {}
  ~RefCountedHelper() {}
  explicit RefCountedHelper(const T& t) : object_(t) {}
  T* get() { return &object_; }
  const T* get() const { return &object_; }

  T object_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedHelper);
};

// Template class to help make reference-counted objects.  You can use
// a typedef or subclass RefCountedPtr<YourClass>.  YourClass does not
// have to implement any helper methods, and does not require a
// copy-constructor.
template<class T>
class RefCountedPtr : public scoped_refptr<RefCountedHelper<T> > {
 public:
  RefCountedPtr()
      : scoped_refptr<RefCountedHelper<T> >(new RefCountedHelper<T>) {}
  explicit RefCountedPtr(const T& t)
      : scoped_refptr<RefCountedHelper<T> >(new RefCountedHelper<T>(t)) {
  }

  // Determines whether any other RefCountedPtr objects share the same
  // storage.  This can be used to create copy-on-write semantics if
  // desired.
  bool unique() const { return this->ptr_->HasOneRef(); }

  T* get() { return this->ptr_->get(); }
  const T* get() const { return this->ptr_->get(); }
  T* operator->() { return this->ptr_->get(); }
  const T* operator->() const { return this->ptr_->get(); }
  T& operator*() { return *this->ptr_->get(); }
  const T& operator*() const { return *this->ptr_->get(); }

  // Note that copy and assign of RefCountedPtr is allowed -- that
  // is how the reference counts are updated.
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_H_
