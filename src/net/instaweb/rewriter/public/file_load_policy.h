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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class GoogleUrl;

// Class for deciding which URLs get loaded from which files.
// Default implementation never loads from files.
class FileLoadPolicy {
 public:
  FileLoadPolicy() {}
  virtual ~FileLoadPolicy();

  virtual bool ShouldLoadFromFile(const GoogleUrl& url,
                                  GoogleString* filename) const;
 private:
  DISALLOW_COPY_AND_ASSIGN(FileLoadPolicy);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_POLICY_H_
