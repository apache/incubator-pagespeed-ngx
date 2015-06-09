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
// Author: mbelshe@google.com (Mike Belshe)
//         jmarantz@google.com (Joshua Marantz)
//
// URL filename encoder goals:
//
// 1. Allow URLs with arbitrary path-segment length, generating filenames
//    with a maximum of 128 characters.
// 2. Provide a somewhat human readable filenames, for easy debugging flow.
// 3. Provide reverse-mapping from filenames back to URLs.
// 4. Be able to distinguish http://x from http://x/ from http://x/index.html.
//    Those can all be different URLs.
// 5. Be able to represent http://a/b/c and http://a/b/c/d, a pattern seen
//    with Facebook Connect.
//
// We need an escape-character for representing characters that are legal
// in URL paths, but not in filenames, such as '?'.
//
// We can pick any legal character as an escape, as long as we escape it too.
// But as we have a goal of having filenames that humans can correlate with
// URLs, we should pick one that doesn't show up frequently in URLs. Candidates
// are ~`!@#$%^&()-=_+{}[],. but we would prefer to avoid characters that are
// treated specially by tools like shells or build tools.
// It turns out that , is neither frequent in URLs nor special anywhere else,
// so we use that.
//
// The escaping algorithm is:
//  1) Escape all unfriendly symbols as ,XX where XX is the hex code.
//  2) Add a ',' at the end (We do not allow ',' at end of any directory name,
//     so this assures that e.g. /a and /a/b can coexist in the filesystem).
//  3) Go through the path segment by segment (where a segment is one directory
//     or leaf in the path) and
//     3a) If the segment is empty, escape the second slash. i.e. if it was
//         www.foo.com//a then we escape the second / like www.foo.com/,2Fa,
//     3a) If it is "." or ".." prepend with ',' (so that we have a non-
//         empty and non-reserved filename).
//     3b) If it is over 128 characters, break it up into smaller segments by
//         inserting ,-/ (Windows limits paths to 128 chars, other OSes also
//         have limits that would restrict us)
//
// For example:
//     URL               File
//     /                 /,
//     /index.html       /index.html,
//     /.                /.,
//     /a/b              /a/b,
//     /a/b/             /a/b/,
//     /a/b/c            /a/b/c,   Note: no prefix problem
//     /u?foo=bar        /u,3Ffoo=bar,
//     //                /,2F,
//     /./               /,./,
//     /../              /,../,
//     /,                /,2C,
//     /,./              /,2C./,
//     /very...longname/ /very...long,-/name   If very...long is about 126 long.

#ifndef PAGESPEED_KERNEL_UTIL_URL_TO_FILENAME_ENCODER_H_
#define PAGESPEED_KERNEL_UTIL_URL_TO_FILENAME_ENCODER_H_

#include <cstddef>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Helper class for converting a URL into a filename.
class UrlToFilenameEncoder {
 public:
  // Encode a portion of URL to a form suitable for filenames.
  // |filename_prefix| is prepended without escaping.
  // |escaped_ending| is the URL to be encoded into a filename. It may have URL
  // escaped characters (like %21 for !).
  // |dir_separator| is "/" on Unix, "\" on Windows.
  // |encoded_filename| is the resultant filename.
  static void EncodeSegment(
      const StringPiece& filename_prefix,
      const StringPiece& escaped_ending,
      char dir_separator,
      GoogleString* encoded_filename);

  // Decodes a filename that was encoded with EncodeSegment with
  // dir_separator = '/', yielding back the original URL.
  //
  // Note: this decoder is not the exact inverse of the
  // UrlToFilenameEncoder::EncodeSegment, because it does not take
  // into account a prefix.
  static bool Decode(const StringPiece& encoded_filename,
                     GoogleString* decoded_url);


  static const char kEscapeChar;
  static const char kTruncationChar;
  static const size_t kMaximumSubdirectoryLength;

  friend class UrlToFilenameEncoderTest;

 private:
  // Appends a segment of the path, special-casing "." and "..", and
  // ensuring that the segment does not exceed the path length.  If it does,
  // it chops the end off the segment, writes the segment with a separator of
  // ",-/", and then rewrites segment to contain just the truncated piece so
  // it can be used in the next iteration.
  // |segment| is a read/write parameter containing segment to write
  // Note: this should not be called with empty segment.
  static void AppendSegment(
      GoogleString* segment,
      GoogleString* dest);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_URL_TO_FILENAME_ENCODER_H_
