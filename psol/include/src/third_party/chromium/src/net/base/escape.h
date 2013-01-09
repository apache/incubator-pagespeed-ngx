// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ESCAPE_H_
#define NET_BASE_ESCAPE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "net/base/net_api.h"

// Escaping --------------------------------------------------------------------

// Escape a file.  This includes:
// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
NET_API std::string EscapePath(const std::string& path);

// Escape application/x-www-form-urlencoded content.  This includes:
// non-printable, non-7bit, and (including space)  ?>=<;+'&%$#"![\]^`{|}
// Space is escaped as + (if use_plus is true) and other special characters
// as %XX (hex).
NET_API std::string EscapeUrlEncodedData(const std::string& path,
                                         bool use_plus);

// Escape all non-ASCII input.
NET_API std::string EscapeNonASCII(const std::string& input);

// Escapes characters in text suitable for use as an external protocol handler
// command.
// We %XX everything except alphanumerics and %-_.!~*'() and the restricted
// chracters (;/?:@&=+$,).
NET_API std::string EscapeExternalHandlerValue(const std::string& text);

// Append the given character to the output string, escaping the character if
// the character would be interpretted as an HTML delimiter.
NET_API void AppendEscapedCharForHTML(char c, std::string* output);

// Escape chars that might cause this text to be interpretted as HTML tags.
NET_API std::string EscapeForHTML(const std::string& text);
NET_API string16 EscapeForHTML(const string16& text);

// Unescaping ------------------------------------------------------------------

class UnescapeRule {
 public:
  // A combination of the following flags that is passed to the unescaping
  // functions.
  typedef uint32 Type;

  enum {
    // Don't unescape anything at all.
    NONE = 0,

    // Don't unescape anything special, but all normal unescaping will happen.
    // This is a placeholder and can't be combined with other flags (since it's
    // just the absence of them). All other unescape rules imply "normal" in
    // addition to their special meaning. Things like escaped letters, digits,
    // and most symbols will get unescaped with this mode.
    NORMAL = 1,

    // Convert %20 to spaces. In some places where we're showing URLs, we may
    // want this. In places where the URL may be copied and pasted out, then
    // you wouldn't want this since it might not be interpreted in one piece
    // by other applications.
    SPACES = 2,

    // Unescapes various characters that will change the meaning of URLs,
    // including '%', '+', '&', '/', '#'. If we unescaped these characters, the
    // resulting URL won't be the same as the source one. This flag is used when
    // generating final output like filenames for URLs where we won't be
    // interpreting as a URL and want to do as much unescaping as possible.
    URL_SPECIAL_CHARS = 4,

    // Unescapes control characters such as %01. This INCLUDES NULLs. This is
    // used for rare cases such as data: URL decoding where the result is binary
    // data. You should not use this for normal URLs!
    CONTROL_CHARS = 8,

    // URL queries use "+" for space. This flag controls that replacement.
    REPLACE_PLUS_WITH_SPACE = 16,
  };
};

// Unescapes |escaped_text| and returns the result.
// Unescaping consists of looking for the exact pattern "%XX", where each X is
// a hex digit, and converting to the character with the numerical value of
// those digits. Thus "i%20=%203%3b" unescapes to "i = 3;".
//
// Watch out: this doesn't necessarily result in the correct final result,
// because the encoding may be unknown. For example, the input might be ASCII,
// which, after unescaping, is supposed to be interpreted as UTF-8, and then
// converted into full UTF-16 chars. This function won't tell you if any
// conversions need to take place, it only unescapes.
NET_API std::string UnescapeURLComponent(const std::string& escaped_text,
                                         UnescapeRule::Type rules);
NET_API string16 UnescapeURLComponent(const string16& escaped_text,
                                      UnescapeRule::Type rules);

// Unescapes the given substring as a URL, and then tries to interpret the
// result as being encoded as UTF-8. If the result is convertable into UTF-8, it
// will be returned as converted. If it is not, the original escaped string will
// be converted into a string16 and returned. (|offset[s]_for_adjustment|)
// specifies one or more offsets into the source strings; each offset will be
// adjusted to point at the same logical place in the result strings during
// decoding.  If this isn't possible because an offset points past the end of
// the source strings or into the middle of a multibyte sequence, the offending
// offset will be set to string16::npos. |offset[s]_for_adjustment| may be NULL.
NET_API string16 UnescapeAndDecodeUTF8URLComponent(
    const std::string& text,
    UnescapeRule::Type rules,
    size_t* offset_for_adjustment);
NET_API string16 UnescapeAndDecodeUTF8URLComponentWithOffsets(
    const std::string& text,
    UnescapeRule::Type rules,
    std::vector<size_t>* offsets_for_adjustment);

// Unescape the following ampersand character codes from |text|:
// &lt; &gt; &amp; &quot; &#39;
NET_API string16 UnescapeForHTML(const string16& text);

// Deprecated ------------------------------------------------------------------

// Escapes characters in text suitable for use as a query parameter value.
// We %XX everything except alphanumerics and -_.!~*'()
// Spaces change to "+" unless you pass usePlus=false.
// This is basically the same as encodeURIComponent in javascript.
// For the string16 version, we do a conversion to charset before encoding the
// string.  If the charset doesn't exist, we return false.
NET_API std::string EscapeQueryParamValue(const std::string& text,
                                          bool use_plus);
NET_API bool EscapeQueryParamValue(const string16& text, const char* codepage,
                                   bool use_plus, string16* escaped);

// A specialized version of EscapeQueryParamValue for string16s that
// assumes the codepage is UTF8.  This is provided as a convenience.
NET_API string16 EscapeQueryParamValueUTF8(const string16& text, bool use_plus);

// Private Functions (Exposed for Unit Testing) --------------------------------

// A function called by std::for_each that will adjust any offset which occurs
// after one or more encoded characters.
struct NET_TEST AdjustEncodingOffset {
  typedef std::vector<size_t> Adjustments;

  explicit AdjustEncodingOffset(const Adjustments& adjustments);
  void operator()(size_t& offset);

  const Adjustments& adjustments;
};

#endif  // NET_BASE_ESCAPE_H_
