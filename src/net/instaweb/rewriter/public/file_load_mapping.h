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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_MAPPING_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_MAPPING_H_

#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Class for storing a mapping from a URL to a filesystem path, for use by
// FileLoadPolicy.
class FileLoadMapping {
 public:
  virtual ~FileLoadMapping();

  // Creates a copy of the mapping.  Caller takes ownership.
  virtual FileLoadMapping* Clone() const = 0;

  // If this mapping applies to this url, put the mapped path into filename and
  // return true.  Otherwise return false.
  virtual bool Substitute(const StringPiece& url,
                          GoogleString* filename) const = 0;
};

class FileLoadMappingRegexp : public FileLoadMapping {
 public:
  FileLoadMappingRegexp(const GoogleString& url_regexp,
                        const GoogleString& filename_prefix)
      : url_regexp_(url_regexp),
        url_regexp_str_(url_regexp),
        filename_prefix_(filename_prefix) {}

  virtual FileLoadMapping* Clone() const;
  virtual bool Substitute(const StringPiece& url, GoogleString* filename) const;

 private:
  const RE2 url_regexp_;
  // RE2s can't be copied, so we need to keep the string around.
  const GoogleString url_regexp_str_;
  const GoogleString filename_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FileLoadMappingRegexp);
};

class FileLoadMappingLiteral : public FileLoadMapping {
 public:
  FileLoadMappingLiteral(const GoogleString& url_prefix,
                         const GoogleString& filename_prefix)
      : url_prefix_(url_prefix),
        filename_prefix_(filename_prefix) {}

  virtual FileLoadMapping* Clone() const;
  virtual bool Substitute(const StringPiece& url, GoogleString* filename) const;

 private:
  const GoogleString url_prefix_;
  const GoogleString filename_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FileLoadMappingLiteral);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_MAPPING_H_
