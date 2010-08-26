/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_STRING_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_STRING_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

#include "base/ref_counted.h"

namespace net_instaweb {


class RefCountedString : public base::RefCountedThreadSafe<RefCountedString> {
 public:
  RefCountedString() { }
  explicit RefCountedString(const StringPiece& str)
      : string_(str.data(), str.size()) {
  }
  const std::string& value() const { return string_; }
  std::string& value() { return string_; }
  size_t size() const { return string_.size(); }
  const char* data() const { return string_.data(); }

 private:
  friend class base::RefCountedThreadSafe<RefCountedString>;
  ~RefCountedString() { }

  std::string string_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedString);
};

class SharedString : public scoped_refptr<RefCountedString> {
 public:
  SharedString() : scoped_refptr<RefCountedString>(new RefCountedString) {
  }

  explicit SharedString(const StringPiece& str)
      : scoped_refptr<RefCountedString>(new RefCountedString(str)) {
  }
  std::string& operator*() { return ptr_->value(); }
  std::string* get() { return &(ptr_->value()); }
  const std::string* get() const { return &(ptr_->value()); }
  std::string* operator->() { return &(ptr_->value()); }
  const std::string* operator->() const { return &(ptr_->value()); }
  bool unique() const { return ptr_->HasOneRef(); }
  std::string::size_type size() const { return ptr_->value().size(); }
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_STRING_H_
