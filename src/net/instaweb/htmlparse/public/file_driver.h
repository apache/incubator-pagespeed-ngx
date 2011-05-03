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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_FILE_DRIVER_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_FILE_DRIVER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/htmlparse/public/logging_html_filter.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
class FileSystem;
class MessageHandler;
class StatisticsLog;

// Framework for reading an input HTML file, running it through
// a chain of HTML filters, and writing an output file.
class FileDriver {
 public:
  FileDriver(HtmlParse* html_parse, FileSystem* file_system);

  // Return the parser.  This can be used to add filters.
  HtmlParse* html_parse() { return html_parse_; }

  // Helper function to generate an output .html filename from
  // an input filename.  Given "/a/b/c.html" returns "a/b/c.out.html".
  // Returns false if the input file does not contain a "."
  static bool GenerateOutputFilename(
      const char* infilename, GoogleString* outfilename);

  // Helper function to generate an output .stats filename from
  // an input filename.  Given "/a/b/c.html" returns "a/b/c.stats".
  // Returns false if the input file does not contain a "."
  static bool GenerateStatsFilename(
      const char* infilename, GoogleString* statsfilename);

  // Error messages are sent to the message file, true is returned
  // if the file was parsed successfully.
  bool ParseFile(const char* infilename,
                 const char* outfilename,
                 const char* statsfilename,
                 MessageHandler* handler);

 private:
  HtmlParse* html_parse_;
  LoggingFilter logging_filter_;
  StatisticsLog* stats_log_;
  HtmlWriterFilter html_write_filter_;
  bool filters_added_;
  FileSystem* file_system_;

  DISALLOW_COPY_AND_ASSIGN(FileDriver);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_FILE_DRIVER_H_
