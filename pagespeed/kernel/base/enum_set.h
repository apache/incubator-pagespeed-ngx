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

#ifndef PAGESPEED_KERNEL_BASE_ENUM_SET_H_
#define PAGESPEED_KERNEL_BASE_ENUM_SET_H_

#include <bitset>
#include <cstddef>

namespace net_instaweb {

// Represents a set of values -- implemented via a bitset.
template<typename EnumType, size_t NumEnums> class EnumSet {
 public:
  bool IsSet(EnumType value) const {
    return bits_.test(static_cast<size_t>(value));
  }

  // Inserts a new value, returning true if a change was made.
  bool Insert(EnumType value) {
    bool result = !IsSet(value);
    insert(value);
    return result;
  }

  // Inserts a value; no return value.
  //
  // TODO(jmarantz): change call-sites to Insert and remove this one.
  void insert(EnumType value) {
    bits_.set(static_cast<size_t>(value));
  }

  // Returns true if a change was made.
  bool Erase(EnumType value) {
    bool result = IsSet(value);
    bits_.reset(static_cast<size_t>(value));
    return result;
  }

  // Merges src into this, returning whether this resulted in a change.
  bool Merge(const EnumSet& src) {
    // We save the current version of the set in order to see whether
    // the merge resulted in a change.  Note that copying and comparing
    // the bits is very cheap; probably cheaper than calling count().
    EnumSet save(*this);
    bits_ |= src.bits_;
    return bits_ != save.bits_;
  }

  // Merges the entries *not* set in src into this, returning whether this
  // resulted in a change.
  bool MergeInverted(const EnumSet& src) {
    EnumSet save(*this);
    bits_ |= ~src.bits_;
    return bits_ != save.bits_;
  }

  void EraseSet(const EnumSet& src) {
    bits_ &= ~src.bits_;
  }

  // Sets all the entries to true.
  void SetAll() {
    bits_.set();
  }

  // Standard STL-like methods.
  void clear() { bits_.reset(); }
  size_t size() const { return bits_.count(); }
  bool empty() const { return bits_.none(); }

  // This overload is required for use in EXPECT_EQ in tests.
  bool operator==(const EnumSet& that) const {
    return bits_ == that.bits_;
  }

 private:
  typedef std::bitset<NumEnums> BitSet;
  BitSet bits_;

  // Implicit copy and assign will work perfectly and are required.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ENUM_SET_H_
