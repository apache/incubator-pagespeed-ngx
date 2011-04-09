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

#include <unistd.h>  // For getpid()

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/stack_buffer.h"

#include "third_party/gflags/src/google/gflags.h"

namespace net_instaweb {


GoogleString GTestSrcDir() {
  // Climb up the directory hierarchy till we find "src".
  // TODO(jmarantz): check to make sure we are not in a subdirectory of
  // our top-level 'src' named src.

  char cwd[kStackBufferSize];
  CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
  std::vector<StringPiece> components;
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

// We must auto-destruct the command-line flags after all tests are
// complete, otherwise valgrind will complain about memory leaks.  There
// is not an obvious place to do this in the general testing
// framework, so we need to use a static object destructor to trigger
// this.
class GTestProcessContext {
 public:
  ~GTestProcessContext() {
    google::ShutDownCommandLineFlags();
  }
};
GTestProcessContext gtest_process_context;


}  // namespace net_instaweb
