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

// Author: sligocki@google.com (Shawn Ligocki)

#include <cstdio>
#include <cstdlib>

#include "net/instaweb/rewriter/public/css_minify.h"
#include "pagespeed/kernel/base/file_message_handler.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/file_writer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/gflags.h"

namespace net_instaweb {

bool MinifyCss_main(int argc, char** argv) {
  StdioFileSystem file_system;
  FileMessageHandler handler(stderr);
  FileSystem::OutputFile* error_file = file_system.Stderr();

  // Load command line args.
  static const char kUsage[] = "Usage: css_minify infilename\n";
  if (argc != 2) {
    error_file->Write(kUsage, &handler);
    return false;
  }

  const char* infilename = argv[1];

  // Read text from file.
  GoogleString in_text;
  if (!file_system.ReadFile(infilename, &in_text, &handler)) {
    error_file->Write(StringPrintf(
        "Failed to read input file %s\n", infilename), &handler);
    return false;
  }

  FileWriter writer(file_system.Stdout());
  FileWriter error_writer(error_file);
  CssMinify minify(&writer, &handler);
  minify.set_error_writer(&error_writer);
  return minify.ParseStylesheet(in_text);
}

}  // namespace net_instaweb

int main(int argc, char** argv) {
  net_instaweb::ParseGflags(argv[0], &argc, &argv);
  return net_instaweb::MinifyCss_main(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
