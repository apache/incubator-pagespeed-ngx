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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_

#include <stdlib.h>
#include <set>
#include "net/instaweb/util/public/atom.h"
#include <string>
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
template<class SymbolCompare> class SymbolTable {
 public:
  ~SymbolTable() {
    while (!string_set_.empty()) {
      // Note: This should perform OK for rb-trees, but will perform
      // poorly if a hash-table is used.
      typename SymbolSet::const_iterator p = string_set_.begin();
      const char* str = *p;
      string_set_.erase(p);
      free(const_cast<char*>(str));
    }
  }

  Atom Intern(const char* src) {
    Atom atom(src);
    typename SymbolSet::const_iterator iter = string_set_.find(src);
    if (iter == string_set_.end()) {
      char* str = strdup(src);
      string_set_.insert(str);
      return Atom(str);
    }
    return Atom(*iter);
  }

  inline Atom Intern(const std::string& src) {
    return Intern(src.c_str());
  }

 private:
  typedef std::set<const char*, SymbolCompare> SymbolSet;
  SymbolSet string_set_;
};

class SymbolTableInsensitive : public SymbolTable<CharStarCompareInsensitive> {
};
class SymbolTableSensitive : public SymbolTable<CharStarCompareSensitive> {
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SYMBOL_TABLE_H_
