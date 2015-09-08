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
// Implements a ref-counted string class, with full sharing.  This
// class does *not* implement copy-on-write semantics, however, it
// does support a unique() method for determining, prior to writing,
// whether other references exist.  Thus it is feasible to implement
// copy-on-write as a layer over this class.

#ifndef PAGESPEED_KERNEL_BASE_SHARED_STRING_H_
#define PAGESPEED_KERNEL_BASE_SHARED_STRING_H_

#include <cstddef>                     // for size_t

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"

namespace net_instaweb {

// Reference-counted string.  This class allows for shared underlying
// storage with other SharedString instances, but for trimming a
// SharedString instance's view of it via RemoveSuffix() and RemovePrefix().
class SharedString {
 public:
  SharedString();

  explicit SharedString(const StringPiece& str);

  // When constructing with a GoogleString, going through the StringPiece
  // ctor above causes an extra copy compared with string implementations that
  // use copy-on-write.  So we make an explicit GoogleString constructor.
  explicit SharedString(const GoogleString& str);

  // Given the two constructors above, it is ambiguous which one gets
  // called when passed a string-literal, so making an explicit const char*
  // constructor eliminates the ambiguity.  This is likely beneficial mostly
  // for tests.
  explicit SharedString(const char* str);

  // Storage-sharing occurs as a result of the copy-constructor and
  // assignment operator.
  SharedString(const SharedString& src);
  SharedString& operator=(const SharedString& src);

  // Returns the value as a StringPiece, taking into account any calls
  // to RemovePrefix, RemoveSuffix, and any string-mutations due to
  // Append or WriteAt on this or any other SharedStrings sharing
  // storage due to the assignment operator or operator=.
  StringPiece Value() const;

  // Resets SharedString to be a copy of str, erasing any previous
  // prefix/suffix.  Calling this function detaches any connected
  // SharedStrings.
  //
  // It is valid to assign from a value inside the SharedString.  In
  // other words, shared_string.Assign(shared_string.Value().substr(...))
  // will work.
  void Assign(StringPiece str) { Assign(str.data(), str.size()); }
  void Assign(const char* data, int size);

  // Appends a new string to the underlying storage.  Other SharedStrings will
  // not be affected by this mutation.
  //
  // This function tries to avoid detaching from other SharedStrings, and only
  // needs to do so if this has been truncated.
  //
  // Unlike Assign, it is invalid to append characters managed by this
  // SharedString.  In other words, shared_string.Append(shared_string.Value())
  // will fail.
  //
  // Note: Append() is not thread-safe.  Concurrent accesses to any
  // SharedStrings with the same storage will fail.
  void Append(StringPiece str) { Append(str.data(), str.size()); }
  void Append(const char* data, size_t size);

  // Makes the string representation at least 'new_size' large,
  // without specifying how new bytes should be filled in.  Typically
  // this will be followed by a call to WriteAt().
  //
  // This function does *not* detach other SharedStrings -- the
  // underlying storage will still be shared.  Consequently this
  // function does not shrink strings, as that could invalidate
  // trimmed SharedStrings sharing the storage.
  //
  // If this method is called on a truncated SharedString, then it will be
  // detached prior to extending it.
  void Extend(int new_size);

  // Swaps storage with the the passed-in string, detaching from any other
  // previously-linked SharedStrings.
  void SwapWithString(GoogleString* str);

  // Clears the contents of the string, and erases any removed prefix
  // or suffix, detaching from any other previously-linked SharedStrings.
  void DetachAndClear();

  // Removes the first n characters from the string.  Other linked
  // SharedStrings remain linked, but are unaffected by this removal
  // because each has its own skip_ and size_.
  void RemovePrefix(int n);

  // Removes the last n characters from the string.  Other linked
  // SharedStrings remain linked, but are unaffected by this removal
  // because each has its own skip_ and size_.
  void RemoveSuffix(int n);

  // Computes the size, taking into account any removed prefix or suffix.
  int size() const { return size_; }
  bool empty() const { return size_ == 0; }
  const char* data() const { return ref_string_->data() + skip_; }

  // WriteAt allows mutation of the underlying string data.  The
  // string must already be sized as needed via previous Append() or
  // Extend() calls.  Mutations done via this method will affect all
  // references to the underlying storage.
  void WriteAt(int dest_offset, const char* source, int count);

  // Disassociates this SharedString with any others that have linked
  // the same storage.  Retains the same string value.
  void DetachRetainingContent() {
    if (!unique()) {
      *this = SharedString(Value());
      DCHECK(unique());
    }
  }

  // Determines whether this SharedString shares storage from other
  // SharedStrings.
  bool unique() const { return ref_string_.unique(); }

  // Determines whether RemovePrefix or RemoveSuffix has every been called
  // on this SharedString.  Note that other SharedStrings sharing the
  // same storage as this may be trimmed differently.
  bool trimmed() const {
    return size_ != static_cast<int>(ref_string_->size());
  }

  // Returns back a GoogleString* representation for the contained value.
  //
  // This is makes sense to call only if the string is not trimmed.  If
  // RemovePrefix or RemoveSuffix has been called on this SharedString, the
  // returned string may have extra characters in it.
  //
  // Note: we suggest against using this routine.  It is better to consume
  // the data via the StringPiece returned from Value().
  //
  // This routine is, however, useful to call from tests to determine
  // storage uniqueness.
  const GoogleString* StringValue() const {
    return ref_string_.get();
  }

  // Determines whether this and that share the same storage.
  bool SharesStorage(const SharedString& that) const {
    return ref_string_.get() == that.ref_string_.get();
  }

 private:
  void UniquifyIfTruncated();
  char* mutable_data() { return &(*ref_string_.get())[0] + skip_; }
  void ClearIfShared() {
    if (!unique()) {
      DetachAndClear();
    }
  }

  RefCountedObj<GoogleString> ref_string_;

  int skip_;  // Number of bytes to skip at the beginning of the string.
  int size_;  // Number of bytes visible in the current view.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_SHARED_STRING_H_
