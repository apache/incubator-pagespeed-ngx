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

#include <cstddef>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Size of regions that is hashed to do initial detection
const size_t kJavascriptHashIdBlockSize = 512;

// Internal data used by JavascriptLibraryId.
// We expose it here to allow the metadata to be
// constructed sensibly by generate_javascript_metadata.
struct LibraryInfo {
  const char* name;
  const char* version;
  uint64 first_block_hash;
  uint64 full_hash;
  size_t full_size;
};

// A JavascriptLibraryId contains information about a single third-party
// Javascript library.  Given a block of minified Javascript with leading and
// trailing whitespace removed, the Find(...) method returns a corresponding
// JavascriptLibraryId.  Note that JavascriptLibraryId is intended to be passed
// by value.
class JavascriptLibraryId {
 public:
  // Constructs an unrecognized library.
  JavascriptLibraryId();

  // Find the JavascriptLibraryId object associated with the given
  // minified_code.  This might be an unrecognized library.
  static JavascriptLibraryId Find(const StringPiece& minified_code);

  // Is this a recognized library?  Otherwise we should ignore it.
  bool recognized() const {
    return info_->name != NULL;
  }

  // Canonical name of javascript library (NULL if unrecognized)
  const char* name() const {
    return info_->name;
  }

  // Version number of javascript library (NULL if unrecognized)
  const char* version() const {
    return info_->version;
  }

 private:
  explicit JavascriptLibraryId(const LibraryInfo* info) : info_(info) { }

  const LibraryInfo* info_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_LIBRARY_IDENTIFICATION_H_
