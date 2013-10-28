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

#include <cstddef>  // for size_t
#include <memory>
#include <set>  // for set
#include <vector>  // for vector
#include "pagespeed/kernel/base/basictypes.h"  // for int64
#include "pagespeed/kernel/base/timer.h"  // for Timer
#include "pagespeed/kernel/http/content_type.h"


#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_rewrite_driver_factory.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_logger.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_keywords.h"

namespace net_instaweb {

extern const char* JS_mod_pagespeed_console_js;
extern const char* CSS_mod_pagespeed_console_css;
extern const char* HTML_mod_pagespeed_console_body;

// Handler which serves PSOL console.
void ConsoleHandler(SystemServerContext* server_context,
                    SystemRewriteOptions* options,
                    Writer* writer,
                    MessageHandler* handler) {
  bool statistics_enabled = options->statistics_enabled();
  bool logging_enabled = options->statistics_logging_enabled();
  bool log_dir_set = !options->log_dir().empty();
  if (statistics_enabled && logging_enabled && log_dir_set) {
    const StaticAssetManager* static_asset_manager =
        server_context->static_asset_manager();
    StringPiece console_js = static_asset_manager->GetAsset(
        StaticAssetManager::kConsoleJs, options);
    StringPiece console_css = static_asset_manager->GetAsset(
        StaticAssetManager::kConsoleCss, options);

    // TODO(sligocki): Move static content to a data2cc library.
    writer->Write("<!DOCTYPE html>\n"
                  "<html>\n"
                  "  <head>\n"
                  "    <title>PageSpeed Console</title>\n"
                  "    <style>\n"
                  "      #title {\n"
                  "        font-size: 300%;\n"
                  "      }\n"
                  "    </style>\n"
                  "    <style>", handler);
    writer->Write(console_css, handler);
    writer->Write("</style>\n"
                  "  </head>\n"
                  "  <body>\n"
                  "    <div id='top-bar'>\n"
                  "      <span id='title'>PageSpeed Console</span>\n"
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
    writer->Write(console_js, handler);
    writer->Write("</script>\n"
                  "  </body>\n"
                  "</html>\n", handler);
  } else {
    writer->Write("<!DOCTYPE html>\n"
                  "<p>\n"
                  "  Failed to load PageSpeed Console because:\n"
                  "</p>\n"
                  "<ul>\n", handler);
    if (!statistics_enabled) {
      writer->Write("  <li>Statistics is not enabled.</li>\n",
                    handler);
    }
    if (!logging_enabled) {
      writer->Write("  <li>StatisticsLogging is not enabled."
                    "</li>\n", handler);
    }
    if (!log_dir_set) {
      writer->Write("  <li>LogDir is not set.</li>\n", handler);
    }
    writer->Write("</ul>\n"
                  "<p>\n"
                  "  In order to use the console you must configure these\n"
                  "  options. See the <a href='https://developers.google.com/"
                  "speed/pagespeed/module/console'>console documentation</a>\n"
                  "  for more details.\n"
                  "</p>\n", handler);
  }
}

// TODO(sligocki): integrate this into the pagespeed_console.
void StatisticsGraphsHandler(SystemRewriteOptions* options,
                             Writer* writer,
                             MessageHandler* message_handler) {
  writer->Write("<!DOCTYPE html>"
               "<title>mod_pagespeed console</title>",
               message_handler);
  writer->Write("<style>", message_handler);
  writer->Write(CSS_mod_pagespeed_console_css, message_handler);
  writer->Write("</style>", message_handler);
  writer->Write(HTML_mod_pagespeed_console_body, message_handler);
  writer->Write("<script>", message_handler);
  if (options->statistics_logging_charts_js().size() > 0 &&
      options->statistics_logging_charts_css().size() > 0) {
    writer->Write("var chartsOfflineJS = '", message_handler);
    writer->Write(options->statistics_logging_charts_js(), message_handler);
    writer->Write("';", message_handler);
    writer->Write("var chartsOfflineCSS = '", message_handler);
    writer->Write(options->statistics_logging_charts_css(), message_handler);
    writer->Write("';", message_handler);
  } else {
    if (options->statistics_logging_charts_js().size() > 0 ||
        options->statistics_logging_charts_css().size() > 0) {
      message_handler->Message(kWarning, "Using online Charts API.");
    }
    writer->Write("var chartsOfflineJS, chartsOfflineCSS;", message_handler);
  }
  writer->Write(JS_mod_pagespeed_console_js, message_handler);
  writer->Write("</script>", message_handler);
}

const char* StatisticsHandler(
    SystemRewriteDriverFactory* factory,
    SystemServerContext* server_context,
    SystemRewriteOptions* spdy_config,  /* may be null */
    bool is_global_request,
    StringPiece query_params,
    ContentType* content_type,
    Writer* writer,
    MessageHandler* message_handler) {
  int64 start_time, end_time, granularity_ms;
  std::set<GoogleString> var_titles;
  QueryParams params;
  params.Parse(query_params);

  Statistics* statistics =
      is_global_request ? factory->statistics() : server_context->statistics();

  // Parse various mode query params.
  bool print_normal_config = params.Has("config");
  bool print_spdy_config = params.Has("spdy_config");

  // JSON statistics handling is done only if we have a console logger.
  bool json_output = false;
  *content_type = kContentTypeHtml;
  if (statistics->console_logger() != NULL) {
    // Default values for start_time, end_time, and granularity_ms in case the
    // query does not include these parameters.
    start_time = 0;
    end_time = server_context->timer()->NowMs();
    // Granularity is the difference in ms between data points. If it is not
    // specified by the query, the default value is 3000 ms, the same as the
    // default logging granularity.
    granularity_ms = 3000;
    for (int i = 0; i < params.size(); ++i) {
      const GoogleString value =
          (params.value(i) == NULL) ? "" : *params.value(i);
      StringPiece name = params.name(i);
      if (name == "json") {
        json_output = true;
        *content_type = kContentTypeJson;
      } else if (name =="start_time") {
        StringToInt64(value, &start_time);
      } else if (name == "end_time") {
        StringToInt64(value, &end_time);
      } else if (name == "var_titles") {
        std::vector<StringPiece> variable_names;
        SplitStringPieceToVector(value, ",", &variable_names, true);
        for (size_t i = 0; i < variable_names.size(); ++i) {
          var_titles.insert(variable_names[i].as_string());
        }
      } else if (name == "granularity") {
        StringToInt64(value, &granularity_ms);
      }
    }
  } else {
    if (params.Has("json")) {
      return "console_logger must be enabled to use '?json' query parameter.";
    }
  }
  if (json_output) {
    statistics->console_logger()->DumpJSON(var_titles, start_time, end_time,
                                           granularity_ms, writer,
                                           message_handler);
  } else {
    // Generate some navigational links to the right to help
    // our users get to other modes.
    writer->Write(
        "<div style='float:right'>View "
        "<a href='?config'>Configuration</a>, "
        "<a href='?spdy_config'>SPDY Configuration</a>, "
        "<a href='?'>Statistics</a> "
        "(<a href='?memcached'>with memcached Stats</a>). "
        "</div>",
        message_handler);

    // Only print stats or configuration, not both.
    if (!print_normal_config && !print_spdy_config) {
      writer->Write(is_global_request ?
                   "Global Statistics" : "VHost-Specific Statistics",
                   message_handler);

      // Write <pre></pre> for Dump to keep good format.
      writer->Write("<pre>", message_handler);
      statistics->Dump(writer, message_handler);
      writer->Write("</pre>", message_handler);
      statistics->RenderHistograms(writer, message_handler);

      int flags = SystemCaches::kDefaultStatFlags;
      if (is_global_request) {
        flags |= SystemCaches::kGlobalView;
      }

      if (params.Has("memcached")) {
        flags |= SystemCaches::kIncludeMemcached;
      }

      GoogleString backend_stats;
      factory->caches()->PrintCacheStats(
          static_cast<SystemCaches::StatFlags>(flags), &backend_stats);
      if (!backend_stats.empty()) {
        HtmlKeywords::WritePre(backend_stats, writer, message_handler);
      }
    }

    if (print_normal_config) {
      writer->Write("Configuration:<br>", message_handler);
      HtmlKeywords::WritePre(
          server_context->system_rewrite_options()->OptionsToString(),
          writer, message_handler);
    }

    if (print_spdy_config) {
      if (spdy_config == NULL) {
        writer->Write("SPDY-specific configuration missing, using default.",
                     message_handler);
      } else {
        writer->Write("SPDY-specific configuration:<br>", message_handler);
        HtmlKeywords::WritePre(spdy_config->OptionsToString(), writer,
                               message_handler);
      }
    }
  }
  return NULL;  // No errors.
}

}  // namespace net_instaweb
