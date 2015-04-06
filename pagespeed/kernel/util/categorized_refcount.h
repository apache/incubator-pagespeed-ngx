/*
 * Copyright 2013 Google Inc.
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

#ifndef PAGESPEED_KERNEL_UTIL_CATEGORIZED_REFCOUNT_H_
#define PAGESPEED_KERNEL_UTIL_CATEGORIZED_REFCOUNT_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

/* This class helps manage a reference count stored in an object where
 * references can be classified into separate types, to further check their use
 * and help in debugging. You would normally store an instance of
 * CategorizedRefcount in the object being managed.
 *
 * There are the following requirements on ObjectType:
 *   void LastRefRemoved();  // called when refcount goes to 0, with
 *                           // mutex_ held.
 *   StringPiece RefCategoryName(EnumType);
 *   const int kNumRefCategories which bounds the EnumType
 *
 * For example, you might have something like this (omitting RefCategoryName
 * implementation, which is only needed for DebugString()):
 *
 * class AsyncDoerOfThings() {
 *  public:
 *   AsyncDoerOfThings() : ref_counts_(this) {
 *     ...
 *     ref_counts_.set_mutex(mutex_.get());
 *   }
 *
 *   void ref() { ref_counts_.AddRef(kRefExternal); }
 *   void deref() { ref_counts_.ReleaseRef(kRefExternal); }
 *   void AsyncOp() {
 *     ref_counts_.AddRef(kRefInternal);
 *     DoSomeRpcOp(this, &AsyncDoerOfThings::AsyncOpComplete);
 *   }
 *
 *  private:
 *   void AsyncOpComplete() { ref_counts_.ReleaseRef(kRefInternal); }
 *   void LastRefRemoved() { delete this; }
 *
 *   enum RefCategory {
 *     kRefExternal,
 *     kRefInternal,
 *     kNumRefCategories
 *   };
 *   friend class CategorizedRefcount<AsyncDoerOfThings, RefCategory>;
 *   CategorizedRefcount<AsyncDoerOfThings, RefCategory> ref_counts_;
 *
 * };
 *
 * TODO(morlovich): Consider having a cap per kind, too? Some are meant to be
 * 0-1 only.
 */
template<typename ObjectType, typename EnumType>
class CategorizedRefcount {
 public:
  // Note: set_mutex must be called before calling any other method on this
  // class.
  // TODO(jud): Instead of holding the mutex in this class, pass in the mutex to
  // each function so that thread safety annotation can be used.
  explicit CategorizedRefcount(ObjectType* object)
      : total_refcount_(0), object_(object), mutex_(NULL) {
    for (int i = 0; i < ObjectType::kNumRefCategories; ++i) {
      ref_counts_[i] = 0;
    }
  }

  // Sets the mutex that should be held when manipulating reference count
  // of this object. Does not take ownership.
  void set_mutex(AbstractMutex* mutex) {
    mutex_ = mutex;
  }

  void AddRef(EnumType category) {
    ScopedMutex hold(mutex_);
    AddRefMutexHeld(category);
  }

  void AddRefMutexHeld(EnumType category) {
    mutex_->DCheckLocked();
    DCHECK_LE(0, category);
    DCHECK_LT(category, ObjectType::kNumRefCategories);
    ++ref_counts_[category];
    ++total_refcount_;
  }

  void ReleaseRef(EnumType category) {
    ScopedMutex hold(mutex_);
    ReleaseRefMutexHeld(category);
  }

  void ReleaseRefMutexHeld(EnumType category) {
    mutex_->DCheckLocked();
    DCHECK_LE(0, category);
    DCHECK_LT(category, ObjectType::kNumRefCategories);
    --ref_counts_[category];
    DCHECK_GE(ref_counts_[category], 0) << category;
    --total_refcount_;
    if (total_refcount_ == 0) {
      object_->LastRefRemoved();
    }
  }

  // QueryCount w/o mutex held externally makes no sense, since there
  // would be no way of using the data.

  int QueryCountMutexHeld(EnumType category) const {
    return ref_counts_[category];
  }

  GoogleString DebugString() const {
    ScopedMutex hold(mutex_);
    return DebugStringMutexHeld();
  }

  GoogleString DebugStringMutexHeld() const {
    mutex_->DCheckLocked();

    GoogleString out;
    for (int i = 0; i < ObjectType::kNumRefCategories; ++i) {
      StrAppend(&out, "\t", object_->RefCategoryName(static_cast<EnumType>(i)),
                ": ", IntegerToString(ref_counts_[i]));
    }
    return out;
  }

  void DCheckAllCountsZero() {
    ScopedMutex hold(mutex_);
    DCheckAllCountsZeroMutexHeld();
  }

  void DCheckAllCountsZeroMutexHeld() {
    mutex_->DCheckLocked();
    DCHECK_EQ(0, total_refcount_);
    for (int i = 0; i < ObjectType::kNumRefCategories; ++i) {
      DCHECK_EQ(0, ref_counts_[i]);
    }
  }

 private:
  int ref_counts_[ObjectType::kNumRefCategories];
  int total_refcount_;
  ObjectType* object_;
  AbstractMutex* mutex_;
  DISALLOW_COPY_AND_ASSIGN(CategorizedRefcount);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_CATEGORIZED_REFCOUNT_H_
