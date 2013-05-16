/*
 * Copyright 2013 Google Inc.
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

// Author: Huibao Lin

#include "pagespeed/kernel/image/test_utils.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string_writer.h"

namespace pagespeed {

namespace image_compression {

bool ReadFile(const GoogleString& file_name,
              GoogleString* content) {
  net_instaweb::StdioFileSystem file_system;
  net_instaweb::MockMessageHandler message_handler;
  net_instaweb::StringWriter writer(content);
  return(file_system.ReadFile(file_name.c_str(), &writer, &message_handler));
}

bool ReadTestFile(const GoogleString& path,
                  const char* name,
                  const char* extension,
                  GoogleString* content) {
  content->clear();
  GoogleString file_name = net_instaweb::StrCat(net_instaweb::GTestSrcDir(),
       kTestRootDir, path, name, ".", extension);
  return ReadFile(file_name, content);
}

bool ReadTestFileWithExt(const GoogleString& path,
                         const char* name_with_extension,
                         GoogleString* content) {
  GoogleString file_name = net_instaweb::StrCat(net_instaweb::GTestSrcDir(),
       kTestRootDir, path, name_with_extension);
  return ReadFile(file_name, content);
}

}  // namespace image_compression

}  // namespace pagespeed
