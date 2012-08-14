/*
 * Copyright 2012 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FAST_WILDCARD_GROUP_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FAST_WILDCARD_GROUP_H_

#include <vector>
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Wildcard;

// This forms the basis of a wildcard selection mechanism, allowing
// a user to issue a sequence of commands like:
//
//   1. allow *.cc
//   2. allow *.h
//   3. disallow a*.h
//   4. allow ab*.h
//   5. disallow c*.cc
//
// This sequence would yield the following results:
//   Match("x.cc") --> true  due to rule #1
//   Match("c.cc") --> false due to rule #5 which overrides rule #1
//   Match("y.h")  --> true  due to rule #2
//   Match("a.h")  --> false due to rule #3 which overrides rule #2
//   Match("ab.h") --> true  due to rule #4 which overrides rule #3
// So order matters.
//
// Note that concurrent calls to Match(...) are permitted, but modifications
// must not occur concurrently (as you would expect).

/* A note on the algorithm used here:

Wildcard matching uses an O(nm) string search algorithm, where m is pattern
length and n is string length (basically we search forward for first char in the
next pattern chunk, then attempt a match at that position).  This is not the
asymptotically efficient O(n+m) as it ignores the effects of prefixes and
repeated substrings, but the wildcards that occur in PageSpeed tend to contain
chunks of diverse literals and so it's good enough in practice.

WildcardGroup simply iterates through wildcards in the group, attempting to
match against each one in turn.

In FastWildcardGroup we attempt a Rabin-Karp string match for a fixed-size
substring of each of the wildcards.  We choose the largest possible substring
size for a given group (for a single wildcard pattern, this will be the length
of the longest literal in the pattern; for the group, it is the minimum such
length).  Note that in the worst case this is a single character (we treat
all-wildcard patterns specially).  We track the insertion index of the
latest-inserted matched pattern (so the first pattern in the set has index 0,
and initially our insertion index is -1).  As in Rabin-Karp we traverse the
string using a rolling hash; when we encounter a hash match, we retrieve the
corresponding insertion index.  If it's larger than our current insertion index
(the pattern would override), we retrieve the pattern and attempt to match the
whole string against it.  If the match succeeds we update the insertion index.
Our return value is the corresponding "allow" status.

We actually optimize this a little in two ways: rather than remembering the
insertion index, we actually remember the insertion index just before the next
change in "allow" status (the effective index).  So for example, if we insert 10
"allow" patterns in a row and then a single "deny" pattern, matching against the
first "allow" pattern means that we will subsequently check only against the
"deny" pattern.  The second optimization builds on this: if the effective index
is the last pattern in the group (always true if the group is nothing but
"allow" or "deny" entries) then we can immediately return.

We use a simple vector of indexes to store the hash table, dealing with
collisions by linear probing.  Metadata (eg a cached hash) is stored with the
patterns.  We make the table size >= 2x the number of patterns so that chains
don't get long, and all failed probes terminate in an empty bucket.

*/

class FastWildcardGroup {
 public:
  FastWildcardGroup()
      : rolling_hash_length_(kUncompiled) { }
  ~FastWildcardGroup();

  // Determines whether a string is allowed by the wildcard group.  If none of
  // the wildcards in the group matches, allow_by_default is returned.
  bool Match(const StringPiece& str, bool allow_by_default) const;

  // Add an expression to Allow, potentially overriding previous calls to
  // Disallow.
  void Allow(const StringPiece& wildcard);

  // Add an expression to Disallow, potentially overriding previous calls to
  // Allow.
  void Disallow(const StringPiece& wildcard);

  void CopyFrom(const FastWildcardGroup& src);
  void AppendFrom(const FastWildcardGroup& src);

  GoogleString Signature() const;

 private:
  // Special values for rolling hash size.
  static const int32 kUncompiled = -1;
  static const int32 kDontHash = 0;

  void Uncompile();
  void Clear();
  inline int& pattern_hash_index(uint64 rolling_hash) const;
  void Compile() const;
  void CompileNonTrivial() const;

  // To avoid having to new another structure we use parallel
  // vectors.  Note that vector<bool> is special-case implemented
  // in STL to be bit-packed.
  std::vector<Wildcard*> wildcards_;
  std::vector<bool> allow_;  // parallel array (actually a bitvector)

  // Information that is computed during compilation.
  mutable std::vector<uint64> rolling_hashes_;  // One per wildcard
  mutable std::vector<int> effective_indices_;  // One per wildcard
  mutable std::vector<int> wildcard_only_indices_;
  mutable std::vector<int> pattern_hash_index_;  // hash table
  mutable AtomicInt32 rolling_hash_length_;

  DISALLOW_COPY_AND_ASSIGN(FastWildcardGroup);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FAST_WILDCARD_GROUP_H_
