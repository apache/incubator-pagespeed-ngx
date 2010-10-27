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

#include "apr_strings.h"
#include "base/basictypes.h"
#include "base/string_util.h"
#include "net/instaweb/apache/apache_slurp.h"
#include "net/instaweb/apache/apr_statistics.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
// The httpd header must be after the pagepseed_server_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

namespace net_instaweb {

namespace {

// Callback for asynchronous fetch. When instaweb handles a request, it will
// fetch the resource for that request with an asynchronous fetcher. We need to
// run the Poll method periodically to check if the fetch finishes.
//
// TODO(jmarantz): copy Release() code from SerfUrlFetcher to avoid
// corrupting stack on a timeout.
class AsyncCallback : public UrlAsyncFetcher::Callback {
 public:
  explicit AsyncCallback(MessageHandler* message_handler)
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
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AsyncCallback);
};

// Default handler when the file is not found
int instaweb_default_handler(const std::string& url, request_rec* request) {
  request->status = HTTP_NOT_FOUND;
  ap_set_content_type(request, "text/html; charset=utf-8");
  ap_rputs("<html><head><title>Not Found</title></head>", request);
  ap_rputs("<body><h1>Apache server with mod_pagespeed</h1>OK", request);
  ap_rputs("<hr>NOT FOUND:", request);
  ap_rputs(url.c_str(), request);
  ap_rputs("</body></html>", request);
  return OK;
}

// predeclare to minimize diffs for now.  TODO(jmarantz): reorder
void send_out_headers_and_body(
    request_rec* request,
    const SimpleMetaData& response_headers,
    const std::string& output);

// Determines whether the url can be handled as a mod_pagespeed resource,
// and handles it, returning true.  A 'true' routine means that this
// method believed the URL was a mod_pagespeed resource -- it does not
// imply that it was handled successfully.  That information will be
// in the status code in the response headers.
bool handle_as_resource(ApacheRewriteDriverFactory* factory,
                        request_rec* request,
                        const std::string& url) {
  RewriteDriver* rewrite_driver = factory->GetRewriteDriver();

  SimpleMetaData request_headers, response_headers;
  std::string output;  // TODO(jmarantz): quit buffering resource output
  StringWriter writer(&output);
  GoogleMessageHandler message_handler;
  AsyncCallback callback(&message_handler);
  bool handled = rewrite_driver->FetchResource(
      url, request_headers, &response_headers, &writer, &message_handler,
      &callback);
  if (handled) {
    message_handler.Message(kWarning, "Fetching resource %s...", url.c_str());
    if (!callback.done()) {
      SerfUrlAsyncFetcher* serf_async_fetcher =
          factory->serf_url_async_fetcher();
      AprTimer timer;
      int64 max_ms = factory->fetcher_time_out_ms();
      for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
           !callback.done() && now_ms - start_ms < max_ms;
           now_ms = timer.NowMs()) {
        int64 remaining_us = max_ms - (now_ms - start_ms);
        serf_async_fetcher->Poll(remaining_us);
      }

      if (!callback.done()) {
        message_handler.Message(kError, "Timeout on url %s", url.c_str());
      }
    }
    if (callback.success()) {
      message_handler.Message(kInfo, "Fetch succeeded for %s, status=%d",
                              url.c_str(), response_headers.status_code());
      send_out_headers_and_body(request, response_headers, output);
    } else {
      message_handler.Message(kError, "Fetch failed for %s, status=%d",
                              url.c_str(), response_headers.status_code());
      factory->Increment404Count();
      return instaweb_default_handler(url, request);
    }
  }
  return handled;
}

void send_out_headers_and_body(
    request_rec* request,
    const SimpleMetaData& response_headers,
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

int instaweb_handler(request_rec* request) {
  ApacheRewriteDriverFactory* factory =
      InstawebContext::Factory(request->server);
  int ret = OK;

  // Only handle GET request
  if (request->method_number != M_GET) {
    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, request,
                  "Not GET request: %d.", request->method_number);
    ret = HTTP_METHOD_NOT_ALLOWED;
  } else if (strcmp(request->handler, "mod_pagespeed_statistics") == 0) {
    std::string output;
    SimpleMetaData response_headers;
    StringWriter writer(&output);
    AprStatistics* statistics = factory->statistics();
    if (statistics) {
      statistics->Dump(&writer, factory->message_handler());
    }
    send_out_headers_and_body(request, response_headers, output);
  } else if (strcmp(request->handler, "mod_pagespeed_beacon") == 0) {
    RewriteDriver* driver = factory->GetRewriteDriver();
    AddInstrumentationFilter* aif = driver->add_instrumentation_filter();
    if (aif && aif->HandleBeacon(request->unparsed_uri)) {
      ret = HTTP_NO_CONTENT;
    } else {
      ret = DECLINED;
    }
    factory->ReleaseRewriteDriver(driver);
  } else {
    /*
     * In some contexts we are seeing relative URLs passed
     * into request->unparsed_uri.  But when using mod_slurp, the rewritten
     * HTML contains complete URLs, so this construction yields the host:port
     * prefix twice.
     *
     * TODO(jmarantz): Figure out how to do this correctly at all times.
     */
    std::string url;
    if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
      url = request->unparsed_uri;
    } else {
      url = ap_construct_url(request->pool, request->unparsed_uri, request);
    }

    if (!handle_as_resource(factory, request, url)) {
      if (factory->slurping_enabled()) {
        SlurpUrl(url, factory, request);
        if (request->status == HTTP_NOT_FOUND) {
          factory->IncrementSlurpCount();
        }
      } else {
        ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                      "mod_pagespeed: Declined request %s", url.c_str());
        ret = DECLINED;
      }
    }
  }
  return ret;
}

}  // namespace net_instaweb
