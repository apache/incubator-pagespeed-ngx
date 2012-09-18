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
//
// Implements a ref-counted string class, with full sharing.  This
// class does *not* implement copy-on-write semantics, however, it
// does support a unique() method for determining, prior to writing,
// whether other references exist.  Thus it is feasible to implement
// copy-on-write as a layer over this class.

#include "net/instaweb/util/public/shared_string.h"

#include <algorithm>  // for std::min, std::max

#include "base/logging.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

SharedString::SharedString() : skip_(0), size_(0) {
}

SharedString::SharedString(const StringPiece& str)
    : skip_(0),
      size_(str.size()) {
  GoogleString* storage = ref_string_.get();
  str.CopyToString(storage);
}

// When constructing with a GoogleString, going through the StringPiece
// ctor above causes an extra copy compared with string implementations that
// use copy-on-write.  So we make an explicit GoogleString constructor.
SharedString::SharedString(const GoogleString& str)
    : ref_string_(str),
      skip_(0),
      size_(str.size()) {
}

// Given the two constructors above, it is ambiguous which one gets
// called when passed a string-literal, so making an explicit const char*
// constructor eliminates the ambiguity.  This is likely beneficial mostly
// for tests.
SharedString::SharedString(const char* str)
    : skip_(0) {
  GoogleString* storage = ref_string_.get();
  *storage = str;
  size_ = storage->size();
}

SharedString::SharedString(const SharedString& src)
    : ref_string_(src.ref_string_),
      skip_(src.skip_),
      size_(src.size_) {
}

SharedString& SharedString::operator=(const SharedString& src) {
  if (&src != this) {
    ref_string_ = src.ref_string_;
    skip_ = src.skip_;
    size_ = src.size_;
  }
  return *this;
}

StringPiece SharedString::Value() const {
  const GoogleString* storage = ref_string_.get();
  DCHECK_LE(size_ + skip_, static_cast<int>(storage->size()));
  return StringPiece(storage->data() + skip_, size_);
}

void SharedString::Assign(const char* data, int size) {
  // Note that 'str' might be a substring of the current storage, so
  // avoid bugs by copying to a temp and swapping.
  GoogleString temp(data, size);
  ClearIfShared();
  GoogleString* storage = ref_string_.get();
  temp.swap(*storage);
  size_ = storage->size();
}

void SharedString::UniquifyIfTruncated() {
  if (size_ != (static_cast<int>(ref_string_->size()) - skip_)) {
    if (unique()) {
      ref_string_->resize(size_ + skip_);
    } else {
      *this = SharedString(Value());
    }
  }
}

void SharedString::Append(const char* new_data, size_t new_size) {
  DCHECK((new_data + new_size) <= data() ||
         (data() + size() < new_data))
      << "Append must be given non-overlapping strings";
  UniquifyIfTruncated();
  ref_string_->append(new_data, new_size);
  size_ += new_size;
}

void SharedString::Extend(int new_size) {
  if (size_ < new_size) {
    UniquifyIfTruncated();
    size_ = new_size;
    ref_string_.get()->resize(size_ + skip_);
  }
}

void SharedString::WriteAt(int dest_offset, const char* source, int count) {
  DCHECK_LT(dest_offset, size());
  DCHECK_LE(dest_offset + count, size());
  if (count > size() - dest_offset) {
    count = std::max(0, size() - dest_offset);
  }
  memcpy(mutable_data() + dest_offset, source, count);
}

void SharedString::SwapWithString(GoogleString* str) {
  ClearIfShared();
  GoogleString* storage = ref_string_.get();
  storage->swap(*str);
  skip_ = 0;
  size_ = storage->size();
}

void SharedString::DetachAndClear() {
  SharedString empty_string;
  *this = empty_string;  // Detaches other strings sharing this value.
}

void SharedString::RemovePrefix(int n) {
  DCHECK_LE(n, size_);
  if (n > size_) {
    n = size_;
  }
  skip_ += n;
  size_ -= n;
}

// Removes the last n characters from the string.  Other linked
// SharedStrings remain linked, but are unaffected by this removal
// because each has their own skip_ and size_.
void SharedString::RemoveSuffix(int n) {
  DCHECK_LE(n, size_);
  size_ -= std::min(n, size_);
}

}  // namespace net_instaweb
