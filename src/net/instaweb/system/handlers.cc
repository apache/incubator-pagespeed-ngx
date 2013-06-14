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

#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

extern const char* CSS_console_css;
extern const char* JS_console_js;

// Handler which serves PSOL console.
void ConsoleHandler(ServerContext* server_context, Writer* writer,
                    MessageHandler* handler) {
  // TODO(sligocki): Move static content to a data2cc library.
  writer->Write("<!DOCTYPE html>\n"
                "<html>\n"
                "  <head>\n"
                "    <title>PSOL Console</title>\n"
                "  </head>\n"
                "  <style>\n"
                "    html, body {\n"
                "      padding: 0; border: 0; margin: 0;\n"
                "      height: 100%;\n"
                "    }\n"
                "    #top-bar {\n"
                "      width: 100%;\n"
                "      border-bottom: 0.1em solid black;\n"
                "    }\n"
                "    #title {\n"
                "      font-size: 300%;\n"
                "    }\n"
                "    #metric-box {\n"
                "      float:right;\n"
                "      background-color: lightgreen;\n"
                "      padding: 1em;\n"
                "      border: 0.1em solid black;\n"
                "    }\n"
                "    #metric-value {\n"
                "      font-size: 200%;\n"
                "      text-align: center;\n"
                "    }\n"
                "    #metric-name {\n"
                "      text-align: center;\n"
                "    }\n"
                "  </style>\n"
                "  <style>", handler);
  writer->Write(CSS_console_css, handler);
  writer->Write("</style>\n"
                "  <body>\n"
                "    <div id='top-bar'>\n"
                "      <span id='title'>PSOL Console</span>\n"
                // TODO(sligocki): Get real metric and uncomment this block.
                "      <!-- <div id='metric-box'>\n"
                "        <div id='metric-value'>32%</div>\n"
                "        <div id='metric-name'>Bytes saved (dummy)</div>\n"
                "      </div> -->\n"
                "    </div>\n"
                "\n"
                "    <div id='suggestions'>\n"
                "      <p>\n"
                "        Notable issues:\n"
                "      </p>\n"
                "      <div id='pagespeed-graphs-container'></div>\n"
                "    </div>\n"
                "    <script src='https://www.google.com/jsapi'></script>\n"
                "    <script>", handler);
  writer->Write(JS_console_js, handler);
  writer->Write("</script>\n"
                "  </body>\n"
                "</html>\n", handler);
}

}  // namespace net_instaweb
