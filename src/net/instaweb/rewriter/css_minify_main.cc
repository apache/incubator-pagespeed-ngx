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
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/util/public/file_message_handler.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "webutil/css/parser.h"

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

  // Parse CSS.
  Css::Parser parser(in_text);
  parser.set_preservation_mode(true);
  scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

  // Report error summary.
  if (parser.errors_seen_mask() != Css::Parser::kNoError) {
    error_file->Write(StringPrintf(
        "CSS parsing error mask %s\n",
        Integer64ToString(parser.errors_seen_mask()).c_str()), &handler);
  }
  if (parser.unparseable_sections_seen_mask() != Css::Parser::kNoError) {
    error_file->Write(StringPrintf(
        "CSS unparseable sections mask %s\n",
        Integer64ToString(parser.unparseable_sections_seen_mask()).c_str()),
                      &handler);
  }
  // Report individual errors.
  for (int i = 0, n = parser.errors_seen().size(); i < n; ++i) {
      Css::Parser::ErrorInfo error = parser.errors_seen()[i];
      error_file->Write(error.message, &handler);
      error_file->Write("\n", &handler);
  }

  // Re-serialize.
  // TODO(sligocki): Allow to be an actual file?
  FileSystem::OutputFile* outfile = file_system.Stdout();
  FileWriter writer(outfile);
  bool written = CssMinify::Stylesheet(*stylesheet, &writer, &handler);

  return written && (parser.errors_seen_mask() == Css::Parser::kNoError);
}

}  // namespace net_instaweb

int main(int argc, char** argv) {
  net_instaweb::ParseGflags(argv[0], &argc, &argv);
  return net_instaweb::MinifyCss_main(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
