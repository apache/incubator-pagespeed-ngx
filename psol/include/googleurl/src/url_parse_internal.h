// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Contains common inline helper functions used by the URL parsing routines.

#ifndef GOOGLEURL_SRC_URL_PARSE_INTERNAL_H__
#define GOOGLEURL_SRC_URL_PARSE_INTERNAL_H__

#include "googleurl/src/url_parse.h"

namespace url_parse {

// We treat slashes and backslashes the same for IE compatability.
inline bool IsURLSlash(char16 ch) {
  return ch == '/' || ch == '\\';
}

// Returns true if we should trim this character from the URL because it is a
// space or a control character.
inline bool ShouldTrimFromURL(char16 ch) {
  return ch <= ' ';
}

// Given an already-initialized begin index and length, this shrinks the range
// to eliminate "should-be-trimmed" characters. Note that the length does *not*
// indicate the length of untrimmed data from |*begin|, but rather the position
// in the input string (so the string starts at character |*begin| in the spec,
// and goes until |*len|).
template<typename CHAR>
inline void TrimURL(const CHAR* spec, int* begin, int* len) {
  // Strip leading whitespace and control characters.
  while (*begin < *len && ShouldTrimFromURL(spec[*begin]))
    (*begin)++;

  // Strip trailing whitespace and control characters. We need the >i test for
  // when the input string is all blanks; we don't want to back past the input.
  while (*len > *begin && ShouldTrimFromURL(spec[*len - 1]))
    (*len)--;
}

// Counts the number of consecutive slashes starting at the given offset
// in the given string of the given length.
template<typename CHAR>
inline int CountConsecutiveSlashes(const CHAR *str,
                                   int begin_offset, int str_len) {
  int count = 0;
  while (begin_offset + count < str_len &&
         IsURLSlash(str[begin_offset + count]))
    ++count;
  return count;
}

// Internal functions in url_parse.cc that parse the path, that is, everything
// following the authority section. The input is the range of everything
// following the authority section, and the output is the identified ranges.
//
// This is designed for the file URL parser or other consumers who may do
// special stuff at the beginning, but want regular path parsing, it just
// maps to the internal parsing function for paths.
void ParsePathInternal(const char* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);
void ParsePathInternal(const char16* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);


// Given a spec and a pointer to the character after the colon following the
// scheme, this parses it and fills in the structure, Every item in the parsed
// structure is filled EXCEPT for the scheme, which is untouched.
void ParseAfterScheme(const char* spec,
                      int spec_len,
                      int after_scheme,
                      Parsed* parsed);
void ParseAfterScheme(const char16* spec,
                      int spec_len,
                      int after_scheme,
                      Parsed* parsed);

}  // namespace url_parse

#endif  // GOOGLEURL_SRC_URL_PARSE_INTERNAL_H__
