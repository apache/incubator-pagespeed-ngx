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

#include "net/instaweb/rewriter/public/javascript_library_identification.h"

#include <cstddef>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/rolling_hash.h"
#include "net/instaweb/util/public/string_util.h"

// Include separately-generated library metadata (kLibraryMetadata)
// whose 0th entry is unrecognized (NULL name).
#include "net/instaweb/rewriter/javascript_metadata.cc"

namespace net_instaweb {

static const LibraryInfo& kUnrecognizedLibraryInfo = kLibraryMetadata[0];
static const size_t kLibrarySize = arraysize(kLibraryMetadata);

JavascriptLibraryId JavascriptLibraryId::Find(
    const StringPiece& minified_code) {
  const LibraryInfo* library = &kUnrecognizedLibraryInfo;
  if (minified_code.size() >= kJavascriptHashIdBlockSize) {
    uint64 block_hash =
        RollingHash(minified_code.data(), 0, kJavascriptHashIdBlockSize);
    // Right now we use a naive linear search.
    // TODO(jmaessen): lazily-initialized search structure of some sort.
    for (library = &kLibraryMetadata[kLibrarySize - 1];
         library->name != NULL; --library) {
      if (library->first_block_hash == block_hash &&
          library->full_size == minified_code.size() &&
          library->full_hash ==
          RollingHash(minified_code.data(), 0, library->full_size)) {
        break;
      }
    }
  }
  return JavascriptLibraryId(library);
}

JavascriptLibraryId::JavascriptLibraryId()
    : info_(&kUnrecognizedLibraryInfo) { }

}  // namespace net_instaweb
