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

// Author: jmarantz@google.com (Joshua Marantz),
//         morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_

#include <cstdlib>
#include <cstring>

#ifdef __GNUC__
#define SYMBOL_TABLE_USE_HASH_TABLE 1
#else
#define SYMBOL_TABLE_USE_HASH_TABLE 0
#endif

#if SYMBOL_TABLE_USE_HASH_TABLE

// This revolting mess is due to the 'deprecated' warnings which at
// least some versions of gcc have attached to hash_set, despite the
// lack of general availability of 'unordered_set' suggested in the
// warning.  I am not able to figure out how to apply
// '-Wno-deprecated', in .gyp files, and am not sure I really want to
// since it would make *all* deprecation issues instead of just this
// one.  And of course since we promote warnings to errors, we cannot
// compile without this.  Nor am I able to figure out how to enable
// Page Speed to compile its instaweb dependency on dense_hash_map, so
// that's out too.
//
// TODO(jmarantz): Remove all this and go back to dense_hash_map pending
// Bryan authorizing us to grow the core library, and his gyp skills in
// helping us successfully link it in Page Speed.

#ifndef _BACKWARD_BACKWARD_WARNING_H
#define NEED_UNDEF
#define _BACKWARD_BACKWARD_WARNING_H
#endif
#include <ext/hash_set>
#ifdef NEED_UNDEF
#undef NEED_UNDEF
#undef _BACKWARD_BACKWARD_WARNING_H
#endif

#else
#include <set>
#endif

#include "base/basictypes.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Implements a generic symbol table, allowing for case-sensitive
// and case insensitive versions.  The elements of SymbolTables are
// Atoms.  Atoms are created by Interning strings.
//
// Atoms are cheap and are passed around by value, not by reference or
// pointer.  Atoms can be compared to one another via ==.  A const char*
// can be extracted from an Atom via ==.
//
// Atoms are memory-managed by the symbol table from which they came.
// When the symbol table is destroyed, so are all the Atoms that
// were interned in it.
//
// Care should be taken not to attempt to compare Atoms created from
// multiple symbol tables.
//
// TODO(jmarantz): Symbol tables are not currently thread-safe.  We
// should consider whether it's worth making them thread-safe, or
// whether it's better to use separate symbol tables in each thread.
template<class CharTransform> class SymbolTable {
 public:
  SymbolTable();
  ~SymbolTable();

  Atom Intern(const StringPiece& src);

 private:
#if SYMBOL_TABLE_USE_HASH_TABLE
  // StringPiece equality aware of CharTransform
  struct Comparator {
    bool operator()(const StringPiece& key_a, const StringPiece& key_b) const {
      if (key_a.length() == key_b.length()) {
        const char* a = key_a.data();
        const char* b = key_b.data();
        const char* a_end = a + key_a.length();
        while (a < a_end) {
          if (CharTransform::Normalize(*a) != CharTransform::Normalize(*b)) {
            return false;
          }
          ++a;
          ++b;
        }
        return true;
      } else {
        return false;
      }
    }
  };

  struct Hash {
    size_t operator()(const StringPiece& key) const {
      return HashString<CharTransform, size_t>(key.data(), key.length());
    }
  };

  typedef __gnu_cxx::hash_set<StringPiece, Hash, Comparator> SymbolSet;
#else
  struct Compare {
    bool operator()(const StringPiece& a, const StringPiece& b) const {
      return CharTransform::Compare(a, b);
    }
  };

  typedef std::set<StringPiece, Compare> SymbolSet;
#endif
  SymbolSet string_set_;

  DISALLOW_COPY_AND_ASSIGN(SymbolTable);
};

typedef SymbolTable<CaseFold> SymbolTableInsensitive;
typedef SymbolTable<CasePreserve> SymbolTableSensitive;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_
