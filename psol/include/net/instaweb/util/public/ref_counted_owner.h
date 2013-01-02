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

// Author: morlovich@google.com (Maksim Orlovich)
//
// A RefCountedOwner<T> helps a family of objects manage lifetime of a single
// shared T, initializing it with the first owner, and getting rid of it
// when all the owners are gone. This is different from a singleton in that
// there is no limit to having only a single instance of T, but rather
// a single T instance per a single RefCountedOwner<T>::Family instance.
//
// Warning: this class doesn't provide for full thread safety; as it assumes
// that all the owners will be created and destroyed in a single thread.
// The accessors, however, are readonly, so can be used from multiple threads
// if their use follows the sequential initialization and precedes object
// destruction.
//
// Typical usage:
//  class OwnerClass {
//     static RefCountedOwner<SharedClass>::Family shared_family_;
//     RefCountedOwner<SharedClass> shared_;
//  };
//
//  OwnerClass::OwnerClass() : shared_(&shared_family_) {
//    if (shared_.ShouldInitialize()) {
//      shared_->Initialize(new SharedClass(...));
//    }
//  }
//


#ifndef NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_OWNER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_OWNER_H_

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"

// See file comment.
template<typename T>
class RefCountedOwner {
 public:
  class Family {
   public:
    Family() : ptr_(NULL), ref_count_(0) {}

   private:
    friend class RefCountedOwner<T>;
    T* ptr_;
    int ref_count_;
    DISALLOW_COPY_AND_ASSIGN(Family);
  };

  // Instances of RefCountedOwner that share the same 'family' object will
  // share an instance of T.
  explicit RefCountedOwner(Family* family)
      : family_(family),
        attached_(false) {}

  ~RefCountedOwner() {
    if (attached_) {
      --family_->ref_count_;
      if (family_->ref_count_ == 0) {
        delete family_->ptr_;
        family_->ptr_ = NULL;
      }
    }
  }

  // If an another member of the family has already created the managed
  // object, Attach() will return true and attach 'this' to it, making the
  // object accessible via get() and pointer operations.
  //
  // Otherwise, it returns false, and you should call Initialize() to
  // set the object.
  bool Attach() {
    if (attached_) {
      return true;  // we are already attached, no need to initialize.
    } else if (family_->ref_count_ != 0) {
      // Someone already made an instance
      attached_ = true;
      ++family_->ref_count_;
      return true;
    } else {
      // If need to create it.
      return false;
    }
  }

  // Sets the value of the object our family will share. Pre-condition:
  // one must not have been set already (in other words, this must only be
  // called if Attach() returned false).
  void Initialize(T* value) {
    CHECK(!attached_ && family_->ref_count_ == 0);
    attached_ = true;
    family_->ref_count_ = 1;
    family_->ptr_ = value;
  }

  // Note that you must call Attach() (and Initialize() if it
  // returned false) before using these.
  T* Get() {
    DCHECK(attached_);
    return family_->ptr_;
  }

  const T* Get() const {
    DCHECK(attached_);
    return family_->ptr_;
  }

 private:
  friend class Family;
  Family* family_;
  bool attached_;  // whether we've grabbed a reference to the data.
  DISALLOW_COPY_AND_ASSIGN(RefCountedOwner);
};

#endif  // NET_INSTAWEB_UTIL_PUBLIC_REF_COUNTED_OWNER_H_
