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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_LIBRARY_IDENTIFICATION_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_LIBRARY_IDENTIFICATION_H_

#include <map>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Holds the data necessary to identify the canonical urls of a set of known
// javascript libraries.  We identify a library as "known" on the basis of its
// minified size in bytes and the web64-encoded md5 hash of its minified code.
// Minification allows us to tolerate changes in whitespace and the mix of
// minified and unminified versions of library code one sees served in the wild.
class JavascriptLibraryIdentification {
 public:
  typedef uint64 SizeInBytes;

  JavascriptLibraryIdentification() { }
  ~JavascriptLibraryIdentification();

  // Register a library for recognition.  False indicates badly-formed hash or
  // url.
  bool RegisterLibrary(
      SizeInBytes bytes, StringPiece md5_hash, StringPiece canonical_url);
  // Find canonical url of library; empty string if none.  Storage for url is
  // owned by the JavascriptLibraryIdentification object.
  StringPiece Find(StringPiece minified_code) const;
  // Merge libraries recognized by src into this one.
  void Merge(const JavascriptLibraryIdentification& src);
  // Append a signature for the libraries recognized to *signature.
  void AppendSignature(GoogleString* signature) const;

 private:
  class LibraryInfo;
  // We map minified file size to LibraryInfo (we expect few libraries to have
  // the same size in bytes, and this permits us to avoid computing the content
  // hash if we don't actually require it).
  typedef GoogleString MD5Signature;
  typedef std::map<MD5Signature, GoogleString> MD5ToUrlMap;
  typedef std::map<SizeInBytes, MD5ToUrlMap> LibraryMap;

  LibraryMap libraries_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptLibraryIdentification);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_LIBRARY_IDENTIFICATION_H_
