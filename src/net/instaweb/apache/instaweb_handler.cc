// Copyright 2010 Google Inc.
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
// Author: lsong@google.com (Libo Song)
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/apache/instaweb_handler.h"

#include <cstddef>
#include <set>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apache_request_context.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/apache_server_context.h"
#include "net/instaweb/apache/apache_slurp.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/in_place_resource_recorder.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "net/instaweb/apache/apache_logging_includes.h"

namespace net_instaweb {

extern const char* JS_mod_pagespeed_console_js;
extern const char* CSS_mod_pagespeed_console_css;
extern const char* HTML_mod_pagespeed_console_body;

namespace {

const char kStatisticsHandler[] = "mod_pagespeed_statistics";
const char kConsoleHandler[] = "mod_pagespeed_console";
const char kGlobalStatisticsHandler[] = "mod_pagespeed_global_statistics";
const char kRefererStatisticsHandler[] = "mod_pagespeed_referer_statistics";
const char kMessageHandler[] = "mod_pagespeed_message";
const char kBeaconHandler[] = "mod_pagespeed_beacon";
const char kLogRequestHeadersHandler[] = "mod_pagespeed_log_request_headers";
const char kGenerateResponseWithOptionsHandler[] =
    "mod_pagespeed_response_options_handler";
const char kResourceUrlNote[] = "mod_pagespeed_resource";
const char kResourceUrlNo[] = "<NO>";
const char kResourceUrlYes[] = "<YES>";

// StringAsyncFetch that can be detached. It will delete itself after the
// latter of Detach() and Done() are called. Therefore, the results can be
// used in the scope of a function, but the fetch can live longer if we
// timeout or want run the fetch asynchronously as well.
class SelfOwnedStringAsyncFetch : public StringAsyncFetch {
 public:
  SelfOwnedStringAsyncFetch(const RequestContextPtr& request_context,
                            AbstractMutex* mutex)
      : StringAsyncFetch(request_context), mutex_(mutex), detached_(false) {}
  virtual ~SelfOwnedStringAsyncFetch() {}

  // Call when you no longer want to the results to be saved. It will either
  // delete itself here or when Done() is called (whichever comes last).
  //
  // Note: Only call once!
  void Detach() {
    ScopedMutex lock(mutex_.get());
    DCHECK(!detached_);
    detached_ = true;
    if (done()) {
      // Note: This is safe because Detach() and Done() should each be called
      // only once, so we don't need locking during the delete step, we can
      // be assured we are the only thread touching this object.
      lock.Release();
      delete this;
    }
  }

  // Fetch does not delete itself unless we have detached it.
  virtual void HandleDone(bool success) {
    ScopedMutex lock(mutex_.get());
    DCHECK(!done());
    StringAsyncFetch::HandleDone(success);
    if (detached_) {
      // Note: This is safe because Detach() and Done() should each be called
      // only once, so we don't need locking during the delete step, we can
      // be assured we are the only thread touching this object.
      lock.Release();
      delete this;
    }
  }

 private:
  scoped_ptr<AbstractMutex> mutex_;
  bool detached_;

  DISALLOW_COPY_AND_ASSIGN(SelfOwnedStringAsyncFetch);
};

bool IsCompressibleContentType(const char* content_type) {
  if (content_type == NULL) {
    return false;
  }
  GoogleString type = content_type;
  size_t separator_idx = type.find(";");
  if (separator_idx != GoogleString::npos) {
    type.erase(separator_idx);
  }

  bool res = false;
  if (type.find("text/") == 0) {
    res = true;
  } else if (type.find("application/") == 0) {
    if (type.find("javascript") != type.npos ||
        type.find("json") != type.npos ||
        type.find("ecmascript") != type.npos ||
        type == "application/livescript" ||
        type == "application/js" ||
        type == "application/jscript" ||
        type == "application/x-js" ||
        type == "application/xhtml+xml" ||
        type == "application/xml") {
      res = true;
    }
  }

  return res;
}

// Default handler when the file is not found
void instaweb_404_handler(const GoogleString& url, request_rec* request) {
  request->status = HTTP_NOT_FOUND;
  ap_set_content_type(request, "text/html; charset=utf-8");
  ap_rputs("<html><head><title>Not Found</title></head>", request);
  ap_rputs("<body><h1>Apache server with mod_pagespeed</h1>OK", request);
  ap_rputs("<hr>NOT FOUND:", request);
  ap_rputs(url.c_str(), request);
  ap_rputs("</body></html>", request);
}

void send_out_headers_and_body(request_rec* request,
                               const ResponseHeaders& response_headers,
                               const GoogleString& output) {
  // We always disable downstream header filters when sending out
  // pagespeed resources, since we've captured them in the origin fetch.
  ResponseHeadersToApacheRequest(response_headers,
                                 true,  // Disable downstream header filters.
                                 request);
  if (response_headers.status_code() == HttpStatus::kOK &&
      IsCompressibleContentType(request->content_type)) {
    // Make sure compression is enabled for this response.
    ap_add_output_filter("DEFLATE", NULL, request, request->connection);
  }

  // Recompute the content-length, because the content may have changed.
  ap_set_content_length(request, output.size());
  // Send the body
  ap_rwrite(output.c_str(), output.size(), request);
}

// Evaluate custom_options based upon global_options, directory-specific
// options and query-param/request-header options. Returns computed
// custom options (or NULL if global_options should be used).
//
// Caller takes ownership of options.
RewriteOptions* get_custom_options(ApacheServerContext* server_context,
                                   request_rec* request,
                                   GoogleUrl* gurl,
                                   RequestHeaders* request_headers,
                                   RewriteOptions* global_options) {
  // Set directory specific options.  These will be the options for the
  // directory the resource is in, which under some configurations will be
  // different from the options for the directory that the referencing html is
  // in.  This can lead to us using different options here when regenerating
  // the resource than would be used if the resource were generated as part of
  // a rewrite kicked off by a request for the referencing html file.  This is
  // hard to fix, so instead we're documenting that you must make sure the
  // configuration for your resources matches the configuration for your html
  // files.
  RewriteOptions* custom_options = NULL;
  ApacheConfig* directory_options = static_cast<ApacheConfig*>
      ap_get_module_config(request->per_dir_config, &pagespeed_module);
  if ((directory_options != NULL) && directory_options->modified()) {
    custom_options = server_context->apache_factory()->NewRewriteOptions();
    custom_options->Merge(*global_options);
    custom_options->Merge(*directory_options);
  }

  // TODO(sligocki): Move inside PSOL.
  // Merge in query-param or header-based options.
  // Note: We do not generally get response headers in the resource flow,
  // so NULL is passed in instead.
  ServerContext::OptionsBoolPair query_options_success =
      server_context->GetQueryOptions(gurl, request_headers, NULL);
  if (!query_options_success.second) {
    server_context->message_handler()->Message(
        kWarning, "Invalid ModPagespeed query params or headers for "
        "request %s. Serving with default options.", gurl->spec_c_str());
  }
  if (query_options_success.first != NULL) {
    if (custom_options == NULL) {
      custom_options = server_context->apache_factory()->NewRewriteOptions();
      custom_options->Merge(*global_options);
    }
    custom_options->Merge(*query_options_success.first);
    delete query_options_success.first;
    // Don't run any experiments if we're handling a customized request.
    custom_options->set_running_furious_experiment(false);
  }

  return custom_options;
}

// Handle url as .pagespeed. rewritten resource.
void handle_as_pagespeed_resource(const RequestContextPtr& request_context,
                                  GoogleUrl* gurl,
                                  const GoogleString& url,
                                  RewriteOptions* custom_options,
                                  ApacheServerContext* server_context,
                                  RequestHeaders* request_headers,
                                  request_rec* request) {
  RewriteDriver* driver = ResourceFetch::GetDriver(
      *gurl, custom_options, server_context, request_context);

  MessageHandler* message_handler = server_context->message_handler();
  message_handler->Message(kInfo, "Fetching resource %s...", url.c_str());

  GoogleString output;  // TODO(jmarantz): Quit buffering resource output.
  StringWriter writer(&output);

  SyncFetcherAdapterCallback* callback = new SyncFetcherAdapterCallback(
      server_context->thread_system(), &writer, request_context);
  callback->SetRequestHeadersTakingOwnership(request_headers);

  if (ResourceFetch::BlockingFetch(*gurl, server_context, driver, callback)) {
    ResponseHeaders* response_headers = callback->response_headers();
    // TODO(sligocki): Check that this is already done in ResourceFetch
    // and remove redundant setting here.
    response_headers->SetDate(server_context->timer()->NowMs());
    // ResourceFetch adds X-Page-Speed header, old mod_pagespeed code
    // did not. For now, we remove that header for consistency.
    // TODO(sligocki): Consistently use X- headers in MPS and PSOL.
    // I think it would be good to change X-Mod-Pagespeed -> X-Page-Speed
    // and use that for all HTML and resource requests.
    response_headers->RemoveAll(kPageSpeedHeader);
    message_handler->Message(kInfo, "Fetch succeeded for %s, status=%d",
                             url.c_str(), response_headers->status_code());
    send_out_headers_and_body(request, *response_headers, output);
  } else {
    RewriteStats* stats = server_context->rewrite_stats();
    stats->resource_404_count()->Add(1);
    instaweb_404_handler(url, request);
  }

  callback->Release();
}

// Handle url with In Place Resource Optimization (IPRO) flow.
bool handle_as_in_place(const RequestContextPtr& request_context,
                       GoogleUrl* gurl,
                        const GoogleString& url,
                        RewriteOptions* custom_options,
                        ApacheServerContext* server_context,
                        RequestHeaders* owned_headers,
                        request_rec* request) {
  scoped_ptr<RequestHeaders> request_headers(owned_headers);
  bool handled = false;

  RewriteDriver* driver = ResourceFetch::GetDriver(
      *gurl, custom_options, server_context, request_context);

  MessageHandler* message_handler = server_context->message_handler();
  message_handler->Message(kInfo, "Trying to optimize in-place: %s",
                           url.c_str());

  SelfOwnedStringAsyncFetch* fetch = new SelfOwnedStringAsyncFetch(
      request_context, server_context->thread_system()->NewMutex());
  bool perform_http_fetch = false;
  driver->FetchInPlaceResource(*gurl, perform_http_fetch, fetch);

  // Wait for cache lookup to complete.
  if (!fetch->done()) {
    int64 max_ms = driver->options()->blocking_fetch_timeout_ms();
    for (int64 start_ms = server_context->timer()->NowMs(), now_ms = start_ms;
         !fetch->done() && now_ms - start_ms < max_ms;
         now_ms = server_context->timer()->NowMs()) {
      int64 remaining_ms = max_ms - (now_ms - start_ms);

      driver->BoundedWaitFor(RewriteDriver::kWaitForCompletion, remaining_ms);
    }

    if (!fetch->done()) {
      // Note: This 5 second timeout should not actually be hit, instead
      // FetchInPlaceResource should take care of timing out our rewrites.
      LOG(DFATAL) << "In-place rewrite timed out on URL " << url;
    }
  }

  if (fetch->done() && fetch->success()) {
    ResponseHeaders* response_headers = fetch->response_headers();
    // TODO(sligocki): Add X-Mod-Pagespeed header.
    message_handler->Message(kInfo, "In-place rewrite fetch succeeded for %s",
                             url.c_str());
    send_out_headers_and_body(request, *response_headers, fetch->buffer());
    handled = true;
  } else {
    message_handler->Message(kInfo, "In-place rewrite fetch failed for %s "
                             "URL was not in cache or was not cacheable.",
                             url.c_str());
    // In-place rewrite failed, perhaps because the URL was not found in cache.
    // So we need to get it into cache, we do that using an output filter.
    // TODO(sligocki): We only want to add this output filter on cache miss
    // (not if we know it's not cacheable).
    InPlaceResourceRecorder* recorder = new InPlaceResourceRecorder(
        url, request_headers.release(), driver->options()->respect_vary(),
        server_context->http_cache(), server_context->statistics(),
        message_handler);
    ap_add_output_filter(kModPagespeedInPlaceFilterName, recorder,
                         request, request->connection);
    ap_add_output_filter(kModPagespeedInPlaceCheckHeadersName, recorder,
                         request, request->connection);
  }
  fetch->Detach();
  driver->Cleanup();

  return handled;
}

// Determines whether the url can be handled as a mod_pagespeed or in-place
// optimized resource, and handles it, returning true.  Success status is
// written to the status code in the response headers.
bool handle_as_resource(ApacheServerContext* server_context,
                        request_rec* request,
                        GoogleUrl* gurl,
                        const GoogleString& url) {
  if (!gurl->is_valid()) {
    return false;
  }

  // We must potentially poll for cache.flush (which can mutate global_options)
  // before constructing the options that we use to decide whether IPRO is
  // enabled.
  server_context->PollFilesystemForCacheFlush();

  ApacheRequestContext* apache_request_context = new ApacheRequestContext(
      server_context->thread_system()->NewMutex(), request);
  apache_request_context->set_url(url);
  RequestContextPtr request_context(apache_request_context);
  bool using_spdy = request_context->using_spdy();
  RewriteOptions* global_options = server_context->global_options();
  if (using_spdy && (server_context->SpdyConfig() != NULL)) {
    global_options = server_context->SpdyConfig();
  }

  scoped_ptr<RequestHeaders> request_headers(new RequestHeaders);
  // Filter limited request headers into backend fetch.
  // TODO(sligocki): Put this filtering in ResourceFetch and instead use:
  // ApacheRequestToRequestHeaders(*request, request_headers.get());
  for (int i = 0, n = arraysize(RewriteDriver::kPassThroughRequestAttributes);
       i < n; ++i) {
    const char* value = apr_table_get(
        request->headers_in,
        RewriteDriver::kPassThroughRequestAttributes[i]);
    if (value != NULL) {
      request_headers->Add(
          RewriteDriver::kPassThroughRequestAttributes[i], value);
    }
  }

  scoped_ptr<RewriteOptions> custom_options(get_custom_options(
      server_context, request, gurl, request_headers.get(), global_options));

  RewriteOptions* options = custom_options.get();  // Options for this request.
  if (custom_options.get() == NULL) {
    options = global_options;
  }

  // Finally, do the actual handling.
  bool handled = false;
  if (server_context->IsPagespeedResource(*gurl)) {
    handled = true;
    handle_as_pagespeed_resource(request_context, gurl, url,
                                 custom_options.release(), server_context,
                                 request_headers.release(), request);
  } else if (options->ajax_rewriting_enabled() && options->enabled() &&
             options->IsAllowed(url)) {
    handled = handle_as_in_place(request_context,gurl, url,
                                 custom_options.release(), server_context,
                                 request_headers.release(), request);
  }

  return handled;
}

// Write response headers and send out headers and output, including the option
//     for a custom Content-Type.
void write_handler_response(const StringPiece& output,
                            request_rec* request,
                            ContentType content_type,
                            const StringPiece& cache_control) {
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.set_major_version(1);
  response_headers.set_minor_version(1);

  response_headers.Add(HttpAttributes::kContentType, content_type.mime_type());
  AprTimer timer;
  int64 now_ms = timer.NowMs();
  response_headers.SetDate(now_ms);
  response_headers.SetLastModified(now_ms);
  response_headers.Add(HttpAttributes::kCacheControl, cache_control);
  send_out_headers_and_body(request, response_headers, output.as_string());
}

void write_handler_response(const StringPiece& output,
                            request_rec* request,
                            ContentType content_type) {
  write_handler_response(output, request, kContentTypeHtml,
                         HttpAttributes::kNoCache);
}

void write_handler_response(const StringPiece& output, request_rec* request) {
  write_handler_response(output, request, kContentTypeHtml);
}

// Returns request URL if it was a .pagespeed. rewritten resource URL.
// Otherwise returns NULL. Since other Apache modules can change request->uri,
// we stow the original request URL in a note. This method reads that note
// and thus should return the URL that the browser actually requested (rather
// than a mod_rewrite altered URL).
const char* get_instaweb_resource_url(request_rec* request,
                                      ApacheServerContext* server_context) {
  const char* resource = apr_table_get(request->notes, kResourceUrlNote);

  // If our translate_name hook, save_url_hook, failed to run because some
  // other module's translate_hook returned OK first, then run it now. The
  // main reason we try to do this early is to save our URL before mod_rewrite
  // mutates it.
  if (resource == NULL) {
    save_url_in_note(request, server_context);
    resource = apr_table_get(request->notes, kResourceUrlNote);
  }

  if (resource != NULL && strcmp(resource, kResourceUrlNo) == 0) {
    return NULL;
  }

  const char* url = apr_table_get(request->notes, kPagespeedOriginalUrl);
  return url;
}

void log_resource_referral(request_rec* request,
                           ApacheRewriteDriverFactory* factory) {
  // If all the pieces are in place, we log this request as a resource referral
  // for future prerender decision-making purposes
  SharedMemRefererStatistics* referer_stats =
      factory->shared_mem_referer_statistics();
  if (referer_stats != NULL) {
    const char* original_url = apr_table_get(request->notes,
                                             kPagespeedOriginalUrl);
    if (original_url != NULL) {
      const char* referer = apr_table_get(request->headers_in,
                                          HttpAttributes::kReferer);
      if (referer != NULL) {
        GoogleUrl referer_url(referer);
        GoogleUrl resource_url(original_url);
        referer_stats->LogResourceRequestWithReferer(resource_url,
                                                     referer_url);
      }
    }
  }
}

// Used by log_request_headers for testing only.
struct HeaderLoggingData {
  HeaderLoggingData(StringWriter* writer_in, MessageHandler* handler_in)
      : writer(writer_in), handler(handler_in) {}
  StringWriter* writer;
  MessageHandler* handler;
};

// Helper function to support the LogRequestHeadersHandler.  Called once for
// each header to write header data in a form suitable for javascript inlining.
// Used only for tests.
int log_request_headers(void* logging_data,
                        const char* key, const char* value) {
  HeaderLoggingData* hld = static_cast<HeaderLoggingData*>(logging_data);
  StringWriter* writer = hld->writer;
  MessageHandler* handler = hld->handler;

  GoogleString escaped_key;
  GoogleString escaped_value;

  EscapeToJsStringLiteral(key, false, &escaped_key);
  EscapeToJsStringLiteral(value, false, &escaped_value);

  writer->Write("alert(\"", handler);
  writer->Write(escaped_key, handler);
  writer->Write("=", handler);
  writer->Write(escaped_value, handler);
  writer->Write("\");\n", handler);

  return 1;  // Continue iteration.
}

// Writes text wrapped in a <pre> block
void WritePre(StringPiece str, Writer* writer, MessageHandler* handler) {
  writer->Write("<pre>\n", handler);
  writer->Write(str, handler);
  writer->Write("</pre>\n", handler);
}

void instaweb_static_handler(request_rec* request,
                             ApacheServerContext* server_context) {
  StaticJavascriptManager* static_javascript_manager =
      server_context->static_javascript_manager();
  StringPiece request_uri_path = request->parsed_uri.path;
  // Strip out the common prefix url before sending to StaticJavascriptManager.
  StringPiece file_name =
      request_uri_path.substr(
          strlen(ApacheRewriteDriverFactory::kStaticJavaScriptPrefix));
  StringPiece file_contents;
  StringPiece cache_header;
  if (static_javascript_manager->GetJsSnippet(file_name, &file_contents,
                                              &cache_header)) {
    write_handler_response(file_contents, request, kContentTypeJavascript,
                           cache_header);
  } else {
    instaweb_404_handler(request->parsed_uri.path, request);
  }
}

apr_status_t instaweb_console_handler(
    request_rec* request, ApacheConfig* config,
    ApacheMessageHandler* message_handler) {
  GoogleString output;
  StringWriter writer(&output);
  writer.Write("<!DOCTYPE html>"
                "<title>mod_pagespeed console</title>",
               message_handler);
  writer.Write("<style>", message_handler);
  writer.Write(CSS_mod_pagespeed_console_css, message_handler);
  writer.Write("</style>", message_handler);
  writer.Write(HTML_mod_pagespeed_console_body, message_handler);
  writer.Write("<script>", message_handler);
  if (config->statistics_logging_charts_js().size() > 0 &&
      config->statistics_logging_charts_css().size() > 0) {
    writer.Write("var chartsOfflineJS = '", message_handler);
    writer.Write(config->statistics_logging_charts_js(), message_handler);
    writer.Write("';", message_handler);
    writer.Write("var chartsOfflineCSS = '", message_handler);
    writer.Write(config->statistics_logging_charts_css(), message_handler);
    writer.Write("';", message_handler);
  } else {
    if (config->statistics_logging_charts_js().size() > 0 ||
        config->statistics_logging_charts_css().size() > 0) {
      message_handler->Message(kWarning, "Using online Charts API.");
    }
    writer.Write("var chartsOfflineJS, chartsOfflineCSS;", message_handler);
  }
  writer.Write(JS_mod_pagespeed_console_js, message_handler);
  writer.Write("</script>", message_handler);
  write_handler_response(output, request);
  return OK;
}

apr_status_t instaweb_statistics_handler(
    request_rec* request, ApacheServerContext* server_context,
    ApacheRewriteDriverFactory* factory, MessageHandler* message_handler) {
  bool general_stats_request =
      (strcmp(request->handler, kStatisticsHandler) == 0);
  bool global_stats_request =
      (strcmp(request->handler, kGlobalStatisticsHandler) == 0);

  int64 start_time, end_time, granularity_ms;
  std::set<GoogleString> var_titles;
  std::set<GoogleString> hist_titles;
  if (general_stats_request && !factory->use_per_vhost_statistics()) {
    global_stats_request = true;
  }

  // Choose the correct statistics.
  Statistics* statistics = global_stats_request ?
      factory->statistics() : server_context->statistics();

  QueryParams params;
  params.Parse(request->args);

  // Parse various mode query params.
  bool print_normal_config = params.Has("config");
  bool print_spdy_config = params.Has("spdy_config");

  // JSON statistics handling is done only if we have a console logger.
  bool json = false;
  if (statistics->console_logger() != NULL) {
    // Default values for start_time, end_time, and granularity_ms in case the
    // query does not include these parameters.
    start_time = 0;
    end_time = statistics->console_logger()->timer()->NowMs();
    // Granularity is the difference in ms between data points. If it is not
    // specified by the query, the default value is 3000 ms, the same as the
    // default logging granularity.
    granularity_ms = 3000;
    for (int i = 0; i < params.size(); ++i) {
      const GoogleString value =
          (params.value(i) == NULL) ? "" : *params.value(i);
      const char* name = params.name(i);
      if (strcmp(name, "json") == 0) {
        json = true;
      } else if (strcmp(name, "start_time") == 0) {
        StringToInt64(value, &start_time);
      } else if (strcmp(name, "end_time") == 0) {
        StringToInt64(value, &end_time);
      } else if (strcmp(name, "var_titles") == 0) {
        std::vector<StringPiece> variable_names;
        SplitStringPieceToVector(value, ",", &variable_names, true);
        for (size_t i = 0; i < variable_names.size(); ++i) {
          var_titles.insert(variable_names[i].as_string());
        }
      } else if (strcmp(name, "hist_titles") == 0) {
        std::vector<StringPiece> histogram_names;
        SplitStringPieceToVector(value, ",", &histogram_names, true);
        for (size_t i = 0; i < histogram_names.size(); ++i) {
          // TODO(morlovich): Cleanup & publicize UrlToFileNameEncoder::Unescape
          // and use it here, instead of this GlobalReplaceSubstring hack.
          GoogleString name = histogram_names[i].as_string();
          GlobalReplaceSubstring("%20", " ", &(name));
          hist_titles.insert(name);
        }
      } else if (strcmp(name, "granularity") == 0) {
        StringToInt64(value, &granularity_ms);
      }
    }
  }
  GoogleString output;
  StringWriter writer(&output);
  if (json) {
    statistics->console_logger()->DumpJSON(var_titles, hist_titles,
                                            start_time, end_time,
                                            granularity_ms, &writer,
                                            message_handler);
  } else {
    // Generate some navigational links to the right to help
    // our users get to other modes.
    writer.Write(
        "<div style='float:right'>View "
        "<a href='?config'>Configuration</a>, "
        "<a href='?spdy_config'>SPDY Configuration</a>, "
        "<a href='?'>Statistics</a> "
        "(<a href='?memcached'>with memcached Stats</a>). "
        "</div>",
        message_handler);

    // Only print stats or configuration, not both.
    if (!print_normal_config && !print_spdy_config) {
      writer.Write(global_stats_request ?
                       "Global Statistics" : "VHost-Specific Statistics",
                   message_handler);

      // Write <pre></pre> for Dump to keep good format.
      writer.Write("<pre>", message_handler);
      statistics->Dump(&writer, message_handler);
      writer.Write("</pre>", message_handler);
      statistics->RenderHistograms(&writer, message_handler);

      if (global_stats_request) {
        // We don't want to print this in per-vhost info since it would leak
        // all the declared caches.
        GoogleString shm_stats;
        factory->PrintShmMetadataCacheStats(&shm_stats);
        writer.Write(shm_stats, message_handler);
      }

      if (params.Has("memcached")) {
        GoogleString memcached_stats;
        factory->PrintMemCacheStats(&memcached_stats);
        if (!memcached_stats.empty()) {
          WritePre(memcached_stats, &writer, message_handler);
        }
      }
    }

    if (print_normal_config) {
      writer.Write("Configuration:<br>", message_handler);
      WritePre(server_context->config()->OptionsToString(),
               &writer, message_handler);
    }

    if (print_spdy_config) {
      ApacheConfig* spdy_config = server_context->SpdyConfig();
      if (spdy_config == NULL) {
        writer.Write("SPDY-specific configuration missing, using default.",
                      message_handler);
      } else {
        writer.Write("SPDY-specific configuration:<br>", message_handler);
        WritePre(spdy_config->OptionsToString(), &writer, message_handler);
      }
    }
  }

  if (json) {
    write_handler_response(output, request, kContentTypeJson);
  } else {
    write_handler_response(output, request);
  }
  return OK;
}

}  // namespace

bool is_pagespeed_subrequest(request_rec* request) {
  StringPiece user_agent = apr_table_get(request->headers_in,
                                         HttpAttributes::kUserAgent);
  return (user_agent.find(kModPagespeedSubrequestUserAgent) != user_agent.npos);
}

apr_status_t instaweb_handler(request_rec* request) {
  apr_status_t ret = DECLINED;
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  ApacheConfig* config = server_context->config();
  // Escape ASAP if we're in unplugged mode.
  if (config->unplugged()) {
    return DECLINED;
  }

  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  ApacheMessageHandler* message_handler = factory->apache_message_handler();
  StringPiece request_handler_str = request->handler;

  log_resource_referral(request, factory);

  // mod_pagespeed_statistics or mod_pagespeed_global_statistics.
  if (request_handler_str == kStatisticsHandler ||
      request_handler_str == kGlobalStatisticsHandler) {
    ret = instaweb_statistics_handler(request, server_context, factory,
                                      message_handler);

  } else if (request_handler_str == kRefererStatisticsHandler) {
    GoogleString output;
    StringWriter writer(&output);
    factory->DumpRefererStatistics(&writer);
    write_handler_response(output, request);
    ret = OK;

  } else if (request_handler_str == kConsoleHandler) {
    ret = instaweb_console_handler(request, config, message_handler);

  } else if (request_handler_str == kMessageHandler) {
    // Request for page /mod_pagespeed_message.
    GoogleString output;
    StringWriter writer(&output);
    // Write <pre></pre> for Dump to keep good format.
    writer.Write("<pre>", message_handler);
    if (!message_handler->Dump(&writer)) {
      writer.Write("Writing to mod_pagespeed_message failed. \n"
                   "Please check if it's enabled in pagespeed.conf.\n",
                   message_handler);
    }
    writer.Write("</pre>", message_handler);
    write_handler_response(output, request);
    ret = OK;

  } else if (request_handler_str == kBeaconHandler) {
    RequestContextPtr request_context(new ApacheRequestContext(
        server_context->thread_system()->NewMutex(), request));
    server_context->HandleBeacon(request->unparsed_uri, request_context);
    ret = HTTP_NO_CONTENT;

  } else if (request_handler_str == kLogRequestHeadersHandler) {
    // For testing ModPagespeedCustomFetchHeader.
    GoogleString output;
    StringWriter writer(&output);
    HeaderLoggingData header_logging_data(&writer, message_handler);
    apr_table_do(&log_request_headers, &header_logging_data,
                 request->headers_in, NULL);

    write_handler_response(output, request, kContentTypeJavascript, "public");
    ret = OK;

  } else if (strcmp(request->handler, kGenerateResponseWithOptionsHandler) == 0
             && request->uri != NULL) {
    // This handler is only needed for apache_system_test. It adds headers to
    // headers_out and/or err_headers_out to test handling of parameters in
    // those resources.
    if (strstr(request->parsed_uri.query, "headers_out") != NULL) {
      apr_table_add(request->headers_out, "ModPagespeed", "off");
    } else if (strstr(request->parsed_uri.query, "headers_errout") != NULL) {
      apr_table_add(request->err_headers_out, "ModPagespeed", "off");
    } else if (strstr(request->parsed_uri.query, "headers_override") != NULL) {
      apr_table_add(request->headers_out, "ModPagespeed", "off");
      apr_table_add(request->headers_out, "ModPagespeedFilters",
                    "-remove_comments");
      apr_table_add(request->err_headers_out, "ModPagespeed", "on");
      apr_table_add(request->err_headers_out, "ModPagespeedFilters",
                    "+remove_comments");
    } else if (strstr(request->parsed_uri.query, "headers_combine") != NULL) {
      apr_table_add(request->headers_out, "ModPagespeed", "on");
      apr_table_add(request->err_headers_out, "ModPagespeedFilters",
                    "+remove_comments");
    }

  } else {
    const char* url = InstawebContext::MakeRequestUrl(*config, request);
    // Do not try to rewrite our own sub-request.
    if (url != NULL && !is_pagespeed_subrequest(request)) {
      GoogleUrl gurl(url);
      // Only handle GET request
      if (request->method_number != M_GET) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                      "Not rewriting non-GET request: %d.",
                      request->method_number);
      } else if (!gurl.is_valid()) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                      "Ignoring invalid URL: %s", gurl.spec_c_str());
      } else if (gurl.PathSansLeaf() ==
                 ApacheRewriteDriverFactory::kStaticJavaScriptPrefix) {
        instaweb_static_handler(request, server_context);
        ret = OK;
      } else if (handle_as_resource(server_context, request, &gurl, url)) {
        ret = OK;
      }
    }

    if (ret != OK && (config->slurping_enabled() || config->test_proxy())) {
      SlurpUrl(server_context, request);
      if (request->status == HTTP_NOT_FOUND) {
        RewriteStats* stats = server_context->rewrite_stats();
        stats->slurp_404_count()->Add(1);
      }
      ret = OK;
    }
  }
  return ret;
}

// This translator must be inserted into the translate_name chain
// prior to mod_rewrite.  By saving the original URL in a
// request->notes and using that in our handler, we prevent
// mod_rewrite from borking URL names that need to be handled by
// mod_pagespeed.
//
// This hack seems to be the most robust way to immunize mod_pagespeed
// from when mod_rewrite rewrites the URL.  We still need mod_rewrite
// to do required complex processing of the filename (e.g. prepending
// the DocumentRoot) so mod_authz_host is happy, so we return DECLINED
// even for mod_pagespeed resources.
//
// One alternative strategy is to return OK to bypass mod_rewrite
// entirely, but then we'd have to duplicate the functionality in
// mod_rewrite that prepends the DocumentRoot, which is itself
// complex.  See mod_rewrite.c:hook_fixup(), and look for calls to
// ap_document_root().
//
// Or we could return DECLINED but set a note "mod_rewrite_rewritten"
// to try to convince mod_rewrite to leave our URLs alone, which seems
// fragile as that's an internal string literal in mod_rewrite.c and
// is not documented anywhere.
//
// Another strategy is to return OK but leave request->filename NULL.
// In that case, the server kernel generates an ominious 'info'
// message:
//
//     [info] [client ::1] Module bug?  Request filename is missing for URI
//     /mod_pagespeed_statistics
//
// This is generated by httpd/src/server/request.c line 486, and right
// above that is this comment:
//
//     "OK" as a response to a real problem is not _OK_, but to
//     allow broken modules to proceed, we will permit the
//     not-a-path filename to pass the following two tests.  This
//     behavior may be revoked in future versions of Apache.  We
//     still must catch it later if it's heading for the core
//     handler.  Leave INFO notes here for module debugging.
//
// It seems like the simplest, most robust approach is to squirrel
// away the original URL *before* mod_rewrite sees it in
// kPagespeedOriginalUrl "mod_pagespeed_url" and use *that* rather than
// request->unparsed_uri (which mod_rewrite might have mangled) when
// procesing the request.
//
// Additionally we store whether or not this request is a pagespeed
// resource or not in kResourceUrlNote.
apr_status_t save_url_hook(request_rec *request) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  return save_url_in_note(request, server_context);
}

apr_status_t save_url_in_note(request_rec *request,
                              ApacheServerContext* server_context) {
  // Escape ASAP if we're in unplugged mode.
  if (server_context->config()->unplugged()) {
    return DECLINED;
  }

  // This call to MakeRequestUrl() not only returns the url but also
  // saves it for future use so that if another module changes the
  // url in the request, we still have the original one.
  const char* url = InstawebContext::MakeRequestUrl(
      *server_context->global_options(), request);
  GoogleUrl gurl(url);

  bool bypass_mod_rewrite = false;
  if (gurl.is_valid()) {
    // Note: We cannot use request->handler because it may not be set yet :(
    // TODO(sligocki): Make this robust to custom statistics and beacon URLs.
    StringPiece leaf = gurl.LeafSansQuery();
    if (leaf == kStatisticsHandler || leaf == kConsoleHandler ||
        leaf == kGlobalStatisticsHandler || leaf == kBeaconHandler ||
        leaf == kMessageHandler || leaf == kRefererStatisticsHandler ||
        (gurl.PathSansLeaf() ==
         ApacheRewriteDriverFactory::kStaticJavaScriptPrefix)) {
      bypass_mod_rewrite = true;
    } else {
      if (server_context->IsPagespeedResource(gurl)) {
        bypass_mod_rewrite = true;
      }
    }
  }

  if (bypass_mod_rewrite) {
    apr_table_set(request->notes, kResourceUrlNote, kResourceUrlYes);
  } else {
    // Leave behind a note for non-instaweb requests that says that
    // our handler got called and we decided to pass.  This gives us
    // one final chance at serving resources in the presence of a
    // module that intercepted 'translate_name' before mod_pagespeed.
    // The absence of this marker indicates that translate_name did
    // not get a chance to run, and thus we should try to look at
    // the URI directly.
    apr_table_set(request->notes, kResourceUrlNote, kResourceUrlNo);
  }
  return DECLINED;
}

// Override core_map_to_storage for pagespeed resources.
apr_status_t instaweb_map_to_storage(request_rec* request) {
  if (request->proxyreq == PROXYREQ_REVERSE) {
    // If Apache is acting as a reverse proxy for this request there is no
    // point in walking the directory because it doesn't apply to this
    // server's htdocs tree, it applies to the server we are proxying to.
    // This can result in it raising a 403 because some path doesn't exist.
    // Note that experimenting shows that it doesn't matter if we return OK
    // or DECLINED here, at least with URLs that aren't overly long; also,
    // we actually fetch the DECODED URL (no .pagespeed. etc) from the proxy
    // server and we rewrite it ourselves.
    return DECLINED;
  }

  if (request->filename == NULL) {
    // We set filename to NULL below, and it appears other modules do too
    // (the WebSphere plugin for example; see issue 610), so to prevent a
    // dereference of NULL.
    return DECLINED;
  }

  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  if (server_context->config()->unplugged()) {
    // If we're in unplugged mode then none of our hooks apply so escape ASAP.
    return DECLINED;
  }

  if (get_instaweb_resource_url(request, server_context) == NULL) {
    return DECLINED;
  }

  // core_map_to_storage does at least two things:
  //  1) checks filename length limits
  //  2) determines directory specific options
  // We want (2) but not (1).  If we simply return OK we will keep
  // core_map_to_storage from running and let through our long filenames but
  // resource requests that require regeneration will not respect directory
  // specific options.
  //
  // To fix this we need to be more dependent on apache internals than we
  // would like.  core_map_to_storage always calls ap_directory_walk(request),
  // which does both (1) and (2) and appears to work entirely off of
  // request->filename.  But ap_directory_walk doesn't care whether the last
  // request->segment of the path actually exists.  So if we change the
  // request->filename from something like:
  //    /var/www/path/to/LEAF_WHICH_MAY_BE_HUGE.pagespeed.FILTER.HASH.EXT
  // to:
  //    /var/www/path/to/A
  // then we will bypass the filename length limit without harming the load of
  // directory specific options.
  //
  // So: modify request->filename in place to cut it off after the last '/'
  // character and replace the whole leaf with 'A', and then call
  // ap_directory_walk to figure out custom options.
  char* filename_starting_at_last_slash = strrchr(request->filename, '/');
  if (filename_starting_at_last_slash != NULL &&
      filename_starting_at_last_slash[1] != '\0') {
    filename_starting_at_last_slash[1] = 'A';
    filename_starting_at_last_slash[2] = '\0';
  }
  ap_directory_walk(request);

  // mod_speling, if enabled, looks for the filename on the file system,
  // and tries to "correct" the spelling.  This is not desired for
  // mod_pagesped resources, but mod_speling will not do this damage
  // when request->filename == NULL.  See line 219 of
  // http://svn.apache.org/viewvc/httpd/httpd/trunk/modules/mappers/
  // mod_speling.c?revision=983065&view=markup
  //
  // Note that mod_speling runs 'hook_fixups' at APR_HOOK_LAST, and
  // we are currently running instaweb_map_to_storage in map_to_storage
  // HOOK_FIRST-2, which is a couple of phases before hook_fixups.
  //
  // If at some point we stop NULLing the filename here we need to modify the
  // code above that mangles it to use a temporary buffer instead.
  request->filename = NULL;

  // While setting request->filename helps get mod_speling (as well as
  // mod_mime and mod_mime_magic) out of our hair, it causes crashes
  // in mod_negotiation (if on) when finfo.filetype is APR_NOFILE.
  // So we give it a type that's something other than APR_NOFILE (plus we
  // also don't want APR_DIR, since that would make mod_mime to set the
  // mimetype to httpd/unix-directory).
  request->finfo.filetype = APR_UNKFILE;

  // Keep core_map_to_storage from running and rejecting our long filenames.
  return OK;
}

}  // namespace net_instaweb
