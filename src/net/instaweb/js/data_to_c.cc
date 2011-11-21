// Copyright 2011 Google Inc. All Rights Reserved.
// Author: atulvasu@google.com (Atul Vasu)

#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

using namespace google;  // NOLINT

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
    "const char* %s =\n"
    "    %s;\n"
    "\n"
    "}  // namespace net_instaweb\n";

namespace net_instaweb {

void DataToC(int argc, char* argv[]) {
  ParseCommandLineFlags(&argc, &argv, true);
  NullMessageHandler handler;
  StdioFileSystem file_system;
  GoogleString input;
  file_system.ReadFile(FLAGS_data_file.c_str(), &input, &handler);
  GoogleString escaped("\"" + CEscape(input) + "\"");
  std::vector<GoogleString> result;
  SplitStringUsingSubstr(escaped, "\\n", &result);
  GoogleString joined = result.empty() ? "" : result[0];
  for (size_t i = 1; i < result.size(); ++i) {
    StrAppend(&joined, "\\n\"\n    \"", result[i]);
  }
  GoogleString output = StringPrintf(kOutputTemplate, FLAGS_data_file.c_str(),
      FLAGS_varname.c_str(), joined.c_str());
  file_system.WriteFile(FLAGS_c_file.c_str(), output, &handler);
}

}  // namespace net_instaweb

int main(int argc, char* argv[]) {
  net_instaweb::DataToC(argc, argv);
}
