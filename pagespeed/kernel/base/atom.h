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

#ifndef PAGESPEED_KERNEL_BASE_ATOM_H_
#define PAGESPEED_KERNEL_BASE_ATOM_H_

#include <set>
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct CaseFold;
struct CasePreserve;
template<class CharTransform> class SymbolTable;

// Atoms are idempotent representations of strings, created
// via a symbol table.
class Atom {
 public:
  Atom(const Atom& src) : str_(src.str_) {}
  Atom();
  ~Atom() {}  // atoms are memory-managed by SymbolTables.

  Atom& operator=(const Atom& src) {
    if (&src != this) {
      str_ = src.str_;
    }
    return *this;
  }

  // Returns the address of the canonical StringPiece representing this Atom.
  // The underlying StringPiece object (and its data) are owned by the
  // SymbolTable.
  const StringPiece* Rep() const { return str_; }

  // This is comparing the underlying StringPiece pointers.  It is invalid
  // to compare Atoms from different symbol tables.
  bool operator==(const Atom& sym) const {
    return str_ == sym.str_;
  }

  // This is comparing the underlying StringPiece pointers.  It is invalid
  // to compare Atoms from different symbol tables.
  bool operator!=(const Atom& sym) const {
    return str_ != sym.str_;
  }

  // SymbolTable is a friend of Symbol because SymbolTable is the
  // only class that has the right to construct a new Atom from
  // a StringPiece*.
  friend class SymbolTable<CaseFold>;
  friend class SymbolTable<CasePreserve>;

 private:
  explicit Atom(const StringPiece* str) : str_(str) {}
  const StringPiece* str_;
};

// Once interned, Atoms are very cheap to put in a set, using
// pointer-comparison.
struct AtomCompare {
  bool operator()(const Atom& a1, const Atom& a2) const {
    // Compares pointers. Note that this assumes we don't overlap the
    // StringPiece's, which is the case for the implementation of SymbolTable.
    return a1.Rep()->data() < a2.Rep()->data();
  }
};

// A set of atoms can be constructed very efficiently.  Note that
// iteration over this set will *not* be in alphabetical order.
typedef std::set<Atom, AtomCompare> AtomSet;

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ATOM_H_
