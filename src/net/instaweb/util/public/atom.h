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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ATOM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ATOM_H_

#include <set>
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

struct CaseFold;
struct CasePreserve;
template<class CharTransform> class SymbolTable;

// Atoms are idempotent representations of strings, created
// via a symbol table.
class Atom {
 public:
  Atom(const Atom& src) : str_(src.str_) {}
  Atom() : str_("") {}
  ~Atom() {}  // atoms are memory-managed by SymbolTables.

  Atom& operator=(const Atom& src) {
    if (&src != this) {
      str_ = src.str_;
    }
    return *this;
  }

  // string-like accessors.
  const char* c_str() const { return str_; }
  int size() const { return strlen(str_); }

  // This is comparing the underlying char* pointers.  It is invalid
  // to compare Atoms from different symbol tables.
  bool operator==(const Atom& sym) const {
    return str_ == sym.str_;
  }

  // This is comparing the underlying char* pointers.  It is invalid
  // to compare Atoms from different symbol tables.
  bool operator!=(const Atom& sym) const {
    return str_ != sym.str_;
  }

  // SymbolTable is a friend of Symbol because SymbolTable is the
  // only class that has the right to construct a new Atom from
  // a char*.
  friend class SymbolTable<CaseFold>;
  friend class SymbolTable<CasePreserve>;

 private:
  explicit Atom(const char* str) : str_(str) {}
  const char* str_;
};

// Once interned, Atoms are very cheap to put in a set, using
// pointer-comparison.
struct AtomCompare {
  bool operator()(const Atom& a1, const Atom& a2) const {
    return a1.c_str() < a2.c_str();   // compares pointers
  }
};

// A set of atoms can be constructed very efficiently.  Note that
// iteration over this set will *not* be in alphabetical order.
typedef std::set<Atom, AtomCompare> AtomSet;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ATOM_H_
