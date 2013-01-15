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

// As with url_canon_internal.h, this file is intended to be included in
// another C++ file where the template types are defined. This allows the
// programmer to use this to use these functions for their own strings
// types, without bloating the code by having inline templates used in
// every call site.
//
// *** This file must be included after url_canon_internal as we depend on some
// functions in it. ***

#ifndef GOOGLEURL_SRC_URL_CANON_INTERNAL_FILE_H__
#define GOOGLEURL_SRC_URL_CANON_INTERNAL_FILE_H__

#include "googleurl/src/url_file.h"
#include "googleurl/src/url_parse_internal.h"

using namespace url_canon;

// Given a pointer into the spec, this copies and canonicalizes the drive
// letter and colon to the output, if one is found. If there is not a drive
// spec, it won't do anything. The index of the next character in the input
// spec is returned (after the colon when a drive spec is found, the begin
// offset if one is not).
template<typename CHAR>
static int FileDoDriveSpec(const CHAR* spec, int begin, int end,
                           CanonOutput* output) {
  // The path could be one of several things: /foo/bar, c:/foo/bar, /c:/foo,
  // (with backslashes instead of slashes as well).
  int num_slashes = CountConsecutiveSlashes(spec, begin, end);
  int after_slashes = begin + num_slashes;

  if (!DoesBeginWindowsDriveSpec(spec, after_slashes, end))
    return begin;  // Haven't consumed any characters

  // DoesBeginWindowsDriveSpec will ensure that the drive letter is valid
  // and that it is followed by a colon/pipe.

  // Normalize Windows drive letters to uppercase
  if (spec[after_slashes] >= 'a' && spec[after_slashes] <= 'z')
    output->push_back(spec[after_slashes] - 'a' + 'A');
  else
    output->push_back(static_cast<char>(spec[after_slashes]));

  // Normalize the character following it to a colon rather than pipe.
  output->push_back(':');
  output->push_back('/');
  return after_slashes + 2;
}

// FileDoDriveSpec will have already added the first backslash, so we need to
// write everything following the slashes using the path canonicalizer.
template<typename CHAR, typename UCHAR>
static void FileDoPath(const CHAR* spec, int begin, int end,
                       CanonOutput* output) {
  // Normalize the number of slashes after the drive letter. The path
  // canonicalizer expects the input to begin in a slash already so
  // doesn't check. We want to handle no-slashes
  int num_slashes = CountConsecutiveSlashes(spec, begin, end);
  int after_slashes = begin + num_slashes;

  // Now use the regular path canonicalizer to canonicalize the rest of the
  // path. We supply it with the path following the slashes. It won't prepend
  // a slash because it assumes any nonempty path already starts with one.
  // We explicitly filter out calls with no path here to prevent that case.
  ParsedURL::Component sub_path(after_slashes, end - after_slashes);
  if (sub_path.len > 0) {
    // Give it a fake output component to write into. DoCanonicalizeFile will
    // compute the full path component.
    ParsedURL::Component fake_output_path;
    URLCanonInternal<CHAR, UCHAR>::DoPath(
        spec, sub_path, output, &fake_output_path);
  }
}

template<typename CHAR, typename UCHAR>
static bool DoCanonicalizeFileURL(const URLComponentSource<CHAR>& source,
                                  const ParsedURL& parsed,
                                  CanonOutput* output,
                                  ParsedURL* new_parsed) {
  // Things we don't set in file: URLs.
  new_parsed->username = ParsedURL::Component(0, -1);
  new_parsed->password = ParsedURL::Component(0, -1);
  new_parsed->port = ParsedURL::Component(0, -1);

  // Scheme (known, so we don't bother running it through the more
  // complicated scheme canonicalizer).
  new_parsed->scheme.begin = output->length();
  output->push_back('f');
  output->push_back('i');
  output->push_back('l');
  output->push_back('e');
  new_parsed->scheme.len = output->length() - new_parsed->scheme.begin;
  output->push_back(':');

  // Write the separator for the host.
  output->push_back('/');
  output->push_back('/');

  // Append the host. For many file URLs, this will be empty. For UNC, this
  // will be present.
  // TODO(brettw) This doesn't do any checking for host name validity. We
  // should probably handle validity checking of UNC hosts differently than
  // for regular IP hosts.
  bool success = URLCanonInternal<CHAR, UCHAR>::DoHost(
      source.host, parsed.host, output, &new_parsed->host);

  // Write a separator for the start of the path. We'll ignore any slashes
  // already at the beginning of the path.
  new_parsed->path.begin = output->length();
  output->push_back('/');

  // Copies and normalizes the "c:" at the beginning, if present.
  int after_drive = FileDoDriveSpec(source.path, parsed.path.begin,
                                    parsed.path.end(), output);

  // Copies the rest of the path
  FileDoPath<CHAR, UCHAR>(source.path, after_drive, parsed.path.end(), output);
  new_parsed->path.len = output->length() - new_parsed->path.begin;

  // Things following the path we can use the standard canonicalizers for.
  success &= URLCanonInternal<CHAR, UCHAR>::DoQuery(
      source.query, parsed.query, output, &new_parsed->query);
  success &= URLCanonInternal<CHAR, UCHAR>::DoRef(
      source.ref, parsed.ref, output, &new_parsed->ref);

  return success;
}

#endif  // GOOGLEURL_SRC_URL_CANON_INTERNAL_FILE_H__
