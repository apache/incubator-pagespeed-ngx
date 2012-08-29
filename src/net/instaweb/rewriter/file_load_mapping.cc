/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)
//
// Implementations of FileLoadMappingLiteral and FileLoadMappingRegexp, two
// subclasses of the abstract class FileLoadMapping.
//
// Tests are in file_load_policy_test.

#include "net/instaweb/rewriter/public/file_load_mapping.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

FileLoadMapping::~FileLoadMapping() {}

bool FileLoadMappingRegexp::Substitute(const StringPiece& url,
                                       GoogleString* filename) const {
  GoogleString potential_filename;
  url.CopyToString(&potential_filename);
  bool ok = RE2::Replace(&potential_filename, url_regexp_, filename_prefix_);
  if (ok) {
    filename->swap(potential_filename);  // Using swap() to avoid copying.
  }
  return ok;
}

bool FileLoadMappingLiteral::Substitute(const StringPiece& url,
                                        GoogleString* filename) const {
  if (url.starts_with(url_prefix_)) {
    // Replace url_prefix_ with filename_prefix_.
    StringPiece suffix = url.substr(url_prefix_.size());
    *filename = StrCat(filename_prefix_, suffix);
    return true;
  }
  return false;
}

}  // namespace net_instaweb
