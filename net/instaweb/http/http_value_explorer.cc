/*
 * Copyright 2015 Google Inc.
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

#include <cstdlib>
#include <iostream>

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/gflags.h"
#include "pagespeed/kernel/base/google_message_handler.h"


namespace net_instaweb {

namespace {

bool HttpValueExplorerMain(int argc, char** argv) {
  const char kUsage[] =
      "Usage: http_value_explorer (encode|decode) infilename\n";
  ParseGflags(kUsage, &argc, &argv);

  if (argc != 3) {
    std::cerr << kUsage;
    return false;
  }

  StdioFileSystem file_system;
  GoogleMessageHandler handler;
  const char* input_filename = argv[2];
  GoogleString input;
  CHECK(file_system.ReadFile(input_filename, &input, &handler))
      << "Failed to read input file " << input_filename;

  StringPiece type(argv[1]);
  if (type == "encode") {
    GoogleString output;
    CHECK(HTTPValue::Encode(input, &output, &handler)) << "Invalid http";
    std::cout << output;

  } else if (type == "decode") {
    GoogleString output;
    CHECK(HTTPValue::Decode(input, &output, &handler)) << "Invalid encoding";
    std::cout << output;

  } else {
    std::cerr << kUsage;
    return false;
  }

  return true;
}

}  // namespace

}  // namespace net_instaweb


int main(int argc, char** argv) {
  return net_instaweb::HttpValueExplorerMain(argc, argv)
      ? EXIT_SUCCESS : EXIT_FAILURE;
}
