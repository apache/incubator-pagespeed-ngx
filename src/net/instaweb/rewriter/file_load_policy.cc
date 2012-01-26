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

#include "net/instaweb/rewriter/public/file_load_policy.h"

#include <list>

#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

FileLoadPolicy::~FileLoadPolicy() {}

bool FileLoadPolicy::ShouldLoadFromFile(const GoogleUrl& url,
                                        GoogleString* filename) const {
  if (!url.is_valid()) {
    return false;
  }

  StringPiece url_string = url.AllExceptQuery();
  if (!url_string.empty()) {
    // TODO(sligocki): Consider layering a cache over this lookup.
    // Note: Later associations take precedence over earlier ones.
    for (UrlFilenames::const_reverse_iterator iter = url_filenames_.rbegin();
         iter != url_filenames_.rend(); ++iter) {
      if (url_string.starts_with(iter->url_prefix)) {
        // Replace url_prefix_ with filename_prefix_.
        StringPiece suffix = url_string.substr(iter->url_prefix.size());
        *filename = StrCat(iter->filename_prefix, suffix);
        // GoogleUrl will decode most %XX escapes, but it does not convert
        // "%20" -> " " which has come up often.
        GlobalReplaceSubstring("%20", " ", filename);
        return true;
      }
    }
  }

  return false;
}

void FileLoadPolicy::Associate(const StringPiece& url_prefix_in,
                               const StringPiece& filename_prefix_in) {
  GoogleString url_prefix(url_prefix_in.data(), url_prefix_in.size());
  GoogleString filename_prefix(filename_prefix_in.data(),
                               filename_prefix_in.size());

  // Make sure these are directories.
  EnsureEndsInSlash(&url_prefix);
  EnsureEndsInSlash(&filename_prefix);

  // TODO(sligocki): Should fail if filename_prefix doesn't start with '/'?

  url_filenames_.push_back(UrlFilename(url_prefix, filename_prefix));
}

void FileLoadPolicy::Merge(const FileLoadPolicy& other) {
  UrlFilenames::const_iterator iter;
  for (iter = other.url_filenames_.begin();
       iter != other.url_filenames_.end(); ++iter) {
    // Copy associations over.
    url_filenames_.push_back(*iter);
  }
}

}  // namespace net_instaweb
