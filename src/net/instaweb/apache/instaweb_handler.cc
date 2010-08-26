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

#include "net/instaweb/apache/instaweb_handler.h"

#include <string>
#include "apr_strings.h"
#include "base/string_util.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/html_parser_message_handler.h"
#include "net/instaweb/apache/html_rewriter.h"
#include "net/instaweb/apache/html_rewriter_config.h"
#include "net/instaweb/apache/pagespeed_server_context.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "mod_spdy/apache/log_message_handler.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_writer.h"
// The httpd header must be after the pagepseed_server_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

namespace {

// Callback for asynchronous fetch. When instaweb handles a request, it will
// fetch the resource for that request with an asynchronous fetcher. We need to
// run the Poll method periodically to check if the fetch finishes.
class AsyncCallback : public net_instaweb::UrlAsyncFetcher::Callback {
 public:
  explicit AsyncCallback(net_instaweb::MessageHandler* message_handler)
      : done_(false),
        success_(false),
        message_handler_(message_handler) {}
  virtual ~AsyncCallback() {}
  virtual void Done(bool success)  {
    done_ = true;
    success_ = success;
  }
  bool done() const { return done_; }
  bool success() const { return success_; }
 private:
  bool done_;
  bool success_;
  net_instaweb::MessageHandler* message_handler_;
};

// Default handler when the file is not found
int instaweb_default_handler(const std::string& url, request_rec* request) {
  request->status = HTTP_NOT_FOUND;
  ap_set_content_type(request, "text/html; charset=utf-8");
  ap_rputs("<html><head><title>Wow, instaweb server</title></head>", request);
  ap_rputs("<body><h1>Instaweb Server is running</h1>OK", request);
  ap_rputs("<hr>NOT FOUND:", request);
  ap_rputs(url.c_str(), request);
  ap_rputs("</body></html>", request);
  return OK;
}

// Check if the request is for instaweb. If it is, set the instaweb resource.
// A instaweb resource string is the full url without the URL prefix. For
// example, http://localhost:9999/cache/cache_pre_cc.8736a.css, and the prefix
// is http://localhost:9999/cache/cache_pre_, then the resource string is
// cc.8736a.css.
int instaweb_check_request(request_rec* request, std::string* resource) {
  // Check if the request is for instaweb content generator
  // Decline the request so that other handler may process
  if (!request->handler || strcmp(request->handler, "instaweb")) {
    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, request,
                  "Not instaweb request: %s.", request->handler);
    return DECLINED;
  }

  // Only handle GET request
  if (request->method_number != M_GET) {
    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, request,
                  "Not GET request: %d.", request->method_number);
    return HTTP_METHOD_NOT_ALLOWED;
  }
  /*
   * In some contexts we are seeing relative URLs passed
   * into request->unparsed_uri.  But when using mod_slurp, the rewritten
   * HTML contains complete URLs, so this construction yields the host:port
   * prefix twice.
   *
   * TODO(jmarantz): Figure out how to do this correctly at all times.
   */
  std::string full_url;
  if (strncmp(request->unparsed_uri, "http:", 5) == 0) {
    full_url = request->unparsed_uri;
  } else {
    full_url = ap_construct_url(request->pool,
                                request->unparsed_uri,
                                request);
  }
  html_rewriter::PageSpeedServerContext* context =
      html_rewriter::mod_pagespeed_get_config_server_context(request->server);
  std::string url_prefix = html_rewriter::GetUrlPrefix(context);
  if (full_url.compare(0, url_prefix.size(), url_prefix) != 0) {
    ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                  "INSTAWEB: Declined request %s", full_url.c_str());
    return DECLINED;
  }
  *resource = full_url.substr(url_prefix.size());
  return OK;
}

bool fetch_resource(const request_rec* request,
                    const std::string& resource,
                    net_instaweb::SimpleMetaData* response_headers,
                    std::string* output) {
  html_rewriter::PageSpeedServerContext* context =
      html_rewriter::mod_pagespeed_get_config_server_context(request->server);
  net_instaweb::RewriteDriver* rewrite_driver =
      context->rewrite_driver_factory()->GetRewriteDriver();
  net_instaweb::SimpleMetaData request_headers;
  net_instaweb::StringWriter writer(output);
  html_rewriter::HtmlParserMessageHandler message_handler;
  AsyncCallback callback(&message_handler);

  message_handler.Message(net_instaweb::kWarning, "Fetching resource %s...",
                          resource.c_str());

  rewrite_driver->FetchResource(resource, request_headers,
                                response_headers,
                                &writer,
                                &message_handler,
                                &callback);
  bool ret = callback.done() && callback.success();
  if (!ret) {
    html_rewriter::SerfUrlAsyncFetcher* serf_async_fetcher =
        context->rewrite_driver_factory()->serf_url_async_fetcher();
    html_rewriter::AprTimer timer;
    int64 max_ms = html_rewriter::GetResourceFetcherTimeOutMs(context);
    for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
         !callback.done() && now_ms - start_ms < max_ms;
         now_ms = timer.NowMs()) {
      int64 remaining_us = max_ms - (now_ms - start_ms);
      serf_async_fetcher->Poll(remaining_us);
    }
    if (!callback.done()) {
      message_handler.Error(resource.c_str(), 0,
                            "Timeout waiting for response");
    } else if (!callback.success()) {
      message_handler.Error(resource.c_str(), 0, "Fetch failed.");
    }
    ret = callback.done() && callback.success();
  }
  message_handler.Message(net_instaweb::kWarning,
                          "...Fetched resource %s, ret=%d",
                          resource.c_str(), ret);
  return ret;
}

void send_out_headers_and_body(
    request_rec* request,
    const net_instaweb::SimpleMetaData& response_headers,
    const std::string& output) {
  for (int idx = 0; idx < response_headers.NumAttributes(); ++idx) {
    std::string lowercase_header(response_headers.Name(idx));
    // Force to lower case.
    StringToLowerASCII(&lowercase_header);
    if (lowercase_header == "content-type") {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(request->pool, response_headers.Value(idx));
      ap_set_content_type(request, ptr);
    } else {
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(request->headers_out,
                    response_headers.Name(idx),
                    response_headers.Value(idx));
    }
  }
  // Recompute the content-length, because the content may have changed.
  ap_set_content_length(request, output.size());
  // Send the body
  ap_rwrite(output.c_str(), output.size(), request);
}

}  // namespace

namespace mod_pagespeed {

int instaweb_handler(request_rec* request) {
  std::string resource;
  int ret = instaweb_check_request(request, &resource);
  if (ret != OK) {
    return ret;
  }
  net_instaweb::SimpleMetaData response_headers;
  std::string output;
  if (!fetch_resource(request, resource, &response_headers, &output)) {
    return instaweb_default_handler(resource, request);
  }
  send_out_headers_and_body(request, response_headers, output);
  return OK;
}

}  // namespace mod_pagespeed
