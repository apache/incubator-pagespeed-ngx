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
#include "base/basictypes.h"

#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/dense_hash_set.h"
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
    std::size_t operator()(const StringPiece& key) const {
      return HashString<CharTransform>(key.data(), key.length());
    }
  };

  typedef dense_hash_set<StringPiece, Hash, Comparator> SymbolSet;
  SymbolSet string_set_;

  DISALLOW_COPY_AND_ASSIGN(SymbolTable);
};

typedef SymbolTable<CaseFold> SymbolTableInsensitive;
typedef SymbolTable<CasePreserve> SymbolTableSensitive;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_
