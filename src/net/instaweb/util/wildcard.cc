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

#include "net/instaweb/util/public/wildcard.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char Wildcard::kMatchAny = '*';
const char Wildcard::kMatchOne = '?';

Wildcard::Wildcard(const StringPiece& wildcard_spec)
    : storage_(wildcard_spec.data(), wildcard_spec.size()) {
  InitFromStorage();
}

void Wildcard::InitFromStorage() {
  // Pre-scan the wildcard spec into an array of StringPieces.  We will
  // copy the original string spec into storage_ but that will only be
  // backing-store for the StringPieces.
  //
  // E.g. we will transform "a?c*def**?xyz?" into
  //   {"a", "?", "c", "*", "def", "*", "?", "xyz", "?"}
  // Note that multiple consecutive '*' will be collapsed into one.
  //
  // TODO(jmarantz): jmaessen suggests moving ? ahead of * as it will
  // reduce code complexity and the cost of backtracking.
  int num_pieces = 0;
  char prev_c = '\0';
  for (int i = 0, n = storage_.size(); i < n; ++i) {
    char c = storage_[i];
    bool add_to_previous = (num_pieces != 0);
    bool skip = false;
    if (add_to_previous) {
      if (c == kMatchAny) {
        add_to_previous = false;
        skip = (prev_c == kMatchAny);  // multiple * in a row means nothing
      } else if (c == kMatchOne) {
        add_to_previous = false;
      } else {
        add_to_previous = ((prev_c != kMatchAny) && (prev_c != kMatchOne));
      }
    }
    if (add_to_previous) {
      pieces_[num_pieces - 1] = StringPiece(pieces_[num_pieces - 1].data(),
                                            pieces_[num_pieces - 1].size() + 1);
    } else if (!skip) {
      pieces_.push_back(StringPiece(storage_.data() + i, 1));
      ++num_pieces;
    }
    prev_c = c;
  }
}

bool Wildcard::MatchHelper(int piece_index, const StringPiece& str) {
  bool prev_was_any = false;
  size_t num_skips_after_any = 0;
  size_t str_index = 0;

  // Walk through the pieces parsed out in the constructor, matching them
  // against str.  Our algorithm will walk linealy through str, although we
  // may need to backtrack if there are multiple matches for the segment
  // following a *.
  for (int num_pieces = pieces_.size(); piece_index < num_pieces; ++piece_index) {
    StringPiece piece = pieces_[piece_index];
    if (piece[0] == kMatchAny) {
      prev_was_any = true;
    } else if (piece[0] == kMatchOne) {
      if (prev_was_any) {
        ++num_skips_after_any;
      } else {
        ++str_index;
        if (str_index > str.size()) {
          return false;
        }
      }
    } else if (prev_was_any) {
      str_index += num_skips_after_any;
      if (str_index > str.size()) {
        return false;
      }

      // Now we have the unenviable task of figuring out how many
      // characters of 'str' to swallow.  Consider this complexity.
      //    CHECK(Wildcard("*abcd?").Match("abcabcdabcdabcdabcde"));
      // If we greedily match the "a" in the wildcard against any of
      // the first 4 'a's in the string then we are screwed -- we
      // won't find the d.  Even if we match against the first or
      // second "abcd" we will get a failure because we will have
      // string left, but no more pattern.
      //
      // There are probably more efficient ways to do this, such as in
      // http://code.google.com/p/re2/, but we will, for short-term
      // expediency, use recursion to search all the possible matches
      // for the current piece in str.
      while ((str_index = str.find(piece, str_index)) != StringPiece::npos) {
        if (MatchHelper(piece_index + 1,
                        str.substr(str_index + piece.size()))) {
          return true;
        }
        ++str_index;
      }
      return false;
    } else if ((str.size() - str_index) < piece.size()) {
      return false;
    } else if (str.substr(str_index, piece.size()) == piece) {
      str_index += piece.size();
    } else {
      return false;
    }
  }
  if (prev_was_any) {
    return (str.size() >= num_skips_after_any);
  }
  return (str_index == str.size());
}

bool Wildcard::IsSimple() const {
  if (pieces_.size() == 0) {
    return true;
  }
  if (pieces_.size() != 1) {
    return false;
  }
  StringPiece piece = pieces_[0];
  CHECK(!piece.empty());
  char ch = piece[0];
  return ((ch != kMatchAny) && (ch != kMatchOne));
}

Wildcard* Wildcard::Duplicate() const {
  return new Wildcard(storage_);
}

}  // namespace net_instaweb
