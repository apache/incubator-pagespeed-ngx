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

#include <vector>

#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/console_suggestions.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

// Handler which serves PSOL console.
void ConsoleHandler(ServerContext* server_context, Writer* writer,
                    MessageHandler* handler) {
  ConsoleSuggestionsFactory suggestions_factory(server_context->statistics());
  suggestions_factory.GenerateSuggestions();
  const std::vector<ConsoleSuggestion>* suggestions =
      suggestions_factory.suggestions();

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
                "      //background-color: green;\n"
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
                "    #menu {\n"
                "      float: left;\n"
                "      width: 15%;\n"
                "      height: 100%;\n"
                "      border-right: 0.1em solid black;\n"
                "    }\n"
                "    #suggestions {\n"
                "      float: left;\n"
                "      height: 100%;\n"
                "    }\n"
                "  </style>\n"
                "  <body>\n"
                "    <div id='top-bar'>\n"
                "      <span id='title'>PSOL Console</span>\n"
                "      <div id='metric-box'>\n"
                // TODO(sligocki): Get real metric.
                "        <div id='metric-value'>32%</div>\n"
                "        <div id='metric-name'>Bytes saved (dummy)</div>\n"
                "      </div>\n"
                "    </div>\n"
                "\n"
                "    <div id='menu'>\n"
                "      <ul>\n"
                // TODO(sligocki): Add links here.
                "        <li>Statistics monitoring</li>\n"
                "        <li>Enabled domains</li>\n"
                "        <li>...</li>\n"
                "      </ul>\n"
                "    </div>\n"
                "\n"
                "    <div id='suggestions'>\n"
                "      <p>\n"
                "        Notable issues (Click through for the links for \n"
                "        info on how to fix these problems):\n"
                "      </p>\n"
                "      <ol>\n", handler);

  for (int i = 0, n = suggestions->size(); i < n; ++i) {
    // TODO(sligocki): Sanitize url and message? Or at least check
    // for sanity.
    // TODO(sligocki): Only list top N suggestions or only issues which are
    // problematic enough (currently we list all issues including things like:
    // "Fetch failure rate: 0.00%")
    if (suggestions->at(i).doc_url.empty()) {
      writer->Write(StringPrintf("        <li>%s</li>\n",
                                 suggestions->at(i).message.c_str()), handler);
    } else {
      writer->Write(StringPrintf("        <li><a href='%s'>%s</a></li>\n",
                                 suggestions->at(i).doc_url.c_str(),
                                 suggestions->at(i).message.c_str()), handler);
    }
  }

  writer->Write("      </ol>\n"
                "    </div>\n"
                "    <div\n"
                "  </body>\n"
                "</html>\n", handler);
}

}  // namespace net_instaweb
