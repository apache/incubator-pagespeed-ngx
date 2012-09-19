/*
 * Copyright 2012 Google Inc.
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
// Author: jmaessen@google.com (Jan Maessen)

#include <cstdio>
#include <cstdlib>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_message_handler.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/js/js_minify.h"

// Command-line javascript minifier and metadata printer.  Takes a single
// javascript file as either standard input or a command-line argument, and by
// default prints the minified code for that file to stdout.  If
// --print_size_and_hash is specified, it instead prints the size of the
// minified file (in bytes) and its minified md5 sum, suitable for configuring
// library recognition in mod_pagespeed.

namespace net_instaweb {

DEFINE_bool(print_size_and_hash, false,
            "Instead of printing minified JavaScript, print the size "
            "and url-encoded md5 checksum of the minified input.  "
            "This yields results suitable for a "
            "ModPagespeedLibrary directive.");

namespace {

bool JSMinifyMain(int argc, char** argv) {
  net_instaweb::FileMessageHandler handler(stderr);
  net_instaweb::StdioFileSystem file_system;
  if (argc >= 3) {
    handler.Message(kError,
                    "Usage: \n"
                    "  js_minify [--print_size_and_hash] foo.js\n"
                    "  js_minify [--print_size_and_hash] < foo.js\n"
                    "Without --print_size_and_hash prints minified foo.js\n"
                    "With --print_size_and_hash instead prints minified "
                    "size and content hash suitable for ModPagespeedLibrary\n");
    return false;
  }
  // Choose stdin if no file name on command line.
  const char* filename = "<stdin>";
  FileSystem::InputFile* input = file_system.Stdin();
  if (argc == 2) {
    filename = argv[1];
    input = file_system.OpenInputFile(filename, &handler);
  }
  // Just read and process the input in bulk.
  GoogleString original;
  if (!file_system.ReadFile(input, &original, &handler)) {
    return false;
  }
  GoogleString stripped;
  if (!pagespeed::js::MinifyJs(original, &stripped)) {
    handler.Message(kError,
                    "%s: Couldn't minify; "
                    "stripping leading and trailing whitespace.\n",
                    filename);
    TrimWhitespace(original, &stripped);
  }
  FileSystem::OutputFile* stdout = file_system.Stdout();
  if (FLAGS_print_size_and_hash) {
    MD5Hasher hasher;
    uint64 size = stripped.size();
    bool ret = stdout->Write(Integer64ToString(size), &handler);
    ret &= stdout->Write(" ", &handler);
    ret &= stdout->Write(hasher.Hash(stripped), &handler);
    return ret;
  } else {
    return stdout->Write(stripped, &handler);
  }
}

}  // namespace

}  // namespace net_instaweb

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  return net_instaweb::JSMinifyMain(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
