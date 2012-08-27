/*
 * Copyright 2011 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_POLICY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_POLICY_H_

#include <list>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class FileLoadMapping;

// Class for deciding which URLs get loaded from which files.
//
// Currently, you must explicitly set which directories to load directly
// from filesystem.
class FileLoadPolicy {
 public:
  FileLoadPolicy() {}
  virtual ~FileLoadPolicy();

  // Note: This is O(N) for N is number of calls to Associate.
  // TODO(sligocki): Set up a more efficient mapper.
  virtual bool ShouldLoadFromFile(const GoogleUrl& url,
                                  GoogleString* filename) const;

  // Tells us to load all URLs with this prefix from filename_prefix directory.
  // Both prefixes must specify directories, if they do not end in slashes,
  // we add them.
  //
  // Tests against youngest association first in case of overlapping prefixes.
  // Because we support regular expressions, checking for overlapping prefixes
  // isn't practical.
  virtual void Associate(const StringPiece& url_prefix,
                         const StringPiece& filename_prefix);

  // A version of Associate supporting RE2-format regular expressions.
  // Backreferences are supported, as in:
  //
  //   AssociateRegexp("^https?://example.com/~([^/]*)/static/",
  //                   "/var/static/\\1", &error);
  //
  // Which will map urls as:
  //
  //   http://example.com/~pat/static/cat.jpg -> /var/static/pat/cat.jpg
  //   http://example.com/~sam/static/dog.jpg -> /var/static/sam/dog.jpg
  //   https://example.com/~al/static/css/ie -> /var/static/al/css/ie
  //
  // If the regular expression and substitution validate, returns true.
  // Otherwise it writes a message to error and returns false.
  virtual bool AssociateRegexp(const StringPiece& url_regexp,
                               const StringPiece& filename_prefix,
                               GoogleString* error);

  // Merge in other policies (needed for rewrite_options).
  virtual void Merge(const FileLoadPolicy& other);

 private:
  typedef std::list<FileLoadMapping*> FileLoadMappings;
  FileLoadMappings file_load_mappings_;

  DISALLOW_COPY_AND_ASSIGN(FileLoadPolicy);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_POLICY_H_
