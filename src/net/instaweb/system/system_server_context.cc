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
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher_stats.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
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
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/property_store.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/split_statistics.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_logger.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

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
      use_per_vhost_statistics_(false),
      cache_flush_mutex_(thread_system()->NewMutex()),
      last_cache_flush_check_sec_(0),
      cache_flush_count_(NULL),         // Lazy-initialized under mutex.
      cache_flush_timestamp_ms_(NULL),  // Lazy-initialized under mutex.
      html_rewrite_time_us_histogram_(NULL),
      local_statistics_(NULL),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))),
      system_caches_(NULL) {
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
  use_per_vhost_statistics_ = factory->use_per_vhost_statistics();
  if (!initialized_ && !global_options()->unplugged()) {
    initialized_ = true;
    system_caches_ = factory->caches();
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

const SystemRewriteOptions* SystemServerContext::SpdyGlobalConfig() const {
  // Subclasses can override to point to the SPDY configuration.
  // ../apache/apache_server_context.h does.
  return NULL;
}

// TODO(jmarantz): consider factoring the code below to a new class AdminSite
// so that it can be more easily unit-tested.
namespace {

// This style fragment is copied from ../rewriter/console.css because it's
// kind of nice.  However if we import the whole console.css into admin pages
// it looks terrible.
//
// TODO(jmarantz): Get UX help to style the whole admin site better.
// TODO(jmarantz): Factor this out into its own CSS file.
const char kATagStyle[] =
    "a {text-decoration:none; color:#15c; cursor:pointer;}"
    "a:visited {color: #61c;}"
    "a:hover {text-decoration:underline;}"
    "a:active {text-decoration:underline; color:#d14836;}";

struct Tab {
  const char* label;
  const char* title;
  const char* admin_link;       // relative from /pagespeed_admin/
  const char* statistics_link;  // relative from /mod_pagespeed_statistics
  const char* space;            // html for inter-link spacing.
};

const char kShortBreak[] = " ";
const char kLongBreak[] = " &nbsp;&nbsp; ";

// TODO(jmarantz): disable or recolor links to pages that are not available
// based on the current config.
const Tab kTabs[] = {
  {"Statistics", "Statistics", "statistics", "?", kLongBreak},
  {"Configuration", "Configuration", "config", "?config", kShortBreak},
  {"(SPDY)", "SPDY Configuration", "spdy_config", "?spdy_config", kLongBreak},
  {"Histograms", "Histograms", "histograms", "?histograms", kLongBreak},
  {"Caches", "Caches", "cache", "?cache", kLongBreak},
  {"Console", "Console", "console", NULL, kLongBreak},
  {"Message History", "Message History", "message_history", NULL, kLongBreak},
};

// Controls the generation of an HTML Admin page.  Constructing it
// establishes the content-type as HTML and response code 200, and
// puts in a banner with links to all the admin pages, ready for
// appending more <body> elements.  Destructing AdminHtml closes the
// body and completes the fetch.
class AdminHtml {
 public:
  AdminHtml(StringPiece current_link, StringPiece head_extra,
            SystemServerContext::AdminSource source, AsyncFetch* fetch,
            MessageHandler* handler)
      : fetch_(fetch),
        handler_(handler) {
    fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);
    fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");

    // Let PageSpeed dynamically minify the html, css, and javasript
    // generated by the admin page, to the extent it's not done
    // already by the tools.  Note, this does mean that viewing the
    // statistics and histograms pages will affect the statistics and
    // histograms.  If we decide this is too annoying, then we can
    // instead procedurally minify the css/js and leave the html
    // alone.
    //
    // Note that we at least turn off add_instrumenation here by explicitly
    // giving a filter list without "+" or "-".
    fetch->response_headers()->Add(
        RewriteQuery::kPageSpeedFilters,
        "rewrite_css,rewrite_javascript,collapse_whitespace");

    // Generate some navigational links to help our users get to other
    // admin pages.
    fetch->Write("<!DOCTYPE html>\n<html><head>", handler_);
    fetch->Write(StrCat("<style>", kATagStyle, "</style>"), handler_);

    GoogleString buf;
    for (int i = 0, n = arraysize(kTabs); i < n; ++i) {
      const Tab& tab = kTabs[i];
      const char* link = NULL;
      switch (source) {
        case SystemServerContext::kPageSpeedAdmin:
          link = tab.admin_link;
          break;
        case SystemServerContext::kStatistics:
          link = tab.statistics_link;
          break;
        case SystemServerContext::kOther:
          link = NULL;
          break;
      }
      if (link != NULL) {
        StringPiece style;
        if (tab.admin_link == current_link) {
          style = " style='color:darkblue;text-decoration:underline;'";
          fetch->Write(StrCat("<title>PageSpeed ", tab.title, "</title>"),
                       handler_);
        }
        StrAppend(&buf,
                  "<a href='", link, "'", style, ">", tab.label, "</a>",
                  tab.space);
      }
    }

    fetch->Write(StrCat(head_extra, "</head>"), handler_);
    fetch->Write(
        StrCat("<body><div style='font-size:16px;font-family:sans-serif;'>\n"
               "<b>Pagespeed Admin</b>", kLongBreak, "\n"),
        handler_);
    fetch->Write(buf, handler_);
    fetch->Write("</div><hr/>\n", handler_);
    fetch->Flush(handler_);
  }

  ~AdminHtml() {
    fetch_->Write("</body></html>", handler_);
    fetch_->Done(true);
  }

 private:
  AsyncFetch* fetch_;
  MessageHandler* handler_;
};

}  // namespace

// Handler which serves PSOL console.
void SystemServerContext::ConsoleHandler(const SystemRewriteOptions& options,
                                         AdminSource source,
                                         const QueryParams& query_params,
                                         AsyncFetch* fetch) {
  if (query_params.Has("json")) {
    ConsoleJsonHandler(query_params, fetch);
    return;
  }

  MessageHandler* handler = message_handler();
  bool statistics_enabled = options.statistics_enabled();
  bool logging_enabled = options.statistics_logging_enabled();
  bool log_dir_set = !options.log_dir().empty();

  // TODO(jmarantz): change StaticAssetManager to take options by const ref.
  // TODO(sligocki): Move static content to a data2cc library.
  StringPiece console_js = static_asset_manager()->GetAsset(
      StaticAssetManager::kConsoleJs, &options);
  StringPiece console_css = static_asset_manager()->GetAsset(
      StaticAssetManager::kConsoleCss, &options);
  GoogleString head_markup = StrCat(
      "<style>", console_css, "</style>\n");
  AdminHtml admin_html("console", head_markup, source, fetch,
                       message_handler());
  if (statistics_enabled && logging_enabled && log_dir_set) {
    fetch->Write("<div class='console_div' id='suggestions'>\n"
                 "  <div class='console_div' id='pagespeed-graphs-container'>"
                 "</div>\n</div>\n"
                 "<script src='https://www.google.com/jsapi'></script>\n"
                 "<script>var pagespeedStatisticsUrl = '';</script>\n"
                 "<script>", handler);
    // From the admin page, the console JSON is relative, so it can
    // be set to ''.  Formerly it was set to options.statistics_handler_path(),
    // but there does not appear to be a disadvantage to always handling it
    // from whatever URL served this console HTML.
    //
    // TODO(jmarantz): Change the JS to remove pagespeedStatisticsUrl.
    fetch->Write(console_js, handler);
    fetch->Write("</script>\n", handler);
  } else {
    fetch->Write("<p>\n"
                 "  Failed to load PageSpeed Console because:\n"
                 "</p>\n"
                 "<ul>\n", handler);
    if (!statistics_enabled) {
      fetch->Write("  <li>Statistics is not enabled.</li>\n",
                    handler);
    }
    if (!logging_enabled) {
      fetch->Write("  <li>StatisticsLogging is not enabled."
                    "</li>\n", handler);
    }
    if (!log_dir_set) {
      fetch->Write("  <li>LogDir is not set.</li>\n", handler);
    }
    fetch->Write("</ul>\n"
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

void SystemServerContext::StatisticsHandler(
    bool is_global_request,
    AdminSource source,
    AsyncFetch* fetch) {
  if (!use_per_vhost_statistics_) {
    is_global_request = true;
  }
  AdminHtml admin_html("statistics", "", source, fetch, message_handler());
  MessageHandler* handler = message_handler();
  Statistics* stats = is_global_request ? factory()->statistics()
      : statistics();
  // Write <pre></pre> for Dump to keep good format.
  fetch->Write("<pre>", handler);
  stats->Dump(fetch, handler);
  fetch->Write("</pre>", handler);
}

void SystemServerContext::ConsoleJsonHandler(
    const QueryParams& params, AsyncFetch* fetch) {
  StatisticsLogger* console_logger = statistics()->console_logger();
  if (console_logger == NULL) {
    fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    fetch->response_headers()->Add(HttpAttributes::kContentType, "text/plain");
    fetch->Write(
        "console_logger must be enabled to use '?json' query parameter.",
        message_handler());
  } else {
    fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);

    fetch->response_headers()->Add(HttpAttributes::kContentType,
                                   kContentTypeJson.mime_type());

    int64 start_time, end_time, granularity_ms;
    std::set<GoogleString> var_titles;

    // Default values for start_time, end_time, and granularity_ms in case the
    // query does not include these parameters.
    start_time = 0;
    end_time = timer()->NowMs();
    // Granularity is the difference in ms between data points. If it is not
    // specified by the query, the default value is 3000 ms, the same as the
    // default logging granularity.
    granularity_ms = 3000;
    for (int i = 0; i < params.size(); ++i) {
      GoogleString value;
      if (params.UnescapedValue(i, &value)) {
        StringPiece name = params.name(i);
        if (name =="start_time") {
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
    }
    console_logger->DumpJSON(var_titles, start_time, end_time, granularity_ms,
                             fetch, message_handler());
  }
  fetch->Done(true);
}

void SystemServerContext::PrintHistograms(bool is_global_request,
                                          AdminSource source,
                                          AsyncFetch* fetch) {
  Statistics* stats = is_global_request ? factory()->statistics()
      : statistics();
  AdminHtml admin_html("histograms", "", source, fetch, message_handler());
  stats->RenderHistograms(fetch, message_handler());
}

namespace {

static const char kToggleScript[] =
    "<script type='text/javascript'>\n"
    "function toggleDetail(id) {\n"
    "  var toggle_button = document.getElementById(id + '_toggle');\n"
    "  var summary_div = document.getElementById(id + '_summary');\n"
    "  var detail_div = document.getElementById(id + '_detail');\n"
    "  if (toggle_button.checked) {\n"
    "    summary_div.style.display = 'none';\n"
    "    detail_div.style.display = 'block';\n"
    "  } else {\n"
    "    summary_div.style.display = 'block';\n"
    "    detail_div.style.display = 'none';\n"
    "  }\n"
    "}\n"
    "</script>\n";

static const char kTableStart[] =
    "<table style='font-family:sans-serif;font-size:0.9em'>\n"
    "  <thead>\n"
    "    <tr style='font-weight:bold'>\n"
    "      <td>Cache</td><td>Detail</td><td>Structure</td>\n"
    "    </tr>\n"
    "  </thead>\n"
    "  <tbody>";

static const char kTableEnd[] =
    "  </tbody>\n"
    "</table>";

// Takes a complicated descriptor like
//    "HTTPCache(Fallback(small=Batcher(cache=Stats(parefix=memcached_async,"
//    "cache=Async(AprMemCache)),parallelism=1,max=1000),large=Stats("
//    "prefix=file_cache,cache=FileCache)))"
// and strips away the crap most users don't want to see, as they most
// likely did not configure it, and return
//    "Async AprMemCache FileCache"
GoogleString HackCacheDescriptor(StringPiece name) {
  GoogleString out;
  // There's a lot of complicated syntax in the cache name giving the
  // detailed hierarchical structure.  This is really hard to read and
  // overly cryptic; it's designed for unit tests.  But let's extract
  // a few keywords out of this to understand the main pointers.
  static const char* kCacheKeywords[] = {
    "Compressed", "Async", "SharedMemCache", "LRUCache", "AprMemCache",
    "FileCache"
  };
  const char* delim = "";
  for (int i = 0, n = arraysize(kCacheKeywords); i < n; ++i) {
    if (name.find(kCacheKeywords[i]) != StringPiece::npos) {
      StrAppend(&out, delim, kCacheKeywords[i]);
      delim = " ";
    }
  }
  if (out.empty()) {
    name.CopyToString(&out);
  }
  return out;
}

// Takes a complicated descriptor like
//    "HTTPCache(Fallback(small=Batcher(cache=Stats(prefix=memcached_async,"
//    "cache=Async(AprMemCache)),parallelism=1,max=1000),large=Stats("
//    "prefix=file_cache,cache=FileCache)))"
// and injects HTML line-breaks and indentation based on the parent depth,
// yielding HTML that renders like this (with &nbsp; and <br/>)
//    HTTPCache(
//       Fallback(
//          small=Batcher(
//             cache=Stats(
//                prefix=memcached_async,
//                cache=Async(
//                   AprMemCache)),
//             parallelism=1,
//             max=1000),
//          large=Stats(
//             prefix=file_cache,
//             cache=FileCache)))
GoogleString IndentCacheDescriptor(StringPiece name) {
  GoogleString out, buf;
  int depth = 0;
  for (int i = 0, n = name.size(); i < n; ++i) {
    StrAppend(&out, HtmlKeywords::Escape(name.substr(i, 1), &buf));
    switch (name[i]) {
      case '(':
        ++depth;
        FALLTHROUGH_INTENDED;
      case ',':
        out += "<br/>";
        for (int j = 0; j < depth; ++j) {
          out += "&nbsp; &nbsp;";
        }
        break;
      case ')':
        --depth;
        break;
    }
  }
  return out;
}

GoogleString CacheInfoHtmlSnippet(StringPiece label, StringPiece descriptor) {
  GoogleString out, escaped;
  StrAppend(&out, "<tr style='vertical-align:top;'><td>", label,
            "</td><td><input id='", label,
            "_toggle' type='checkbox' onclick='toggleDetail(\"", label,
            "\")'/></td><td><code id='", label, "_summary'>");
  StrAppend(&out, HtmlKeywords::Escape(HackCacheDescriptor(descriptor),
                                       &escaped));
  StrAppend(&out, "</code><code id='", label,
            "_detail' style='display:none;'>");
  StrAppend(&out, IndentCacheDescriptor(descriptor));
  StrAppend(&out, "</code></td></tr>\n");
  return out;
}

}  // namespace

void SystemServerContext::PrintCaches(bool is_global, AdminSource source,
                                      const QueryParams& query_params,
                                      const RewriteOptions* options,
                                      AsyncFetch* fetch) {
  GoogleString url;
  if ((query_params.Lookup1Unescaped("url", &url)) &&
      (source == kPageSpeedAdmin)) {
    // Delegate to ShowCacheHandler to get the cached value for that
    // URL, which it may do asynchronously, so we cannot use the
    // AdminSite abstraction which closes the connection in its
    // destructor.
    ShowCacheHandler(url, fetch, options->Clone());
  } else {
    AdminHtml admin_html("cache", "", source, fetch, message_handler());

    // Present a small form to enter a URL.
    if (source == kPageSpeedAdmin) {
      const char* user_agent = fetch->request_headers()->Lookup1(
          HttpAttributes::kUserAgent);
      fetch->Write(ShowCacheForm(user_agent), message_handler());
    }

    // Display configured cache information.
    if (system_caches_ != NULL) {
      int flags = SystemCaches::kDefaultStatFlags;
      if (is_global) {
        flags |= SystemCaches::kGlobalView;
      }

      // TODO(jmarantz): Consider whether it makes sense to disable
      // either of these flags to limit the content when someone asks
      // for info about the cache.
      flags |= SystemCaches::kIncludeMemcached;

      fetch->Write(kTableStart, message_handler());
      CacheInterface* fsmdc = filesystem_metadata_cache();
      fetch->Write(StrCat(
          CacheInfoHtmlSnippet("HTTP Cache", http_cache()->Name()),
          CacheInfoHtmlSnippet("Metadata Cache", metadata_cache()->Name()),
          CacheInfoHtmlSnippet("Property Cache",
                               page_property_cache()->property_store()->Name()),
          CacheInfoHtmlSnippet("FileSystem Metadata Cache",
                               (fsmdc == NULL) ? "none" : fsmdc->Name())),
                   message_handler());
      fetch->Write(kTableEnd, message_handler());

      GoogleString backend_stats;
      system_caches_->PrintCacheStats(
          static_cast<SystemCaches::StatFlags>(flags), &backend_stats);
      if (!backend_stats.empty()) {
        HtmlKeywords::WritePre(backend_stats, fetch, message_handler());
      }

      // Practice what we preach: put the blocking JS in the tail.
      // TODO(jmarantz): use static asset manager to compile & deliver JS
      // externally.
      fetch->Write(kToggleScript, message_handler());
    }
  }
}

void SystemServerContext::PrintNormalConfig(AdminSource source,
                                            AsyncFetch* fetch) {
  AdminHtml admin_html("config", "", source, fetch, message_handler());
  HtmlKeywords::WritePre(
      global_system_rewrite_options()->OptionsToString(),
      fetch, message_handler());
}

void SystemServerContext::PrintSpdyConfig(AdminSource source,
                                          AsyncFetch* fetch) {
  AdminHtml admin_html("spdy_config", "", source, fetch, message_handler());
  const SystemRewriteOptions* spdy_config = SpdyGlobalConfig();
  if (spdy_config == NULL) {
    fetch->Write("SPDY-specific configuration missing.", message_handler());
  } else {
    HtmlKeywords::WritePre(spdy_config->OptionsToString(), fetch,
                           message_handler());
  }
}

void SystemServerContext::MessageHistoryHandler(AdminSource source,
                                                AsyncFetch* fetch) {
  // Request for page /mod_pagespeed_message.
  GoogleString log;
  StringWriter log_writer(&log);
  AdminHtml admin_html("message_history", "", source, fetch, message_handler());
  if (message_handler()->Dump(&log_writer)) {
    // Write pre-tag for Dump to keep good format.
    HtmlKeywords::WritePre(log, fetch, message_handler());
  } else {
    fetch->Write("<p>Writing to mod_pagespeed_message failed. \n"
                 "Please check if it's enabled in pagespeed.conf.</p>\n",
                 message_handler());
  }
}

void SystemServerContext::AdminPage(
    bool is_global, const GoogleUrl& stripped_gurl,
    const QueryParams& query_params,
    const RewriteOptions* options,
    AsyncFetch* fetch) {
  // The handler is "pagespeed_admin", so we must dispatch off of
  // the remainder of the URL.  For
  // "http://example.com/pagespeed_admin/foo?a=b" we want to pull out
  // "foo".
  //
  // Note that the comments here referring to "/pagespeed_admin" reflect
  // only the default admin path in Apache for fresh installs.  In fact
  // we can put the handler on any path, and this code should still work;
  // all the paths here are specified relative to the incoming URL.
  StringPiece path = stripped_gurl.PathSansQuery();   // "/pagespeed_admin/foo"
  path = path.substr(1);                              // "pagespeed_admin/foo"

  // If there are no slashes at all in the path, e.g. it's "pagespeed_admin",
  // then the relative references to "config" etc will not work.  We need
  // to serve the admin pages on "/pagespeed_admin/".  So if we got to this
  // point and there are no slashes, then we can just redirect immediately
  // by adding a slash.
  //
  // If the user has mapped the pagespeed_admin handler to a path with
  // an embbedded slash, say "pagespeed/myadmin", then it's hard to tell
  // whether we should redirect, because we don't know what the the
  // intended path is.  In this case, we'll fall through to a leaf
  // analysis on "myadmin", fail to find a match, and print a "Did You Mean"
  // page.  It's not as good as a redirect but since we can't tell an
  // omitted slash from a typo it's the best we can do.
  if (path.find('/') == StringPiece::npos) {
    // If the URL is "/pagespeed_admin", then redirect to "/pagespeed_admin/" so
    // that relative URL references will work.
    ResponseHeaders* response_headers = fetch->response_headers();
    response_headers->SetStatusAndReason(HttpStatus::kMovedPermanently);
    GoogleString admin_with_slash = StrCat(stripped_gurl.AllExceptQuery(), "/");
    response_headers->Add(HttpAttributes::kLocation, admin_with_slash);
    response_headers->Add(HttpAttributes::kContentType, "text/html");
    GoogleString escaped_url;
    HtmlKeywords::Escape(admin_with_slash, &escaped_url);
    fetch->Write(StrCat("Redirecting to URL ", escaped_url), message_handler());
    fetch->Done(true);
  } else {
    StringPiece leaf = stripped_gurl.LeafSansQuery();
    if ((leaf == "statistics") || (leaf.empty())) {
      StatisticsHandler(is_global, kPageSpeedAdmin, fetch);
    } else if (leaf == "config") {
      PrintNormalConfig(kPageSpeedAdmin, fetch);
    } else if (leaf == "spdy_config") {
      PrintSpdyConfig(kPageSpeedAdmin, fetch);
    } else if (leaf == "console") {
      // TODO(jmarantz): add vhost-local and aggregate message buffers.
      ConsoleHandler(*global_system_rewrite_options(), kPageSpeedAdmin,
                     query_params, fetch);
    } else if (leaf == "message_history") {
      MessageHistoryHandler(kPageSpeedAdmin, fetch);
    } else if (leaf == "cache") {
      PrintCaches(is_global, kPageSpeedAdmin, query_params, options, fetch);
    } else if (leaf == "histograms") {
      PrintHistograms(is_global, kPageSpeedAdmin, fetch);
    } else {
      fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
      fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");
      fetch->Write("Unknown admin page: ", message_handler());
      HtmlKeywords::WritePre(leaf, fetch, message_handler());

      // It's possible that the handler is installed on /a/b/c, and we
      // are now reporting "unknown admin page: c".  This is kind of a guess,
      // but provide a nice link here to what might be the correct admin page.
      //
      // This is just a guess, so we don't want to redirect.
      fetch->Write("<br/>Did you mean to visit: ", message_handler());
      GoogleString escaped_url;
      HtmlKeywords::Escape(StrCat(stripped_gurl.AllExceptQuery(), "/"),
                           &escaped_url);
      fetch->Write(StrCat("<a href='", escaped_url, "'>", escaped_url,
                          "</a>\n"),
                   message_handler());
      fetch->Done(true);
    }
  }
}

void SystemServerContext::StatisticsPage(bool is_global,
                                         const QueryParams& query_params,
                                         const RewriteOptions* options,
                                         AsyncFetch* fetch) {
  if (query_params.Has("json")) {
    ConsoleJsonHandler(query_params, fetch);
  } else if (query_params.Has("config")) {
    PrintNormalConfig(kStatistics, fetch);
  } else if (query_params.Has("spdy_config")) {
    PrintSpdyConfig(kStatistics, fetch);
  } else if (query_params.Has("histograms")) {
    PrintHistograms(is_global, kStatistics, fetch);
  } else if (query_params.Has("cache")) {
    PrintCaches(is_global, kStatistics, query_params, options, fetch);
  } else {
    StatisticsHandler(is_global, kStatistics, fetch);
  }
}

}  // namespace net_instaweb
