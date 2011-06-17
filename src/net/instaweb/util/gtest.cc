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

// Author: jmarantz@google.com (Joshua Marantz)
//

#include "net/instaweb/util/public/gtest.h"
//#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/string.h"
#include <unistd.h>  // For getpid()
#include <vector>
#include "net/instaweb/util/stack_buffer.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {


GoogleString GTestSrcDir() {
  // Climb up the directory hierarchy till we find "src".
  // TODO(jmarantz): check to make sure we are not in a subdirectory of
  // our top-level 'src' named src.

  char cwd[kStackBufferSize];
  CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
  StringPieceVector components;
  SplitStringPieceToVector(cwd, "/", &components, true);
  int level = components.size();
  bool found = false;
  for (int i = level - 1; i >= 0; --i) {
    if (components[i] == "src") {
      level = i + 1;
      found = true;
      break;
    }
  }

  CHECK(found) << "Cannot find 'src' directory from cwd=" << cwd;
  GoogleString src_dir;
  for (int i = 0; i < level; ++i) {
    src_dir += "/";
    components[i].AppendToString(&src_dir);
  }
  return src_dir;
}

GoogleString GTestTempDir() {
  return StringPrintf("/tmp/gtest.%d", getpid());
}


}  // namespace net_instaweb
