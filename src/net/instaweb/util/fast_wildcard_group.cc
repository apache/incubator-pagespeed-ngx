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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include <limits.h>
#include <algorithm>
#include <cstddef>
#include <vector>
#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fast_wildcard_group.h"
#include "net/instaweb/util/public/rolling_hash.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {

namespace {
// Don't generate a hash unless there are this many
// non-wildcard-only patterns.
const int kMinPatterns = 10;

// Special index value for unused hash table entry.
const int kNoEntry = -1;

StringPiece LongestLiteralStringInWildcard(const Wildcard* wildcard) {
  StringPiece spec = wildcard->spec();
  const char kWildcardChars[] = { Wildcard::kMatchAny, Wildcard::kMatchOne };
  StringPiece wildcardChars(kWildcardChars, arraysize(kWildcardChars));
  size_t longest_pos = 0;
  size_t longest_len = 0;
  size_t next_wildcard = 0;
  for (int pos = 0; pos < spec.size(); pos = next_wildcard + 1) {
    next_wildcard = spec.find_first_of(wildcardChars, pos);
    if (next_wildcard == StringPiece::npos) {
      next_wildcard = spec.size();
    }
    size_t len = next_wildcard - pos;
    if (len > longest_len) {
      longest_pos = pos;
      longest_len = len;
    }
  }
  return spec.substr(longest_pos, longest_len);
}

}  // namespace

FastWildcardGroup::~FastWildcardGroup() {
  Clear();
}

void FastWildcardGroup::Uncompile() {
  if (rolling_hash_length_ == kUncompiled) {
    return;
  }
  rolling_hashes_.clear();
  effective_indices_.clear();
  wildcard_only_indices_.clear();
  pattern_hash_index_.clear();
}

void FastWildcardGroup::Clear() {
  Uncompile();
  STLDeleteElements(&wildcards_);
  allow_.clear();
}

inline int& FastWildcardGroup::pattern_hash_index(uint64 rolling_hash) const {
  // We exploit the fact that pattern_hash_index.size() is a power of two
  // to do integer modulus by bit masking.
  return pattern_hash_index_[rolling_hash & (pattern_hash_index_.size() - 1)];
}

void FastWildcardGroup::CompileNonTrivial() const {
  // First, assemble longest literal strings of each pattern
  std::vector<StringPiece> longest_literal_strings;
  int num_nontrivial_patterns = 0;
  rolling_hash_length_ = INT_MAX;
  for (int i = 0; i < wildcards_.size(); ++i) {
    longest_literal_strings.push_back(
        LongestLiteralStringInWildcard(wildcards_[i]));
    CHECK_EQ(i + 1, longest_literal_strings.size());
    int length = longest_literal_strings[i].size();
    if (length > 0) {
      ++num_nontrivial_patterns;
      rolling_hash_length_ = std::min(rolling_hash_length_, length);
    }
  }
  if (num_nontrivial_patterns < kMinPatterns) {
    // Not enough non-trivial patterns.  Don't compile.
    rolling_hash_length_ = kDontHash;
    return;
  }
  CHECK_LT(0, rolling_hash_length_);
  CHECK_GT(INT_MAX, rolling_hash_length_);
  // Allocate a hash table that's power-of-2 sized and >=
  // 2*num_nontrivial_patterns.
  int hash_index_size;
  for (hash_index_size = 8;
       hash_index_size < 2 * num_nontrivial_patterns;
       hash_index_size *= 2) { }
  pattern_hash_index_.resize(hash_index_size, kNoEntry);
  rolling_hashes_.resize(wildcards_.size());
  effective_indices_.resize(allow_.size());
  int current_effective_index = allow_.size() - 1;
  int current_allow = allow_[current_effective_index];
  // Fill in the hash table with a rolling hash.  We do this in
  // reverse order so that collisions will result in the later
  // pattern being matched first (if that succeeds, no further
  // matching will be required).
  for (int i = longest_literal_strings.size() - 1; i >= 0; --i) {
    const StringPiece literal(longest_literal_strings[i]);
    if (allow_[i] != current_allow) {
      // Change from allow to deny or vice versa;
      // change the current effective index and allow state.
      current_effective_index = i;
      current_allow = allow_[i];
    }
    effective_indices_[i] = current_effective_index;
    CHECK_LE(i, current_effective_index);
    CHECK_EQ(allow_[i], current_allow);
    CHECK_EQ(current_allow, allow_[effective_indices_[i]]);
    if (literal.size() == 0) {
      // All-wildcard pattern.
      wildcard_only_indices_.push_back(i);
      rolling_hashes_[i] = 0;
    } else {
      CHECK_GE(literal.size(), rolling_hash_length_);
      // If possible, find a non-colliding rolling hash taken from literal.  If
      // the first hash collides, using a different hash is OK; we'll still end
      // up checking both matches in the table for an input that matches both.
      // The goal is to avoid chaining by spreading the entries out across the
      // table.
      // TODO(jmaessen): Consider re-hashing the current entry as well on
      // collision to favor collision-free hash tables.
      int max_start = literal.size() - rolling_hash_length_;
      int start = 0;
      uint64 rolling_hash =
          RollingHash(literal.data(), start, rolling_hash_length_);
      for (start = 1;
           start <= max_start && pattern_hash_index(rolling_hash) != kNoEntry;
           ++start) {
        rolling_hash = NextRollingHash(literal.data(), start,
                                       rolling_hash_length_, rolling_hash);
      }
      // Now insert the entry, dealing with any collisions.
      rolling_hashes_[i] = rolling_hash;
      while (pattern_hash_index(rolling_hash) != kNoEntry) {
        ++rolling_hash;
      }
      pattern_hash_index(rolling_hash) = i;
    }
  }
}

void FastWildcardGroup::Compile() const {
  // Basic invariant
  CHECK_EQ(wildcards_.size(), allow_.size());
  // Make sure we don't have cruft left around from a previous compile.
  CHECK_EQ(0, rolling_hashes_.size());
  CHECK_EQ(0, effective_indices_.size());
  CHECK_EQ(0, wildcard_only_indices_.size());
  CHECK_EQ(0, pattern_hash_index_.size());
  CHECK_EQ(kUncompiled, rolling_hash_length_);

  // Fast path for small groups
  if (wildcards_.size() < kMinPatterns) {
    rolling_hash_length_ = kDontHash;
  } else {
    CompileNonTrivial();
  }

  // When we're done, things should be in a sensible state.
  CHECK_NE(kUncompiled, rolling_hash_length_);
  if (rolling_hash_length_ == kDontHash) {
    CHECK_EQ(0, rolling_hashes_.size());
    CHECK_EQ(0, effective_indices_.size());
    CHECK_EQ(0, wildcard_only_indices_.size());
    CHECK_EQ(0, pattern_hash_index_.size());
  } else {
    CHECK_EQ(wildcards_.size(), rolling_hashes_.size());
    CHECK_EQ(wildcards_.size(), effective_indices_.size());
    size_t hash_pats = wildcards_.size() - wildcard_only_indices_.size();
    CHECK_LE(kMinPatterns, hash_pats);
    CHECK_LE(2 * hash_pats, pattern_hash_index_.size());
    CHECK_LT(0, rolling_hash_length_);
  }
}

void FastWildcardGroup::Allow(const StringPiece& expr) {
  Uncompile();
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(true);
}

void FastWildcardGroup::Disallow(const StringPiece& expr) {
  Uncompile();
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(false);
}

bool FastWildcardGroup::Match(const StringPiece& str, bool allow) const {
  if (rolling_hash_length_ == kUncompiled) {
    Compile();
  }
  if (rolling_hash_length_ == kDontHash) {
    // Set of wildcards is small.
    // Just match against each pattern in reverse order (starting with most
    // recent, which overrides less recent), returning when a match succeeds.
    for (int i = wildcards_.size() - 1; i >= 0; --i) {
      if (wildcards_[i]->Match(str)) {
        return allow_[i];
      }
    }
    return allow;
  }
  int max_effective_index = kNoEntry;
  // Start by matching against all-wildcard patterns in reverse order,
  // stopping if a match is found (since earlier matches will have
  // a smaller index and be overridden by the already-found match).
  // TODO(jmaessen): These patterns all devolve to
  // a string length check (== or >=).  Consider optimizing them.
  for (int i = wildcard_only_indices_.size() - 1; i >= 0; --i) {
    int index = wildcard_only_indices_[i];
    if (wildcards_[index]->Match(str)) {
      max_effective_index = effective_indices_[index];
      break;
    }
  }
  int rolling_end = str.size() - rolling_hash_length_;
  if (rolling_end >= 0) {
    // Do a Rabin-Karp rolling match through the string.
    uint64 rolling_hash = RollingHash(str.data(), 0, rolling_hash_length_);
    int exit_effective_index = wildcards_.size() - 1;
      // Uses signed arithmetic for correct comparison below.
    for (int ofs = 0;
         max_effective_index < exit_effective_index && ofs <= rolling_end; ) {
      // Look up rolling_hash in table, stopping if we find a:
      //   1) Smaller index than max_effective_index
      //   2) Matching string (update max_effective_index).
      // In either case, all subsequent hash matches will be overridden by
      // max_effective_index, so we need not search the bucket anymore.  This is
      // guaranteed by the order of table insertion (largest index first)
      // and the fact that later smaller colliding entries are further along
      // in the linear probe.  This is true even if intervening slots are
      // occupied by entries with completely different hash values - those
      // entries will still have larger indices as they were inserted earlier.
      for (uint64 probe = 0; ; ++probe) {
        // The following invariant (and thus loop termination) should be
        // guaranteed by the sparseness of pattern_hash_index_.
        DCHECK_GT(pattern_hash_index_.size(), probe);
        int index = pattern_hash_index(rolling_hash + probe);
        if (index <= max_effective_index) {
          // This also includes the kNoEntry case.
          break;
        }
        if (rolling_hash == rolling_hashes_[index] &&
            wildcards_[index]->Match(str)) {
          max_effective_index = effective_indices_[index];
          break;
        }
      }
      if (++ofs <= rolling_end) {
        rolling_hash = NextRollingHash(str.data(), ofs, rolling_hash_length_,
                                       rolling_hash);
      }
    }
  }
  if (max_effective_index == kNoEntry) {
    return allow;
  } else {
    return allow_[max_effective_index];
  }
}

void FastWildcardGroup::CopyFrom(const FastWildcardGroup& src) {
  Clear();
  AppendFrom(src);
}

void FastWildcardGroup::AppendFrom(const FastWildcardGroup& src) {
  CHECK_EQ(src.wildcards_.size(), src.allow_.size());
  for (int i = 0, n = src.wildcards_.size(); i < n; ++i) {
    wildcards_.push_back(src.wildcards_[i]->Duplicate());
    allow_.push_back(src.allow_[i]);
  }
}

GoogleString FastWildcardGroup::Signature() const {
  GoogleString signature;
  for (int i = 0, n = wildcards_.size(); i < n; ++i) {
    StrAppend(&signature, wildcards_[i]->spec(), (allow_[i] ? "A" : "D"), ",");
  }
  return signature;
}

}  // namespace net_instaweb
