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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/string_util.h"
#include <vector>

namespace net_instaweb {

void SplitStringPieceToVector(const StringPiece& sp, const char* separator,
                              std::vector<StringPiece>* components,
                              bool omit_empty_strings) {
  size_t prev_pos = 0;
  size_t pos = 0;
  StringPiece sep(separator);
  while ((pos = sp.find(sep, pos)) != StringPiece::npos) {
    if (!omit_empty_strings || (pos > prev_pos)) {
      components->push_back(sp.substr(prev_pos, pos - prev_pos));
    }
    ++pos;
    prev_pos = pos;
  }
  if (!omit_empty_strings || (prev_pos < sp.size())) {
    components->push_back(sp.substr(prev_pos, prev_pos - sp.size()));
  }
}

std::string StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f,
                    const StringPiece& g,
                    const StringPiece& h) {
  std::string res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size() + h.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  h.AppendToString(&res);
  return res;
}

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     std::string* dest) {
  dest->reserve(dest->size() + src.size());
  for (const char *p = src.data(), *end = src.data() + src.size();
       p != end; ++p) {
    if (to_escape.find(*p) != StringPiece::npos) {
      dest->push_back('\\');
    }
    dest->push_back(*p);
  }
}

// From src/third_party/protobuf2/src/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
void LowerString(std::string* s) {
  std::string::iterator end = s->end();
  for (std::string::iterator i = s->begin(); i != end; ++i) {
    *i = LowerChar(*i);
  }
}

void UpperString(std::string* s) {
  std::string::iterator end = s->end();
  for (std::string::iterator i = s->begin(); i != end; ++i) {
    *i = UpperChar(*i);
  }
}

bool StringCaseEqual(const StringPiece& s1, const StringPiece& s2) {
  return ((s1.size() == s2.size()) && (0 == StringCaseCompare(s1, s2)));
}

bool StringCaseStartsWith(const StringPiece& str, const StringPiece& prefix) {
  return (str.size() >= prefix.size() &&
          0 == StringNCaseCompare(str.data(), prefix.data(), prefix.size()));
}

bool StringCaseEndsWith(const StringPiece& str, const StringPiece& suffix) {
  return ((str.size() >= suffix.size()) &&
          (0 == StringNCaseCompare(str.data() + str.size() - suffix.size(),
                                   suffix.data(), suffix.size())));
}

void ParseShellLikeString(const StringPiece& input,
                          std::vector<std::string>* output) {
  output->clear();
  for (size_t index = 0; index < input.size();) {
    const char ch = input[index];
    // If we see a quoted section, treat it as a single item even if there are
    // spaces in it.
    if (ch == '"' || ch == '\'') {
      const char quote = ch;
      ++index;  // skip open quote
      output->push_back("");
      std::string& part = output->back();
      for (; index < input.size() && input[index] != quote; ++index) {
        if (input[index] == '\\') {
          ++index;  // skip backslash
          if (index >= input.size()) {
            break;
          }
        }
        part.push_back(input[index]);
      }
      ++index;  // skip close quote
    }
    // Without quotes, items are whitespace-separated.
    else if (!isspace(ch)) {
      output->push_back("");
      std::string& part = output->back();
      for (;index < input.size() && !isspace(input[index]); ++index) {
        part.push_back(input[index]);
      }
    }
    // Ignore whitespace (outside of quotes).
    else {
      ++index;
    }
  }
}

// From src/third_party/protobuf2/src/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
bool HasPrefixString(const StringPiece& str, const StringPiece& prefix) {
  return ((str.size() >= prefix.size()) &&
          (str.substr(0, prefix.size()) == prefix));
}

// ----------------------------------------------------------------------
// GlobalReplaceSubstring()
//    Replaces all instances of a substring in a string.  Returns the
//    number of replacements.
//
//    NOTE: The string pieces must not overlap s.
// ----------------------------------------------------------------------
int GlobalReplaceSubstring(const StringPiece& substring,
                           const StringPiece& replacement,
                           std::string* s) {
  CHECK(s != NULL);
  if (s->empty())
    return 0;
  std::string tmp;
  int num_replacements = 0;
  size_t pos = 0;
  for (size_t match_pos = s->find(substring.data(), pos, substring.length());
       match_pos != std::string::npos;
       pos = match_pos + substring.length(),
           match_pos = s->find(substring.data(), pos, substring.length())) {
    ++num_replacements;
    // Append the original content before the match.
    tmp.append(*s, pos, match_pos - pos);
    // Append the replacement for the match.
    tmp.append(replacement.begin(), replacement.end());
  }
  // Append the content after the last match. If no replacements were made, the
  // original string is left untouched.
  if (num_replacements > 0) {
    tmp.append(*s, pos, s->length() - pos);
    s->swap(tmp);
  }
  return num_replacements;
}

int StringCaseCompare(const StringPiece& s1, const StringPiece& s2) {
  for (int i = 0, n = std::min(s1.size(), s2.size()); i < n; ++i) {
    unsigned char c1 = UpperChar(s1[i]);
    unsigned char c2 = UpperChar(s2[i]);
    if (c1 < c2) {
      return -1;
    } else if (c1 > c2) {
      return 1;
    }
  }
  if (s1.size() < s2.size()) {
    return -1;
  } else if (s1.size() > s2.size()) {
    return 1;
  }
  return 0;
}

const StringPiece EmptyString::kEmptyString;

}  // namespace net_instaweb
