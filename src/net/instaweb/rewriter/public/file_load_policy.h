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
  // Currently tests against youngest association first in case of overlapping
  // prefixes. We may disallow overlapping prefixes in the future.
  virtual void Associate(const StringPiece& url_prefix,
                         const StringPiece& filename_prefix);

  // Merge in other policies (needed for rewrite_options).
  virtual void Merge(const FileLoadPolicy& other);

 private:
  struct UrlFilename {
    UrlFilename(const StringPiece& url_prefix_in,
                const StringPiece& filename_prefix_in)
        : url_prefix(url_prefix_in.data(), url_prefix_in.size()),
          filename_prefix(filename_prefix_in.data(), filename_prefix_in.size())
    {}

    GoogleString url_prefix;
    GoogleString filename_prefix;
  };
  // TODO(sligocki): This is not a very efficient way to store associations
  // if there are many. Write a better version. Perhaps a trie.
  typedef std::list<UrlFilename> UrlFilenames;
  UrlFilenames url_filenames_;

  FRIEND_TEST(FileLoadPolicyTest, Merge);

  DISALLOW_COPY_AND_ASSIGN(FileLoadPolicy);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_POLICY_H_
