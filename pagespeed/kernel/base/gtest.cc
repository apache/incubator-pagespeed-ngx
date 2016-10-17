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

#include "pagespeed/kernel/base/gtest.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>  // For getpid()
#include <vector>
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string_util.h"

#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

GoogleString GTestSrcDir() {
  char cwd[kStackBufferSize];
  CHECK(getcwd(cwd, sizeof(cwd)) != NULL);

  // This needs to return the root of the git checkout. In practice all the
  // tests are run automatically from there, so we just stat a few directories
  // to make sure it looks good and return getcwd(). An alternative might
  // be to return the value of $(git rev-parse --show-toplevel).

  bool found = true;
  for (const char* dir : {"third_party", "pagespeed"}) {
    struct stat file_info;
    int ret = stat(dir, &file_info);
    if (ret != 0 || !S_ISDIR(file_info.st_mode)) {
      found = false;
    }
  }
  CHECK(found) << "You must run this test from the root of the checkout";
  return cwd;
}

GoogleString GTestTempDir() {
  return StringPrintf("/tmp/gtest.%d", getpid());
}


}  // namespace net_instaweb
