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

FileLoadMapping* FileLoadMappingRegexp::Clone() const {
  // TODO(jefftk): This recompiles the RE2.  On 20120 hardware
  // http://swtch.com/~rsc/regexp/regexp3.html has benchmarks indicating that
  // RE2 compilation is 10-20 microseconds.  Clone() runs for every regexp for
  // every RewriteOptions.Clone().  In cases where the RewriteOptions are cloned
  // on every request, for example Apache with .htaccess files or when running
  // an experiment, this means 10-20us per regexp per request.  This is enough
  // that reference-counting to avoid this recompilation should be worth it.
  return new FileLoadMappingRegexp(url_regexp_str_, filename_prefix_);
}

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

FileLoadMapping* FileLoadMappingLiteral::Clone() const {
  return new FileLoadMappingLiteral(url_prefix_, filename_prefix_);
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
