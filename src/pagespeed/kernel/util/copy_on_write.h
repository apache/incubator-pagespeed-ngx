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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_UTIL_COPY_ON_WRITE_H_
#define PAGESPEED_KERNEL_UTIL_COPY_ON_WRITE_H_

#include "pagespeed/kernel/base/ref_counted_ptr.h"

namespace net_instaweb {

// Implements a copy-on-write container.  This is intended to be used
// to facilitate sharing of expensive-to-copy objects when most of the
// time we don't need to modify the copies.
//
// T must be copyable and assignable.  It does not need to be derived
// from any other class.  It also must have an empty constructor and,
// if you want to use MergeOrShare, a Merge method.
template<class T>
class CopyOnWrite {
 public:
  // Usage of default constructor requires that T also have a default
  // constructor.
  CopyOnWrite() {}

  // Explicitly constructed CopyOnWrite pointers don't require T to
  // have a default constructor.
  explicit CopyOnWrite(const T& obj) : reference_(obj) {}
  ~CopyOnWrite() {}

  const T* get() const { return reference_.get(); }
  const T* operator->() const { return reference_.get(); }
  const T& operator*() const { return *reference_.get(); }

  // Gets a unique mutable version of the object.  This is not
  // inherently thread-safe: if you call this from one thread while adding
  // references from another thread, you will need to perform locking
  // at a higher level.
  T* MakeWriteable() {
    if (!reference_.unique()) {
      reference_.reset(*get());
    }
    return reference_.get();
  }

  CopyOnWrite& operator=(const CopyOnWrite& src) {
    if (&src != this) {
      reference_ = src.reference_;
    }
    return *this;
  }

  CopyOnWrite(const CopyOnWrite& src) : reference_(src.reference_) {}

  // Merges in the contents of src into this.  To increase speed and
  // save memory, this method shares storage with src if this was empty.
  void MergeOrShare(const CopyOnWrite& src) {
    // If src is empty then the function is a no-op.
    if (!src->empty()) {
      // If this is empty, then we want to make it share src's storage.
      if ((*this)->empty()) {
        *this = src;
      } else {
        MakeWriteable()->Merge(*src.get());
      }
    }
  }

 private:
  typedef RefCountedObj<T> Reference;

  Reference reference_;

  // Copying and assigning CopyOnWrite objects is supported.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_COPY_ON_WRITE_H_
