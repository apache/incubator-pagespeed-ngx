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

// Author: jmarantz@google.com (Joshua Marantz),
//         morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/util/public/symbol_table.h"

namespace net_instaweb {

template<class CharTransform>
SymbolTable<CharTransform>::SymbolTable() {
}

template<class CharTransform>
SymbolTable<CharTransform>::~SymbolTable() {
#if SYMBOL_TABLE_USE_HASH_TABLE
  // If we  were using dense_hash_map we could just iterate through
  // the table and delete the elements.  But this is no good for
  // g++ hash tables.

  //  // Note: this is safe because we don't need the actual contents to test
  //  // the data vs. empty keys, which is all we need in ~dense_hash_set
  //  for (typename SymbolSet::iterator p = string_set_.begin();
  //        p != string_set_.end(); ++p) {
  //    std::free(const_cast<char*>(p->data()));
  //  }

  CharStarVector v(string_set_.size());
  for (typename SymbolSet::iterator p = string_set_.begin();
       p != string_set_.end(); ++p) {
    v.push_back(p->data());
  }
  string_set_.clear();
  for (int i = 0, n = v.size(); i < n; ++i) {
    std::free(const_cast<char*>(v[i]));
  }
#else
  while (!string_set_.empty()) {
    typename SymbolSet::iterator p = string_set_.begin();
    char* str = const_cast<char*>(p->data());
    string_set_.erase(p);
    std::free(str);
  }
#endif
}

template<class CharTransform>
Atom SymbolTable<CharTransform>::Intern(const StringPiece& src) {
  if (src.empty()) {
    return Atom();
  }

  typename SymbolSet::const_iterator iter = string_set_.find(src);
  if (iter == string_set_.end()) {
    char* str = strdup(src.data());
    string_set_.insert(StringPiece(str));
    return Atom(str);
  }
  return Atom(iter->data());
}

// We explicitly instantiate since we want ::Intern to be out-of-line
template class SymbolTable<CaseFold>;
template class SymbolTable<CasePreserve>;

}  // namespace net_instaweb
