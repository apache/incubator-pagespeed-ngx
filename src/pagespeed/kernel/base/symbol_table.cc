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

#include "pagespeed/kernel/base/symbol_table.h"

#include <cstddef>
#include <cstdlib>
#include <vector>
#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/atom.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace {

// Strategically select a chunk size that will allow for some fixed
// overhead imposed by some versions of malloc.  If we have a
// zero-overhead malloc like tcmalloc there's no big deal in missing
// out on 16 bytes on a chunk this big.
const size_t kChunkSize = 32768 - 16;

}  // namespace

namespace net_instaweb {

template<class CharTransform>
SymbolTable<CharTransform>::SymbolTable()
    : next_ptr_(NULL),
      string_bytes_allocated_(0) {
  // We can use an empty string piece as the empty value, since
  // ::Intern has a quick exit on empty inputs.
  string_map_.set_empty_key(StringPiece());
}

template<class CharTransform>
void SymbolTable<CharTransform>::Clear() {
  string_map_.clear();
  for (int i = 0, n = storage_.size(); i < n; ++i) {
    std::free(storage_[i]);
  }
  storage_.clear();
  pieces_.clear();
  next_ptr_ = NULL;
  string_bytes_allocated_ = 0;
}

template<class CharTransform>
void SymbolTable<CharTransform>::NewStorage() {
  next_ptr_ = static_cast<char*>(std::malloc(kChunkSize));
  storage_.push_back(next_ptr_);
}

template<class CharTransform>
Atom SymbolTable<CharTransform>::Intern(const StringPiece& src) {
  if (src.empty()) {
    return Atom();
  }

  typename SymbolMap::const_iterator iter = string_map_.find(src);
  if (iter == string_map_.end()) {
    // Lazy-initialize to ensure at least one available block.
    if (storage_.empty()) {
      NewStorage();
    }

    size_t bytes_required = src.size();
    char* new_symbol_storage = NULL;
    if (bytes_required > kChunkSize / 4) {
      // The string we are trying to put into the symbol table is sufficiently
      // large that it might waste a lot of our chunked storage, so just
      // allocate it directly.
      new_symbol_storage = static_cast<char*>(std::malloc(bytes_required));

      // Insert this large chunk into the second-to-last position in the
      // storage array so that we can keep using the last normal chunk.
      int last_pos = storage_.size() - 1;
      storage_.push_back(storage_[last_pos]);
      storage_[last_pos] = new_symbol_storage;
    } else {
      DCHECK_GE(next_ptr_, storage_.back());
      size_t bytes_used = next_ptr_ - storage_.back();
      size_t bytes_remaining = kChunkSize - bytes_used;
      if (bytes_remaining < bytes_required) {
        NewStorage();
      }
      new_symbol_storage = next_ptr_;
      next_ptr_ += bytes_required;
    }
    memcpy(new_symbol_storage, src.data(), src.size());

    StringPiece new_sp(new_symbol_storage, src.size());
    pieces_.push_back(new_sp);
    StringPiece* canonical_sp = &pieces_.back();
    string_map_.insert(make_pair(new_sp, canonical_sp));
    string_bytes_allocated_ += bytes_required;
    return Atom(canonical_sp);
  }
  return Atom(iter->second);
}

// We explicitly instantiate since we want ::Intern to be out-of-line
template class SymbolTable<CaseFold>;
template class SymbolTable<CasePreserve>;

}  // namespace net_instaweb
