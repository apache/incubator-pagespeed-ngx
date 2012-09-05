/*
 * Copyright 2011 Google Inc.
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

#include <cstdio>

#include "net/instaweb/automatic/public/static_rewriter.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class MessageHandler;

}  // namespace net_instaweb

// The purpose of this program is to help us test that pagespeed_automatic.a
// contains all that's needed to successfully link a rewriter using standard
// g++, without using the gyp flow.
int main(int argc, char** argv) {
  net_instaweb::ProcessContext process_context;
  net_instaweb::RewriteDriverFactory::Initialize();
  net_instaweb::StaticRewriter static_rewriter(&argc, &argv);

  // Having stripped all the flags, there should be exactly 3
  // arguments remaining:
  //
  //   input_directory:   The directory where the origin web site is stored
  //   output_directory:  The directory where the rewritten web site is written
  //   URL:               The URL of HTML to rewrite.
  if (argc != 4) {
    fprintf(stderr, "Usage: [options] %s input_dir output_dir url.\n", argv[0]);
    fprintf(stderr, "Type '%s --help' to see the options\n", argv[0]);
    return 1;
  }
  const char* input_dir = argv[1];
  const char* output_dir = argv[2];
  const char* html_name = argv[3];

  GoogleString url = net_instaweb::StrCat("http://test.com/", html_name);
  GoogleString input_file_path = net_instaweb::StrCat(input_dir, "/",
                                                      html_name);
  GoogleString output_file_path = net_instaweb::StrCat(output_dir, "/",
                                                       html_name);
  GoogleString html_input_buffer, html_output_buffer;
  net_instaweb::FileSystem* file_system = static_rewriter.file_system();
  net_instaweb::MessageHandler* message_handler =
      static_rewriter.message_handler();
  net_instaweb::StringWriter writer(&html_output_buffer);

  int exit_status = 1;
  if (!file_system->ReadFile(input_file_path.c_str(), &html_input_buffer,
                             message_handler)) {
    fprintf(stderr, "failed to read file %s\n", input_file_path.c_str());
  } else if (!static_rewriter.ParseText(url, input_file_path, html_input_buffer,
                                        output_dir, &writer)) {
    fprintf(stderr, "StartParseId failed on url %s\n", url.c_str());
  } else if (!file_system->WriteFile(
      output_file_path.c_str(), html_output_buffer, message_handler)) {
    fprintf(stderr, "failed to write file %s\n", output_file_path.c_str());
  } else {
    exit_status = 0;
  }

  // TODO(jmarantz): set up a file-based fetcher that will allow us to
  // rewrite resources in HTML files in this demonstration.

  net_instaweb::RewriteDriverFactory::Terminate();
  return 0;
}
