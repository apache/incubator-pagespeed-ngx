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

#include "pagespeed/kernel/base/string_util.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

GoogleString StrCat(const StringPiece& a, const StringPiece& b) {
  GoogleString res;
  res.reserve(a.size() + b.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a, const StringPiece& b,
                    const StringPiece& c) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a, const StringPiece& b,
                    const StringPiece& c, const StringPiece& d) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a, const StringPiece& b,
                    const StringPiece& c, const StringPiece& d,
                    const StringPiece& e) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f,
                    const StringPiece& g) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  return res;
}
GoogleString StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f,
                    const StringPiece& g,
                    const StringPiece& h) {
  GoogleString res;
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
GoogleString StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f,
                    const StringPiece& g,
                    const StringPiece& h,
                    const StringPiece& i) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size() + h.size() + i.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  h.AppendToString(&res);
  i.AppendToString(&res);
  return res;
}

void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b) {
  target->reserve(target->size() +
                  a.size() + b.size());
  a.AppendToString(target);
  b.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d,
               const StringPiece& e) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d,
               const StringPiece& e,
               const StringPiece& f) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d,
               const StringPiece& e,
               const StringPiece& f,
               const StringPiece& g) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d,
               const StringPiece& e,
               const StringPiece& f,
               const StringPiece& g,
               const StringPiece& h) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size() + h.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
  h.AppendToString(target);
}
void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b,
               const StringPiece& c,
               const StringPiece& d,
               const StringPiece& e,
               const StringPiece& f,
               const StringPiece& g,
               const StringPiece& h,
               const StringPiece& i) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size() + h.size() + i.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
  h.AppendToString(target);
  i.AppendToString(target);
}

void SplitStringPieceToVector(const StringPiece& sp,
                              const StringPiece& separators,
                              StringPieceVector* components,
                              bool omit_empty_strings) {
  size_t prev_pos = 0;
  size_t pos = 0;
  while ((pos = sp.find_first_of(separators, pos)) != StringPiece::npos) {
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

void SplitStringUsingSubstr(const StringPiece& full,
                            const StringPiece& substr,
                            StringPieceVector* result) {
  StringPiece::size_type begin_index = 0;
  while (true) {
    const StringPiece::size_type end_index = full.find(substr, begin_index);
    if (end_index == StringPiece::npos) {
      const StringPiece term = full.substr(begin_index);
      result->push_back(term);
      return;
    }
    const StringPiece term = full.substr(begin_index, end_index - begin_index);
    if (!term.empty()) {
      result->push_back(term);
    }
    begin_index = end_index + substr.size();
  }
}

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     GoogleString* dest) {
  dest->reserve(dest->size() + src.size());
  for (const char *p = src.data(), *end = src.data() + src.size();
       p != end; ++p) {
    if (to_escape.find(*p) != StringPiece::npos) {
      dest->push_back('\\');
    }
    dest->push_back(*p);
  }
}

GoogleString CEscape(const StringPiece& src) {
  int len = src.length();
  const char* read = src.data();
  const char* end = read + len;
  int used = 0;
  char* dest = new char[len * 4 + 1];
  for (; read != end; ++read) {
    unsigned char ch = static_cast<unsigned char>(*read);
    switch (ch) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n'; break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r'; break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't'; break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default:
        if (ch < 32 || ch >= 127) {
          base::snprintf(dest + used, 5, "\\%03o", ch);  // NOLINT
          used += 4;
        } else {
          dest[used++] = ch;
        }
        break;
    }
  }
  GoogleString final(dest, used);
  delete[] dest;
  return final;
}

// From src/third_party/protobuf/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
bool HasPrefixString(const StringPiece& str, const StringPiece& prefix) {
  return ((str.size() >= prefix.size()) &&
          (str.substr(0, prefix.size()) == prefix));
}

// From src/third_party/protobuf/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
void UpperString(GoogleString* s) {
  GoogleString::iterator end = s->end();
  for (GoogleString::iterator i = s->begin(); i != end; ++i) {
    *i = UpperChar(*i);
  }
}

void LowerString(GoogleString* s) {
  GoogleString::iterator end = s->end();
  for (GoogleString::iterator i = s->begin(); i != end; ++i) {
    *i = LowerChar(*i);
  }
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
                           GoogleString* s) {
  CHECK(s != NULL);
  if (s->empty())
    return 0;
  GoogleString tmp;
  int num_replacements = 0;
  size_t pos = 0;
  for (size_t match_pos = s->find(substring.data(), pos, substring.length());
       match_pos != GoogleString::npos;
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

GoogleString JoinStringStar(const ConstStringStarVector& vector,
                            const StringPiece& delim) {
  GoogleString result;

  if (vector.size() > 0) {
    // Precompute resulting length so we can reserve() memory in one shot.
    int length = delim.size() * (vector.size() - 1);
    for (ConstStringStarVector::const_iterator iter = vector.begin();
         iter < vector.end(); ++iter) {
      length += (*iter)->size();
    }
    result.reserve(length);

    // Now combine everything.
    for (ConstStringStarVector::const_iterator iter = vector.begin();
         iter < vector.end(); ++iter) {
      if (iter != vector.begin()) {
        result.append(delim.data(), delim.size());
      }
      result.append(**iter);
    }
  }

  return result;
}

bool StringCaseStartsWith(const StringPiece& str, const StringPiece& prefix) {
  return ((str.size() >= prefix.size()) &&
          (0 == StringCaseCompare(prefix, str.substr(0, prefix.size()))));
}

bool StringCaseEndsWith(const StringPiece& str, const StringPiece& suffix) {
  return ((str.size() >= suffix.size()) &&
          (0 == StringCaseCompare(suffix,
                                  str.substr(str.size() - suffix.size()))));
}

bool StringEqualConcat(const StringPiece& str, const StringPiece& first,
                       const StringPiece& second) {
  return (str.size() == first.size() + second.size()) &&
      str.starts_with(first) && str.ends_with(second);
}

StringPiece PieceAfterEquals(const StringPiece& piece) {
  size_t index = piece.find("=");
  if (index != piece.npos) {
    ++index;
    StringPiece ret = piece;
    ret.remove_prefix(index);
    TrimWhitespace(&ret);
    return ret;
  }
  return StringPiece(piece.data(), 0);
}

void ParseShellLikeString(const StringPiece& input,
                          std::vector<GoogleString>* output) {
  output->clear();
  for (size_t index = 0; index < input.size();) {
    const char ch = input[index];
    if (ch == '"' || ch == '\'') {
      // If we see a quoted section, treat it as a single item even if there are
      // spaces in it.
      const char quote = ch;
      ++index;  // skip open quote
      output->push_back("");
      GoogleString& part = output->back();
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
    } else if (!IsHtmlSpace(ch)) {
      // Without quotes, items are whitespace-separated.
      output->push_back("");
      GoogleString& part = output->back();
      for (; index < input.size() && !IsHtmlSpace(input[index]); ++index) {
        part.push_back(input[index]);
      }
    } else {
      // Ignore whitespace (outside of quotes).
      ++index;
    }
  }
}

int CountSubstring(const StringPiece& text, const StringPiece& substring) {
  int number_of_occurrences = 0;
  size_t pos = 0;
  for (size_t match_pos = text.find(substring, pos);
       match_pos != StringPiece::npos;
       pos = match_pos + 1, match_pos = text.find(substring, pos)) {
    number_of_occurrences++;
  }
  return number_of_occurrences;
}

int FindIgnoreCase(StringPiece haystack, StringPiece needle) {
  int pos = 0;
  while (haystack.size() >= needle.size()) {
    if (StringCaseStartsWith(haystack, needle)) {
      return pos;
    }
    ++pos;
    haystack.remove_prefix(1);
  }
  return static_cast<int>(StringPiece::npos);
}


// In-place StringPiece whitespace trimming.  This mutates the StringPiece.
bool TrimLeadingWhitespace(StringPiece* str) {
  // We use stringpiece_ssize_type to avoid signedness problems.
  stringpiece_ssize_type size = str->size();
  stringpiece_ssize_type trim = 0;
  while (trim != size && IsHtmlSpace(str->data()[trim])) {
    ++trim;
  }
  str->remove_prefix(trim);
  return (trim > 0);
}

bool TrimTrailingWhitespace(StringPiece* str) {
  // We use stringpiece_ssize_type to avoid signedness problems.
  stringpiece_ssize_type rightmost = str->size();
  while (rightmost != 0 && IsHtmlSpace(str->data()[rightmost - 1])) {
    --rightmost;
  }
  if (rightmost != str->size()) {
    str->remove_suffix(str->size() - rightmost);
    return true;
  }
  return false;
}

bool TrimWhitespace(StringPiece* str) {
  // We *must* trim *both* leading and trailing spaces, so we use the
  // non-shortcut bitwise | on the boolean results.
  return TrimLeadingWhitespace(str) | TrimTrailingWhitespace(str);
}

void TrimQuote(StringPiece* str) {
  TrimWhitespace(str);
  if (str->starts_with("\"") || str->starts_with("'")) {
    str->remove_prefix(1);
  }
  if (str->ends_with("\"") || str->ends_with("'")) {
    str->remove_suffix(1);
  }
  TrimWhitespace(str);
}


// TODO(jmarantz): This is a very slow implementation.  But strncasecmp
// will fail test StringCaseTest.Locale.  If this shows up as a performance
// bottleneck then an SSE implementation would be better.
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

bool MemCaseEqual(const char* s1, size_t size1, const char* s2, size_t size2) {
  if (size1 != size2) {
    return false;
  }
  for (; size1 != 0; --size1, ++s1, ++s2) {
    if (UpperChar(*s1) != UpperChar(*s2)) {
      return false;
    }
  }
  return true;
}

bool SplitStringPieceToIntegerVector(
    const StringPiece& src, const StringPiece& separators,
    std::vector<int>* ints) {
  StringPieceVector values;
  SplitStringPieceToVector(
      src, separators, &values, true /* omit_empty_strings */);
  ints->clear();
  int v;
  for (int i = 0, n = values.size(); i < n; ++i) {
    if (StringToInt(values[i], &v)) {
      ints->push_back(v);
    } else {
      ints->clear();
      return false;
    }
  }
  return true;
}

}  // namespace net_instaweb
