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
// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/system/public/system_server_context.h"

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher_stats.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/add_headers_fetcher.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_request_context.h"
#include "net/instaweb/system/public/system_rewrite_driver_factory.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/split_statistics.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_logger.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

namespace {

const char kHtmlRewriteTimeUsHistogram[] = "Html Time us Histogram";
const char kLocalFetcherStatsPrefix[] = "http";
const char kCacheFlushCount[] = "cache_flush_count";
const char kCacheFlushTimestampMs[] = "cache_flush_timestamp_ms";
const char kStatistics404Count[] = "statistics_404_count";

}  // namespace

// Generated from JS, CSS, and HTML source via net/instaweb/js/data_to_c.cc.
extern const char* JS_mod_pagespeed_console_js;
extern const char* CSS_mod_pagespeed_console_css;
extern const char* HTML_mod_pagespeed_console_body;

SystemServerContext::SystemServerContext(
    RewriteDriverFactory* factory, StringPiece hostname, int port)
    : ServerContext(factory),
      initialized_(false),
      cache_flush_mutex_(thread_system()->NewMutex()),
      last_cache_flush_check_sec_(0),
      cache_flush_count_(NULL),         // Lazy-initialized under mutex.
      cache_flush_timestamp_ms_(NULL),  // Lazy-initialized under mutex.
      html_rewrite_time_us_histogram_(NULL),
      local_statistics_(NULL),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))) {
  global_system_rewrite_options()->set_description(hostname_identifier_);
}

SystemServerContext::~SystemServerContext() {
}

// If we haven't checked the timestamp of $FILE_PREFIX/cache.flush in the past
// cache_flush_poll_interval_sec_ seconds do so, and if the timestamp has
// expired then update the cache_invalidation_timestamp in global_options,
// thus flushing the cache.
void SystemServerContext::FlushCacheIfNecessary() {
  int64 cache_flush_poll_interval_sec =
      global_system_rewrite_options()->cache_flush_poll_interval_sec();
  if (cache_flush_poll_interval_sec > 0) {
    int64 now_sec = timer()->NowMs() / Timer::kSecondMs;
    bool check_cache_file = false;
    {
      ScopedMutex lock(cache_flush_mutex_.get());
      if (now_sec >= (last_cache_flush_check_sec_ +
                      cache_flush_poll_interval_sec)) {
        last_cache_flush_check_sec_ = now_sec;
        check_cache_file = true;
      }
      if (cache_flush_count_ == NULL) {
        cache_flush_count_ = statistics()->GetVariable(kCacheFlushCount);
        cache_flush_timestamp_ms_ = statistics()->GetVariable(
            kCacheFlushTimestampMs);
      }
    }

    if (check_cache_file) {
      GoogleString cache_flush_filename =
          global_system_rewrite_options()->cache_flush_filename();
      if (cache_flush_filename.empty()) {
        cache_flush_filename = "cache.flush";
      }
      if (cache_flush_filename[0] != '/') {
        // Implementations must ensure the file cache path is an absolute path.
        // mod_pagespeed checks in mod_instaweb.cc:pagespeed_post_config while
        // ngx_pagespeed checks in ngx_pagespeed.cc:ps_merge_srv_conf.
        DCHECK_EQ('/', global_system_rewrite_options()->file_cache_path()[0]);
        cache_flush_filename = StrCat(
            global_system_rewrite_options()->file_cache_path(), "/",
            cache_flush_filename);
      }
      int64 cache_flush_timestamp_sec;
      NullMessageHandler null_handler;
      if (file_system()->Mtime(cache_flush_filename,
                               &cache_flush_timestamp_sec,
                               &null_handler)) {
        int64 timestamp_ms = cache_flush_timestamp_sec * Timer::kSecondMs;

        bool flushed = UpdateCacheFlushTimestampMs(timestamp_ms);

        // The multiple child processes each must independently
        // discover a fresh cache.flush and update the options. However,
        // as shown in
        //     http://code.google.com/p/modpagespeed/issues/detail?id=568
        // we should only bump the flush-count and print a warning to
        // the log once per new timestamp.
        if (flushed &&
            (timestamp_ms !=
             cache_flush_timestamp_ms_->SetReturningPreviousValue(
                 timestamp_ms))) {
          int count = cache_flush_count_->Add(1);
          message_handler()->Message(kWarning, "Cache Flush %d", count);
        }
      }
    } else {
      // Check on every request whether another child process has updated the
      // statistic.
      int64 timestamp_ms = cache_flush_timestamp_ms_->Get();

      // Do the difference-check first because that involves only a
      // reader-lock, so we have zero contention risk when the cache is not
      // being flushed.
      if ((timestamp_ms > 0) &&
          (global_options()->cache_invalidation_timestamp() < timestamp_ms)) {
        UpdateCacheFlushTimestampMs(timestamp_ms);
      }
    }
  }
}

bool SystemServerContext::UpdateCacheFlushTimestampMs(int64 timestamp_ms) {
  return global_options()->UpdateCacheInvalidationTimestampMs(timestamp_ms);
}

void SystemServerContext::AddHtmlRewriteTimeUs(int64 rewrite_time_us) {
  if (html_rewrite_time_us_histogram_ != NULL) {
    html_rewrite_time_us_histogram_->Add(rewrite_time_us);
  }
}

SystemRewriteOptions* SystemServerContext::global_system_rewrite_options() {
  SystemRewriteOptions* out =
      dynamic_cast<SystemRewriteOptions*>(global_options());
  CHECK(out != NULL);
  return out;
}

void SystemServerContext::CreateLocalStatistics(
    Statistics* global_statistics,
    SystemRewriteDriverFactory* factory) {
  local_statistics_ =
      factory->AllocateAndInitSharedMemStatistics(
          true /* local */, hostname_identifier(),
          *global_system_rewrite_options());
  split_statistics_.reset(new SplitStatistics(
      factory->thread_system(), local_statistics_, global_statistics));
  // local_statistics_ was ::InitStat'd by AllocateAndInitSharedMemStatistics,
  // but we need to take care of split_statistics_.
  factory->NonStaticInitStats(split_statistics_.get());
}

void SystemServerContext::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheFlushCount);
  statistics->AddVariable(kCacheFlushTimestampMs);
  statistics->AddVariable(kStatistics404Count);
  Histogram* html_rewrite_time_us_histogram =
      statistics->AddHistogram(kHtmlRewriteTimeUsHistogram);
  // We set the boundary at 2 seconds which is about 2 orders of magnitude worse
  // than anything we have reasonably seen, to make sure we don't cut off actual
  // samples.
  html_rewrite_time_us_histogram->SetMaxValue(2 * Timer::kSecondUs);
  UrlAsyncFetcherStats::InitStats(kLocalFetcherStatsPrefix, statistics);
}

Variable* SystemServerContext::statistics_404_count() {
  return statistics()->GetVariable(kStatistics404Count);
}

void SystemServerContext::ChildInit(SystemRewriteDriverFactory* factory) {
  DCHECK(!initialized_);
  if (!initialized_ && !global_options()->unplugged()) {
    initialized_ = true;
    set_lock_manager(factory->caches()->GetLockManager(
        global_system_rewrite_options()));
    UrlAsyncFetcher* fetcher =
        factory->GetFetcher(global_system_rewrite_options());
    set_default_system_fetcher(fetcher);

    if (split_statistics_.get() != NULL) {
      // Readjust the SHM stuff for the new process
      local_statistics_->Init(false, message_handler());

      // Create local stats for the ServerContext, and fill in its
      // statistics() and rewrite_stats() using them; if we didn't do this here
      // they would get set to the factory's by the InitServerContext call
      // below.
      set_statistics(split_statistics_.get());
      local_rewrite_stats_.reset(new RewriteStats(
          split_statistics_.get(), factory->thread_system(), factory->timer()));
      set_rewrite_stats(local_rewrite_stats_.get());

      // In case of gzip fetching, we will have the UrlAsyncFetcherStats take
      // care of decompression rather than the original fetcher, so we get
      // correct numbers for bytes fetched.
      bool fetch_with_gzip = global_system_rewrite_options()->fetch_with_gzip();
      if (fetch_with_gzip) {
        fetcher->set_fetch_with_gzip(false);
      }
      stats_fetcher_.reset(new UrlAsyncFetcherStats(
          kLocalFetcherStatsPrefix, fetcher,
          factory->timer(), split_statistics_.get()));
      if (fetch_with_gzip) {
        stats_fetcher_->set_fetch_with_gzip(true);
      }
      set_default_system_fetcher(stats_fetcher_.get());
    }

    // To allow Flush to come in while multiple threads might be
    // referencing the signature, we must be able to mutate the
    // timestamp and signature atomically.  RewriteOptions supports
    // an optional read/writer lock for this purpose.
    global_options()->set_cache_invalidation_timestamp_mutex(
        thread_system()->NewRWLock());
    factory->InitServerContext(this);

    html_rewrite_time_us_histogram_ = statistics()->GetHistogram(
        kHtmlRewriteTimeUsHistogram);
    html_rewrite_time_us_histogram_->SetMaxValue(2 * Timer::kSecondUs);
  }
}


void SystemServerContext::ApplySessionFetchers(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const SystemRewriteOptions* conf =
      SystemRewriteOptions::DynamicCast(driver->options());
  CHECK(conf != NULL);
  SystemRequestContext* system_request = SystemRequestContext::DynamicCast(
      request.get());
  if (system_request == NULL) {
    return;  // decoding_driver has a null RequestContext.
  }

  // Note that these fetchers are applied in the opposite order of how they are
  // added: the last one added here is the first one applied and vice versa.
  //
  // Currently, we want AddHeadersFetcher running first, then perhaps
  // SpdyFetcher and then LoopbackRouteFetcher (and then Serf).
  //
  // We want AddHeadersFetcher to run before the SpdyFetcher since we
  // want any headers it adds to be visible.
  //
  // We want SpdyFetcher to run before LoopbackRouteFetcher as it needs
  // to know the request hostname, which LoopbackRouteFetcher could potentially
  // rewrite to 127.0.0.1; and it's OK without the rewriting since it will
  // always talk to the local machine anyway.
  SystemRewriteOptions* options = global_system_rewrite_options();
  if (!options->disable_loopback_routing() &&
      !options->slurping_enabled() &&
      !options->test_proxy()) {
    // Note the port here is our port, not from the request, since
    // LoopbackRouteFetcher may decide we should be talking to ourselves.
    driver->SetSessionFetcher(new LoopbackRouteFetcher(
        driver->options(), system_request->local_ip(),
        system_request->local_port(), driver->async_fetcher()));
  }

  // Apache has experimental support for direct fetching from mod_spdy.  Other
  // implementations that support something similar would use this hook.
  MaybeApplySpdySessionFetcher(request, driver);

  if (driver->options()->num_custom_fetch_headers() > 0) {
    driver->SetSessionFetcher(new AddHeadersFetcher(driver->options(),
                                                    driver->async_fetcher()));
  }
}

void SystemServerContext::CollapseConfigOverlaysAndComputeSignatures() {
  ComputeSignature(global_system_rewrite_options());
}

// Handler which serves PSOL console.
void SystemServerContext::ConsoleHandler(SystemRewriteOptions* options,
                                         Writer* writer) {
  MessageHandler* handler = message_handler();
  bool statistics_enabled = options->statistics_enabled();
  bool logging_enabled = options->statistics_logging_enabled();
  bool log_dir_set = !options->log_dir().empty();
  if (statistics_enabled && logging_enabled && log_dir_set) {
    StringPiece console_js = static_asset_manager()->GetAsset(
        StaticAssetManager::kConsoleJs, options);
    StringPiece console_css = static_asset_manager()->GetAsset(
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
void SystemServerContext::StatisticsGraphsHandler(Writer* writer) {
  SystemRewriteOptions* options = global_system_rewrite_options();
  writer->Write("<!DOCTYPE html>"
                "<title>mod_pagespeed console</title>",
                message_handler());
  writer->Write("<style>", message_handler());
  writer->Write(CSS_mod_pagespeed_console_css, message_handler());
  writer->Write("</style>", message_handler());
  writer->Write(HTML_mod_pagespeed_console_body, message_handler());
  writer->Write("<script>", message_handler());
  if (options->statistics_logging_charts_js().size() > 0 &&
      options->statistics_logging_charts_css().size() > 0) {
    writer->Write("var chartsOfflineJS = '", message_handler());
    writer->Write(options->statistics_logging_charts_js(), message_handler());
    writer->Write("';", message_handler());
    writer->Write("var chartsOfflineCSS = '", message_handler());
    writer->Write(options->statistics_logging_charts_css(), message_handler());
    writer->Write("';", message_handler());
  } else {
    if (options->statistics_logging_charts_js().size() > 0 ||
        options->statistics_logging_charts_css().size() > 0) {
      message_handler()->Message(kWarning, "Using online Charts API.");
    }
    writer->Write("var chartsOfflineJS, chartsOfflineCSS;", message_handler());
  }
  writer->Write(JS_mod_pagespeed_console_js, message_handler());
  writer->Write("</script>", message_handler());
}

const char* SystemServerContext::StatisticsHandler(
    SystemCaches* caches,
    Statistics* stats,
    SystemRewriteOptions* spdy_config,
    StringPiece query_params,
    ContentType* content_type,
    Writer* writer) {
  int64 start_time, end_time, granularity_ms;
  std::set<GoogleString> var_titles;
  QueryParams params;
  params.Parse(query_params);

  // Parse various mode query params.
  bool print_normal_config = params.Has("config");
  bool print_spdy_config = params.Has("spdy_config");
  MessageHandler* handler = message_handler();

  // JSON statistics handling is done only if we have a console logger.
  bool json_output = false;
  *content_type = kContentTypeHtml;
  if (stats->console_logger() != NULL) {
    // Default values for start_time, end_time, and granularity_ms in case the
    // query does not include these parameters.
    start_time = 0;
    end_time = timer()->NowMs();
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
    stats->console_logger()->DumpJSON(var_titles, start_time, end_time,
                                      granularity_ms, writer,
                                      handler);
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
        handler);

    // Only print stats or configuration, not both.
    if (!print_normal_config && !print_spdy_config) {
      bool is_global_request = (stats != statistics());
      writer->Write(is_global_request ?
                    "Global Statistics" : "VHost-Specific Statistics",
                    handler);

      // Write <pre></pre> for Dump to keep good format.
      writer->Write("<pre>", handler);
      stats->Dump(writer, handler);
      writer->Write("</pre>", handler);
      stats->RenderHistograms(writer, handler);

      int flags = SystemCaches::kDefaultStatFlags;
      if (is_global_request) {
        flags |= SystemCaches::kGlobalView;
      }

      if (params.Has("memcached")) {
        flags |= SystemCaches::kIncludeMemcached;
      }

      GoogleString backend_stats;
      caches->PrintCacheStats(
          static_cast<SystemCaches::StatFlags>(flags), &backend_stats);
      if (!backend_stats.empty()) {
        HtmlKeywords::WritePre(backend_stats, writer, handler);
      }
    }

    if (print_normal_config) {
      writer->Write("Configuration:<br>", handler);
      HtmlKeywords::WritePre(
          global_system_rewrite_options()->OptionsToString(),
          writer, handler);
    }

    if (print_spdy_config) {
      if (spdy_config == NULL) {
        writer->Write("SPDY-specific configuration missing, using default.",
                      handler);
      } else {
        writer->Write("SPDY-specific configuration:<br>", handler);
        HtmlKeywords::WritePre(spdy_config->OptionsToString(), writer,
                               handler);
      }
    }
  }
  return NULL;  // No errors.
}

}  // namespace net_instaweb
