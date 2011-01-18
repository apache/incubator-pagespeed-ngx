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

#include <stdio.h>
#include "net/instaweb/htmlparse/public/file_driver.h"
#include "net/instaweb/util/public/file_message_handler.h"
#include "net/instaweb/htmlparse/public/file_statistics_log.h"
#include "net/instaweb/util/public/stdio_file_system.h"

int null_filter(int argc, char** argv) {
  int ret = 1;

  if ((argc < 2) || (argc > 4)) {
    fprintf(stdout, "Usage: %s input_file [- | output_file] [log_file]\n",
            argv[0]);
    return ret;
  }

  const char* infile = argv[1];
  net_instaweb::FileMessageHandler message_handler(stderr);
  net_instaweb::StdioFileSystem file_system;
  net_instaweb::HtmlParse html_parse(&message_handler);
  net_instaweb::FileDriver file_driver(&html_parse, &file_system);
  const char* outfile = NULL;
  std::string outfile_buffer;
  const char* statsfile = NULL;
  std::string statsfile_buffer;

  if (argc >= 3) {
    outfile = argv[2];
  } else if (net_instaweb::FileDriver::GenerateOutputFilename(
                  infile, &outfile_buffer)) {
    outfile = outfile_buffer.c_str();
    fprintf(stdout, "Null rewriting %s into %s\n", infile, outfile);
  } else {
    message_handler.FatalError(infile, 0, "Cannot generate output filename");
  }

  if (argc >= 4) {
    statsfile = argv[3];
  } else if (net_instaweb::FileDriver::GenerateStatsFilename(
                 infile, &statsfile_buffer)) {
    statsfile = statsfile_buffer.c_str();
    fprintf(stdout, "Logging statistics for %s into %s\n",
            infile, statsfile);
  } else {
    message_handler.FatalError(infile, 0, "Cannot generate stats file name");
  }

  if (file_driver.ParseFile(infile, outfile, statsfile, &message_handler)) {
    ret = 0;
  }

  return ret;
}
