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

#ifndef GOOGLEURL_SRC_URL_UTIL_H__
#define GOOGLEURL_SRC_URL_UTIL_H__

#include <string>

#include "base/string16.h"
#include "googleurl/src/url_common.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_canon.h"

namespace url_util {

// Init ------------------------------------------------------------------------

// Initialization is NOT required, it will be implicitly initialized when first
// used. However, this implicit initialization is NOT threadsafe. If you are
// using this library in a threaded environment and don't have a consistent
// "first call" (an example might be calling "AddStandardScheme" with your
// special application-specific schemes) then you will want to call initialize
// before spawning any threads.
//
// It is OK to call this function more than once, subsequent calls will simply
// "noop", unless Shutdown() was called in the mean time. This will also be a
// "noop" if other calls to the library have forced an initialization
// beforehand.
GURL_API void Initialize();

// Cleanup is not required, except some strings may leak. For most user
// applications, this is fine. If you're using it in a library that may get
// loaded and unloaded, you'll want to unload to properly clean up your
// library.
GURL_API void Shutdown();

// Schemes --------------------------------------------------------------------

// Adds an application-defined scheme to the internal list of "standard" URL
// schemes. This function is not threadsafe and can not be called concurrently
// with any other url_util function. It will assert if the list of standard
// schemes has been locked (see LockStandardSchemes).
GURL_API void AddStandardScheme(const char* new_scheme);

// Sets a flag to prevent future calls to AddStandardScheme from succeeding.
//
// This is designed to help prevent errors for multithreaded applications.
// Normal usage would be to call AddStandardScheme for your custom schemes at
// the beginning of program initialization, and then LockStandardSchemes. This
// prevents future callers from mistakenly calling AddStandardScheme when the
// program is running with multiple threads, where such usage would be
// dangerous.
//
// We could have had AddStandardScheme use a lock instead, but that would add
// some platform-specific dependencies we don't otherwise have now, and is
// overkill considering the normal usage is so simple.
GURL_API void LockStandardSchemes();

// Locates the scheme in the given string and places it into |found_scheme|,
// which may be NULL to indicate the caller does not care about the range.
//
// Returns whether the given |compare| scheme matches the scheme found in the
// input (if any). The |compare| scheme must be a valid canonical scheme or
// the result of the comparison is undefined.
GURL_API bool FindAndCompareScheme(const char* str,
                                   int str_len,
                                   const char* compare,
                                   url_parse::Component* found_scheme);
GURL_API bool FindAndCompareScheme(const char16* str,
                                   int str_len,
                                   const char* compare,
                                   url_parse::Component* found_scheme);
inline bool FindAndCompareScheme(const std::string& str,
                                 const char* compare,
                                 url_parse::Component* found_scheme) {
  return FindAndCompareScheme(str.data(), static_cast<int>(str.size()),
                              compare, found_scheme);
}
inline bool FindAndCompareScheme(const string16& str,
                                 const char* compare,
                                 url_parse::Component* found_scheme) {
  return FindAndCompareScheme(str.data(), static_cast<int>(str.size()),
                              compare, found_scheme);
}

// Returns true if the given string represents a standard URL. This means that
// either the scheme is in the list of known standard schemes.
GURL_API bool IsStandard(const char* spec,
                         const url_parse::Component& scheme);
GURL_API bool IsStandard(const char16* spec,
                         const url_parse::Component& scheme);

// TODO(brettw) remove this. This is a temporary compatibility hack to avoid
// breaking the WebKit build when this version is synced via Chrome.
inline bool IsStandard(const char* spec, int spec_len,
                       const url_parse::Component& scheme) {
  return IsStandard(spec, scheme);
}

// URL library wrappers -------------------------------------------------------

// Parses the given spec according to the extracted scheme type. Normal users
// should use the URL object, although this may be useful if performance is
// critical and you don't want to do the heap allocation for the std::string.
//
// As with the url_canon::Canonicalize* functions, the charset converter can
// be NULL to use UTF-8 (it will be faster in this case).
//
// Returns true if a valid URL was produced, false if not. On failure, the
// output and parsed structures will still be filled and will be consistent,
// but they will not represent a loadable URL.
GURL_API bool Canonicalize(const char* spec,
                           int spec_len,
                           url_canon::CharsetConverter* charset_converter,
                           url_canon::CanonOutput* output,
                           url_parse::Parsed* output_parsed);
GURL_API bool Canonicalize(const char16* spec,
                           int spec_len,
                           url_canon::CharsetConverter* charset_converter,
                           url_canon::CanonOutput* output,
                           url_parse::Parsed* output_parsed);

// Resolves a potentially relative URL relative to the given parsed base URL.
// The base MUST be valid. The resulting canonical URL and parsed information
// will be placed in to the given out variables.
//
// The relative need not be relative. If we discover that it's absolute, this
// will produce a canonical version of that URL. See Canonicalize() for more
// about the charset_converter.
//
// Returns true if the output is valid, false if the input could not produce
// a valid URL.
GURL_API bool ResolveRelative(const char* base_spec,
                              int base_spec_len,
                              const url_parse::Parsed& base_parsed,
                              const char* relative,
                              int relative_length,
                              url_canon::CharsetConverter* charset_converter,
                              url_canon::CanonOutput* output,
                              url_parse::Parsed* output_parsed);
GURL_API bool ResolveRelative(const char* base_spec,
                              int base_spec_len,
                              const url_parse::Parsed& base_parsed,
                              const char16* relative,
                              int relative_length,
                              url_canon::CharsetConverter* charset_converter,
                              url_canon::CanonOutput* output,
                              url_parse::Parsed* output_parsed);

// Replaces components in the given VALID input url. The new canonical URL info
// is written to output and out_parsed.
//
// Returns true if the resulting URL is valid.
GURL_API bool ReplaceComponents(
    const char* spec,
    int spec_len,
    const url_parse::Parsed& parsed,
    const url_canon::Replacements<char>& replacements,
    url_canon::CharsetConverter* charset_converter,
    url_canon::CanonOutput* output,
    url_parse::Parsed* out_parsed);
GURL_API bool ReplaceComponents(
    const char* spec,
    int spec_len,
    const url_parse::Parsed& parsed,
    const url_canon::Replacements<char16>& replacements,
    url_canon::CharsetConverter* charset_converter,
    url_canon::CanonOutput* output,
    url_parse::Parsed* out_parsed);

// String helper functions ----------------------------------------------------

// Compare the lower-case form of the given string against the given ASCII
// string.  This is useful for doing checking if an input string matches some
// token, and it is optimized to avoid intermediate string copies.
//
// The versions of this function that don't take a b_end assume that the b
// string is NULL terminated.
GURL_API bool LowerCaseEqualsASCII(const char* a_begin,
                                   const char* a_end,
                                   const char* b);
GURL_API bool LowerCaseEqualsASCII(const char* a_begin,
                                   const char* a_end,
                                   const char* b_begin,
                                   const char* b_end);
GURL_API bool LowerCaseEqualsASCII(const char16* a_begin,
                                   const char16* a_end,
                                   const char* b);

// Unescapes the given string using URL escaping rules.
GURL_API void DecodeURLEscapeSequences(const char* input, int length,
                                       url_canon::CanonOutputW* output);

// Escapes the given string as defined by the JS method encodeURIComponent.  See
// https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/encodeURIComponent
GURL_API void EncodeURIComponent(const char* input, int length,
                                 url_canon::CanonOutput* output);


}  // namespace url_util

#endif  // GOOGLEURL_SRC_URL_UTIL_H__
