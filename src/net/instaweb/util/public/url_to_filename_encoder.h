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
// shell escapes or that blaze or g4 do not like.
//
// .#&%-=_+ occur frequently in URLs.
// <>:"/\|?* are illegal in Windows
//   See http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
// ~`!$^&(){}[]'; are special to Unix shells
// In addition, blaze does not like ^@  Perforce does not like #%
//
// Josh took a quick look at the frequency of some special characters in
// Sadeesh's slurped directory from Fall 09 and found the following occurrences:
//
//   ^   3               blaze doesn't like ^ in testdata filenames
//   @   10              blaze doesn't like @ in testdata filenames
//   .   1676            too frequent in URLs
//   ,   76              THE WINNER
//   #   0               g4 doesn't like it
//   &   487             Prefer to avoid shell escapes
//   %   374             g4 doesn't like it
//   =   579             very frequent in URLs -- leave unmodified
//   -   464             very frequent in URLs -- leave unmodified
//   _   798             very frequent in URLs -- leave unmodified
//
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

// TODO(sligocki): Strip out code we don't use.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_URL_TO_FILENAME_ENCODER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_URL_TO_FILENAME_ENCODER_H_

#include <cstddef>

#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Helper class for converting a URL into a filename.
class UrlToFilenameEncoder {
 public:
  // Given a |url| and a |base_path|, returns a filename which represents this
  // |url|. |url| may include URL escaping such as %21 for !
  // |legacy_escape| indicates that this function should use the old-style
  // of encoding.
  // TODO(mbelshe): delete the legacy_escape code.
  static GoogleString Encode(const GoogleString& url, GoogleString base_path,
                             bool legacy_escape) {
    GoogleString filename;
    if (!legacy_escape) {
      GoogleString url_no_scheme = GetUrlHostPath(url);
      EncodeSegment(base_path, url_no_scheme, '/', &filename);
#ifdef WIN32
      ReplaceAll(&filename, "/", "\\");
#endif
    } else {
      GoogleString clean_url(url);
      if (clean_url.length() && clean_url[clean_url.length()-1] == '/')
        clean_url.append("index.html");

      GoogleString host = GetUrlHost(clean_url);
      filename.append(base_path);
      filename.append(host);
#ifdef WIN32
      filename.append("\\");
#else
      filename.append("/");
#endif

      GoogleString url_filename = GetUrlPath(clean_url);
      // Strip the leading '/'.
      if (url_filename[0] == '/')
        url_filename = url_filename.substr(1);

      // Replace '/' with '\'.
      ConvertToSlashes(&url_filename);

      // Strip double back-slashes ("\\\\").
      StripDoubleSlashes(&url_filename);

      // Save path as filesystem-safe characters.
      url_filename = LegacyEscape(url_filename);
      filename.append(url_filename);

#ifndef WIN32
      // Last step - convert to native slashes.
      const GoogleString slash("/");
      const GoogleString backslash("\\");
      ReplaceAll(&filename, backslash, slash);
#endif
    }

    return filename;
  }

  // Rewrite HTML in a form that the SPDY in-memory server
  // can read.
  // |filename_prefix| is prepended without escaping.
  // |escaped_ending| is the URL to be encoded into a filename. It may have URL
  // escaped characters (like %21 for !).
  // |dir_separator| is "/" on Unix, "\" on Windows.
  // |encoded_filename| is the resultant filename.
  static void EncodeSegment(
      const GoogleString& filename_prefix,
      const GoogleString& escaped_ending,
      char dir_separator,
      GoogleString* encoded_filename);

  // Decodes a filename that was encoded with EncodeSegment,
  // yielding back the original URL.
  static bool Decode(const GoogleString& encoded_filename,
                     char dir_separator,
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

  // Allow reading of old slurped files.
  static GoogleString LegacyEscape(const GoogleString& path);

  // Replace all instances of |from| within |str| as |to|.
  static void ReplaceAll(GoogleString* str, const GoogleString& from,
                         const GoogleString& to) {
    GoogleString::size_type pos(0);
    while ((pos = str->find(from, pos)) != GoogleString::npos) {
      str->replace(pos, from.size(), to);
      pos += from.size();
    }
  }

  // Replace all instances of "/" with "\" in |path|.
  static void ConvertToSlashes(GoogleString* path) {
    const GoogleString slash("/");
    const GoogleString backslash("\\");
    ReplaceAll(path, slash, backslash);
  }

  // Replace all instances of "\\" with "%5C%5C" in |path|.
  static void StripDoubleSlashes(GoogleString* path) {
    const GoogleString doubleslash("\\\\");
    const GoogleString escaped_doubleslash("%5C%5C");
    ReplaceAll(path, doubleslash, escaped_doubleslash);
  }

  // Get the host from an url, strip the port number as well if the url
  // has one.
  // For example: calling GetUrlHost(www.foo.com:8080/boo) returns www.foo.com
  static GoogleString GetUrlHost(const GoogleString& url);

  // Get the host + path portion of an url
  // e.g   http://www.foo.com/path
  //       returns www.foo.com/path
  static GoogleString GetUrlHostPath(const GoogleString& url);

  // Get the path portion of an url
  // e.g   http://www.foo.com/path
  //       returns /path
  static GoogleString GetUrlPath(const GoogleString& url);

  // Unescape a url, converting all %XX to the the actual char 0xXX.
  // For example, this will convert "foo%21bar" to "foo!bar".
  //
  // This will work with strings that have embedded NULLs.
  static GoogleString Unescape(const GoogleString& escaped_url);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_URL_TO_FILENAME_ENCODER_H_
