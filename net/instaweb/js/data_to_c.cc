// Copyright 2011 Google Inc. All Rights Reserved.
// Author: atulvasu@google.com (Atul Vasu)

#include <algorithm>
#include <cstdlib>

#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/gflags.h"

DEFINE_string(data_file, "/tmp/a.js", "Input data file");
DEFINE_string(c_file, "/tmp/a.c", "Output C file");
DEFINE_string(varname, "str", "Variable name.");

const char kOutputTemplate[] =
    "// Copyright 2011 Google Inc. All Rights Reserved.\n"
    "//\n"
    "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
    "// you may not use this file except in compliance with the License.\n"
    "// You may obtain a copy of the License at\n"
    "//\n"
    "//      http://www.apache.org/licenses/LICENSE-2.0\n"
    "//\n"
    "// Unless required by applicable law or agreed to in writing, software\n"
    "// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
    "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied"
    ".\n"
    "// See the License for the specific language governing permissions and\n"
    "// limitations under the License.\n"
    "\n"
    "// Automatically generated from %s\n"
    "\n"
    "namespace net_instaweb {\n"
    "\n"
    "const char* %s =%s;\n"
    "\n"
    "}  // namespace net_instaweb\n";

namespace net_instaweb {

bool DataToC(int argc, char* argv[]) {
  ParseGflags(argv[0], &argc, &argv);
  NullMessageHandler handler;
  StdioFileSystem file_system;

  GoogleString input;
  file_system.ReadFile(FLAGS_data_file.c_str(), &input, &handler);

  // Transform input file contents into escaped C string.
  GoogleString joined = "";
  for (size_t i = 0; i < input.size(); i += 60) {
    // substr says if length exceeds it takes chars till end of string.
    GoogleString part = input.substr(i, 60);
    StrAppend(&joined, "\n    \"", CEscape(part), "\"");
  }
  GoogleString output = StringPrintf(kOutputTemplate, FLAGS_data_file.c_str(),
      FLAGS_varname.c_str(), joined.c_str());

  file_system.RemoveFile(FLAGS_c_file.c_str(), &handler);
  return file_system.WriteFileAtomic(FLAGS_c_file, output, &handler);
}

}  // namespace net_instaweb

int main(int argc, char* argv[]) {
  if (net_instaweb::DataToC(argc, argv)) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}
