// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/system/public/handlers.h"

#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

extern const char* CSS_console_css;
extern const char* JS_console_js;

// Handler which serves PSOL console.
void ConsoleHandler(SystemRewriteOptions* options, Writer* writer,
                    MessageHandler* handler) {
  bool statistics_enabled = options->statistics_enabled();
  bool logging_enabled = options->statistics_logging_enabled();
  bool log_dir_set = !options->log_dir().empty();
  if (statistics_enabled && logging_enabled && log_dir_set) {
    // TODO(sligocki): Move static content to a data2cc library.
    writer->Write("<!DOCTYPE html>\n"
                  "<html>\n"
                  "  <head>\n"
                  "    <title>Pagespeed Console</title>\n"
                  "    <style>\n"
                  "      #title {\n"
                  "        font-size: 300%;\n"
                  "      }\n"
                  "    </style>\n"
                  "    <style>", handler);
    writer->Write(CSS_console_css, handler);
    writer->Write("</style>\n"
                  "  </head>\n"
                  "  <body>\n"
                  "    <div id='top-bar'>\n"
                  "      <span id='title'>Pagespeed Console</span>\n"
                  "    </div>\n"
                  "\n"
                  "    <div id='suggestions'>\n"
                  "      <p>\n"
                  "        Notable issues:\n"
                  "      </p>\n"
                  "      <div id='pagespeed-graphs-container'></div>\n"
                  "    </div>\n"
                  "    <script src='https://www.google.com/jsapi'></script>\n"
                  "    <script>var pagespeedStatisticsUrl = '", handler);
    writer->Write(options->statistics_handler_path(), handler);
    writer->Write("'</script>\n"
                  "    <script>", handler);
    writer->Write(JS_console_js, handler);
    writer->Write("</script>\n"
                  "  </body>\n"
                  "</html>\n", handler);
  } else {
    writer->Write("<!DOCTYPE html>\n"
                  "<p>\n"
                  "  Failed to load Pagespeed Console because:\n"
                  "</p>\n"
                  "<ul>\n", handler);
    if (!statistics_enabled) {
      writer->Write("  <li>ModPagespeedStatistics is not enabled.</li>\n",
                    handler);
    }
    if (!logging_enabled) {
      writer->Write("  <li>ModPagespeedStatisticsLogging is not enabled."
                    "</li>\n", handler);
    }
    if (!log_dir_set) {
      writer->Write("  <li>ModPagespeedLogFile is not set.</li>\n", handler);
    }
    writer->Write("</ul>\n"
                  "<p>\n"
                  "  In order to use the console you must add the following\n"
                  "  configuration:\n"
                  "</p>\n"
                  "<pre>\n"
                  "  ModPagespeedStatistics on\n"
                  "  ModPagespeedStatisticsLogging on\n"
                  "  ModPagespeedLogFile /var/log/mod_pagespeed\n"
                  "</pre>\n"
                  "<p>\n"
                  "  See <a href='https://developers.google.com/speed/pagespe"
                  "ed/module/console'>documentation</a> for more details.\n"
                  "</p>\n", handler);
  }
}

}  // namespace net_instaweb
