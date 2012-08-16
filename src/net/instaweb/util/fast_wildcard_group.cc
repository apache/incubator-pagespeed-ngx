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

#include <algorithm>
#include <vector>
#include "base/logging.h"
#include "net/instaweb/util/public/atomic_int32.h"
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
const int kMinPatterns = 11;

// Maximum rolling hash window size
const int32 kMaxRollingHashWindow = 256;

// Special index value for unused hash table entry.
const int kNoEntry = -1;

StringPiece LongestLiteralStringInWildcard(const Wildcard* wildcard) {
  StringPiece spec = wildcard->spec();
  const char kWildcardChars[] = { Wildcard::kMatchAny, Wildcard::kMatchOne };
  StringPiece wildcardChars(kWildcardChars, arraysize(kWildcardChars));
  int longest_pos = 0;
  int longest_len = 0;
  int next_wildcard = 0;
  for (int pos = 0;
       pos < static_cast<int>(spec.size()); pos = next_wildcard + 1) {
    next_wildcard = spec.find_first_of(wildcardChars, pos);
    if (next_wildcard == static_cast<int>(StringPiece::npos)) {
      next_wildcard = spec.size();
    }
    int len = next_wildcard - pos;
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
  if (rolling_hash_length_.value() == kUncompiled) {
    return;
  }
  rolling_hash_length_.set_value(kUncompiled);
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
  int32 rolling_hash_length = kMaxRollingHashWindow;
  for (int i = 0; i < static_cast<int>(wildcards_.size()); ++i) {
    longest_literal_strings.push_back(
        LongestLiteralStringInWildcard(wildcards_[i]));
    DCHECK_EQ(i + 1, static_cast<int>(longest_literal_strings.size()));
    int length = longest_literal_strings[i].size();
    if (length > 0) {
      ++num_nontrivial_patterns;
      rolling_hash_length = std::min(rolling_hash_length, length);
    }
  }
  if (num_nontrivial_patterns < kMinPatterns) {
    // Not enough non-trivial patterns.
    DCHECK_EQ(kDontHash, rolling_hash_length_.value());
    return;
  }
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
  bool current_allow = allow_[current_effective_index];
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
    DCHECK_LE(i, current_effective_index);
    DCHECK_EQ(allow_[i], current_allow);
    DCHECK_EQ(current_allow, allow_[effective_indices_[i]]);
    if (literal.size() == 0) {
      // All-wildcard pattern.
      wildcard_only_indices_.push_back(i);
      rolling_hashes_[i] = 0;
    } else {
      DCHECK_GE(static_cast<int>(literal.size()), rolling_hash_length);
      // If possible, find a non-colliding rolling hash taken from literal.  If
      // the first hash collides, using a different hash is OK; we'll still end
      // up checking both matches in the table for an input that matches both.
      // The goal is to avoid chaining by spreading the entries out across the
      // table.
      // TODO(jmaessen): Consider re-hashing the current entry as well on
      // collision to favor collision-free hash tables.
      int max_start = literal.size() - rolling_hash_length;
      int start = 0;
      uint64 rolling_hash =
          RollingHash(literal.data(), start, rolling_hash_length);
      for (start = 1;
           start <= max_start && pattern_hash_index(rolling_hash) != kNoEntry;
           ++start) {
        rolling_hash = NextRollingHash(literal.data(), start,
                                       rolling_hash_length, rolling_hash);
      }
      // Now insert the entry, dealing with any collisions.
      rolling_hashes_[i] = rolling_hash;
      while (pattern_hash_index(rolling_hash) != kNoEntry) {
        ++rolling_hash;
      }
      pattern_hash_index(rolling_hash) = i;
    }
  }
  // Finally, after all the metadata is initialized, make rolling_hash_length
  // visible to the world.  This has release semantics, meaning that if another
  // thread reads rolling_hash_length_ (with acquire semantics) and gets the
  // value we set here, it is guaranteed to see all the preceding writes we did
  // to the other compilation metadata.
  rolling_hash_length_.set_value(rolling_hash_length);
}

void FastWildcardGroup::Compile() const {
  // Basic invariant
  CHECK_EQ(wildcards_.size(), allow_.size());
  // Make sure we don't have cruft left around from a previous compile.
  CHECK_EQ(0, static_cast<int>(rolling_hashes_.size()));
  CHECK_EQ(0, static_cast<int>(effective_indices_.size()));
  CHECK_EQ(0, static_cast<int>(wildcard_only_indices_.size()));
  CHECK_EQ(0, static_cast<int>(pattern_hash_index_.size()));
  CHECK_EQ(kDontHash, rolling_hash_length_.value());

  if (static_cast<int>(wildcards_.size()) >= kMinPatterns) {
    // Slow path, compute metadata and set rolling_hash_length_ to
    // its final value.
    CompileNonTrivial();
  }

  // When we're done, things should be in a sensible state.
  int32 rolling_hash_length = rolling_hash_length_.value();
  DCHECK_NE(kUncompiled, rolling_hash_length);
  if (rolling_hash_length == kDontHash) {
    DCHECK_EQ(0, static_cast<int>(rolling_hashes_.size()));
    DCHECK_EQ(0, static_cast<int>(effective_indices_.size()));
    DCHECK_EQ(0, static_cast<int>(wildcard_only_indices_.size()));
    DCHECK_EQ(0, static_cast<int>(pattern_hash_index_.size()));
  } else {
    DCHECK_LT(0, rolling_hash_length);
    DCHECK_EQ(wildcards_.size(), rolling_hashes_.size());
    DCHECK_EQ(wildcards_.size(), effective_indices_.size());
    int hash_pats = wildcards_.size() - wildcard_only_indices_.size();
    DCHECK_LE(kMinPatterns, hash_pats);
    DCHECK_LE(2 * hash_pats, static_cast<int>(pattern_hash_index_.size()));
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
  int32 rolling_hash_length = rolling_hash_length_.value();
  // The previous read has acquire semantics, and all writes to
  // rolling_hash_length_ have release semantics.  This means we'll see the
  // results of compilation if rolling_hash_length > 0.
  //
  // NOTE: it is unsafe (and expensive) to just CompareAndSwap (CAS) here.
  //  AtomicInt32::CAS guarantees release semantics but not acquire semantics.
  //  As a result we would potentially miss the results of compilation released
  //  by a prior write to rolling_hash_length_.  This would cause us to read
  //  inconsistent compilation metadata, possibly resulting in a crash.
  if (rolling_hash_length == kUncompiled) {
    if (rolling_hash_length_.CompareAndSwap(kUncompiled, kDontHash) ==
        kUncompiled) {
      // During compilation other Match attempts will see kDontHash
      // and will perform matching naively.  Only the caller that
      // does the kUncompiled -> kDontHash transition is permitted
      // to compile.
      Compile();
    }
    // rolling_hash_length is no longer kUncompiled, due to some call to
    // Compile().  Re-acquire it so that we can safely view the results of
    // compilation so far.
    rolling_hash_length = rolling_hash_length_.value();
  }
  if (rolling_hash_length == kDontHash) {
    // Set of wildcards is small, or compilation was ongoing when we last read
    // rolling_hash_length_.
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
  int rolling_end = str.size() - rolling_hash_length;
  if (rolling_end >= 0) {
    // Do a Rabin-Karp rolling match through the string.
    uint64 rolling_hash = RollingHash(str.data(), 0, rolling_hash_length);
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
        rolling_hash = NextRollingHash(str.data(), ofs, rolling_hash_length,
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
  Uncompile();
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
