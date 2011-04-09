// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)
//         jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/util/public/wildcard.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char Wildcard::kMatchAny = '*';
const char Wildcard::kMatchOne = '?';

Wildcard::Wildcard(const StringPiece& wildcard_spec) {
  InitFromSpec(wildcard_spec);
}

Wildcard::Wildcard(const GoogleString& storage, int num_blocks,
                   int last_block_offset, bool is_simple)
    : storage_(storage),
      num_blocks_(num_blocks),
      last_block_offset_(last_block_offset),
      is_simple_(is_simple) {
}

// Pre-scan the wildcard spec into storage_, canonicalizing its representation
// as we go.  We view the input wildcard_spec as a series of possibly-empty
// blocks each of which contains a mix of literal characters and kMatchOne (?),
// separated by kMatchAny (*).  Each block matches a fixed number of characters
// in a candidate string.
//
// We transform this into an internal representation (in storage_) that contains
// a series of blocks each *terminated* by a *.  This means that we end up
// adding a sentinel * at the end of the string, and that our interpretation of
// * changes: it now represents a block terminator, rather than a sequence of
// arbitrary characters.  This transformation simplifies termination testing in
// the inner match loop (MatchBlock).
//
// Another way to think about this is that we could use a special 257th
// separator character, and rewrite the input into blocks terminated by the
// separator character.  Rather than using this nonexistent character, we decide
// to reuse * instead---at the price of a bit of potential confusion.
//
// We also observe that the sub-pattern *? matches exactly the same set of
// strings as ?*, and that ** matches the same set of strings as *.  We use this
// to eliminate empty blocks (except at the start and end of string), and to
// make sure that every block except the first begins with a literal character
// and not a ? (by shifting the ? to the end of the previous block).  This
// permits a fast search for start of block during matching using memchr.
//
// We also remember the start of the last block in storage_, as the first and
// last blocks must match at an exact position in a string; the middle blocks
// are treated differently, as their position in a matched string can vary.
// After preprocessing, only the first or last block may be empty (corresponding
// to a leading or trailing * respectively).
void Wildcard::InitFromSpec(const StringPiece& wildcard_spec) {
  storage_.reserve(wildcard_spec.size() + 1);
  num_blocks_ = 1;
  last_block_offset_ = 0;
  is_simple_ = true;
  bool last_was_any = false;
  for (size_t i = 0; i < wildcard_spec.size(); ++i) {
    char c = wildcard_spec[i];
    if (c == kMatchAny) {
      // Note that this in effect deletes redundant *s
      // (by simply setting last_was_any more than once).
      last_was_any = true;
      is_simple_ = false;
    } else if (c == kMatchOne) {
      // Move ? to end of previous block by dint of adding it to pattern
      // without inserting * first if last_was_any is set.
      // So a? => a? but a*? => a?*.  This means that * is always followed by
      // a literal char or end of string after preprocessing.
      storage_.push_back(c);
      is_simple_ = false;
    } else {
      if (last_was_any) {
        ++num_blocks_;
        storage_.push_back(kMatchAny);
        last_block_offset_ = storage_.size();
        last_was_any = false;
      }
      storage_.push_back(c);
    }
  }
  // Clean up after trailing * (leading to empty last block)
  if (last_was_any) {
    ++num_blocks_;
    storage_.push_back(kMatchAny);
    last_block_offset_ = storage_.size();
  }
  // Insert sentinel * at end of storage_ to make inner match loop simpler.
  storage_.push_back(kMatchAny);
}

namespace {

// Match pat block (terminated by a *) against str, return offset of first
// mismatch or of the * in pat.  Requires that str be long enough to
// contain chars in block (not counting final *).
int MatchBlock(const char* pat, const char* str) {
  int pos;
  for (pos = 0 ;
       pat[pos] != Wildcard::kMatchAny &&
       (pat[pos] == str[pos] || pat[pos] == Wildcard::kMatchOne); ++pos) { }
  return pos;
}

}  // namespace

bool Wildcard::Match(const StringPiece& actual) {
  // We match a block at a time, checking incrementally that there are always
  // enough characters remaining in actual to match the remaining blocks in
  // storage_.  We do this by maintaining "chars_to_skip", which counts the
  // remaining number of characters that must be skipped over between blocks.
  // We start by matching the first and last blocks, as those must be located at
  // the beginning and end of the string respectively.  We then match the middle
  // blocks left to right, using memchr() to identify candidate positions for
  // matching.  We only need to match a given block once, but that might require
  // multiple match attempts.  The leftmost match is sufficient because our only
  // wildcards are ? and *, which match arbitrary characters.

  // Overall length check.  Guarantees that the first and last pattern blocks
  // will match without length checking, since they're matched at fixed
  // positions in actual and we don't skip any chars.
  int chars_in_pat = storage_.size() - num_blocks_;
  int chars_to_skip = actual.size() - chars_in_pat;
  if (chars_to_skip < 0) {
    return false;
  }
  const char* pat = storage_.data();
  const char* str = actual.data();
  int blocks_left = num_blocks_;
  // Match last block.  This block can't be shifted wrt actual.
  int last_block_size = storage_.size() - last_block_offset_ - 1;
  const char* pat_last_block = pat + last_block_offset_;
  const char* str_last_block = str + actual.size() - last_block_size;
  int ofs = MatchBlock(pat_last_block, str_last_block);
  if (pat_last_block[ofs] != kMatchAny) {
    return false;
  }
  if (--blocks_left == 0) {
    // There was only one block (the last), and it matched.
    // Succeed if entire string was consumed.
    return (chars_to_skip == 0);
  }
  // Match first block.  This block can't be shifted wrt actual.
  ofs = MatchBlock(pat, str);
  if (pat[ofs] != kMatchAny) {
    return false;
  }
  str += ofs;
  pat += ofs + 1;  // Skip leading *
  --blocks_left;
  // Match all remaining blocks, left to right.  We try candidate
  // positions that match the first char in each block.
  while (blocks_left > 0) {
    // Here are our invariants (the latter two guaranteed by
    // initialization).
    DCHECK_EQ(kMatchAny, pat[-1]);
    DCHECK_NE(kMatchAny, pat[ 0]);
    DCHECK_NE(kMatchOne, pat[ 0]);
    // The number of characters left to match in the pattern plus the remaining
    // chars_to_skip must be equal to the number of characters remaining in the
    // string.  This invariant is guaranteed by reducing chars_to_skip when we
    // skip chars in str.
    DCHECK_EQ(chars_to_skip + (pat_last_block - pat) - blocks_left,
              str_last_block - str);
    // Advance str to first occurrence of pat[0]; that's next
    // candidate match position.
    const char* new_str =
        static_cast<const char*>(memchr(str, pat[0], str_last_block - str));
    if (new_str == NULL) {
      // First char in block wasn't found, so we can't match.
      return false;
    }
    // memchr skipped over chars in str.  Adjust chars_to_skip to match.
    chars_to_skip -= (new_str - str);
    if (chars_to_skip < 0) {
      // More chars left in remaining blocks than in str.
      return false;
    }
    str = new_str;
    // Now check for a match here.  We already know pat[0] == str[0].
    ofs = 1 + MatchBlock(pat + 1, str + 1);
    if (pat[ofs] != kMatchAny) {
      // We failed to match leftmost occurence of *pat in str.
      // Move further right in str and try to match current block again.
      ++str;
      --chars_to_skip;
      if (chars_to_skip < 0) {
        // With new shift, once again more chars left in remaining blocks than
        // in str.
        return false;
      }
    } else {
      // Matched.  Advance to next block of pattern.
      str += ofs;
      pat += ofs + 1;                 // Skip the *
      --blocks_left;
    }
  }
  return true;
}

Wildcard* Wildcard::Duplicate() const {
  return new Wildcard(storage_, num_blocks_, last_block_offset_, is_simple_);
}

}  // namespace net_instaweb
