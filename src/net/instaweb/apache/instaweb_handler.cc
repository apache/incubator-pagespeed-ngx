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
#include "net/instaweb/apache/header_util.h"
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
#include "net/instaweb/util/public/time_util.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

namespace net_instaweb {

namespace {

const char kRepairCachingHeader[] = "X-Mod-Pagespeed-Repair";

bool IsCompressibleContentType(const char* content_type) {
  if (content_type == NULL) {
    return false;
  }
  std::string type = content_type;
  size_t separator_idx = type.find(";");
  if (separator_idx != std::string::npos) {
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
void instaweb_default_handler(const std::string& url, request_rec* request) {
  request->status = HTTP_NOT_FOUND;
  ap_set_content_type(request, "text/html; charset=utf-8");
  ap_rputs("<html><head><title>Not Found</title></head>", request);
  ap_rputs("<body><h1>Apache server with mod_pagespeed</h1>OK", request);
  ap_rputs("<hr>NOT FOUND:", request);
  ap_rputs(url.c_str(), request);
  ap_rputs("</body></html>", request);
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
  RewriteDriver* rewrite_driver = factory->NewRewriteDriver();

  SimpleMetaData request_headers, response_headers;
  std::string output;  // TODO(jmarantz): quit buffering resource output
  StringWriter writer(&output);
  MessageHandler* message_handler = factory->message_handler();
  SerfAsyncCallback* callback = new SerfAsyncCallback();
  bool handled = rewrite_driver->FetchResource(
      url, request_headers, &response_headers, &writer, message_handler,
      callback);
  if (handled) {
    message_handler->Message(kInfo, "Fetching resource %s...", url.c_str());
    if (!callback->done()) {
      SerfUrlAsyncFetcher* serf_async_fetcher =
          factory->serf_url_async_fetcher();
      AprTimer timer;
      int64 max_ms = factory->fetcher_time_out_ms();
      for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
           !callback->done() && now_ms - start_ms < max_ms;
           now_ms = timer.NowMs()) {
        int64 remaining_us = max_ms - (now_ms - start_ms);
        serf_async_fetcher->Poll(remaining_us);
      }

      if (!callback->done()) {
        message_handler->Message(kError, "Timeout on url %s", url.c_str());
      }
    }
    if (callback->success()) {
      message_handler->Message(kInfo, "Fetch succeeded for %s, status=%d",
                              url.c_str(), response_headers.status_code());
      send_out_headers_and_body(request, response_headers, output);
    } else {
      message_handler->Message(kError, "Fetch failed for %s, status=%d",
                              url.c_str(), response_headers.status_code());
      factory->Increment404Count();
      instaweb_default_handler(url, request);
    }
  }
  callback->Release();
  factory->ReleaseRewriteDriver(rewrite_driver);
  return handled;
}

void send_out_headers_and_body(
    request_rec* request,
    const SimpleMetaData& response_headers,
    const std::string& output) {
  for (int idx = 0; idx < response_headers.NumAttributes(); ++idx) {
    const char* name = response_headers.Name(idx);
    const char* value = response_headers.Value(idx);
    if (strcasecmp(name, HttpAttributes::kContentType) == 0) {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(request->pool, value);
      ap_set_content_type(request, ptr);
    } else {
      if (strcasecmp(name, HttpAttributes::kCacheControl) == 0) {
        // In case Apache configuration directives set up caching headers,
        // we will need override our cache-extended resources in a late-running
        // output filter.
        // TODO(jmarantz): Do not use headers_out as out message passing
        // mechanism. Switch to configuration vectors or something like that
        // instead.
        apr_table_add(request->headers_out, kRepairCachingHeader, value);
        // Add the repair headers filter to fix the cacheing header.
        ap_add_output_filter("MOD_PAGESPEED_REPAIR_HEADERS", NULL, request,
                             request->connection);
      }
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(request->headers_out, name, value);
    }
  }
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

}  // namespace

apr_status_t repair_caching_header(ap_filter_t *filter,
                                   apr_bucket_brigade *bb) {
  request_rec* request = filter->r;
  SimpleMetaData response_headers;
  ApacheHeaderToMetaData(request->headers_out, request->status,
                         request->proto_num, &response_headers);

  // TODO(jmarantz): Define a better policy, or at least document this
  // one, for what we do when we do a meta-data lookup and find more
  // than one response for our query.  In this case the query is our
  // special "repair" header and something would be very wrong if
  // there was more than one.
  CharStarVector v;
  if (response_headers.Lookup(kRepairCachingHeader, &v) && (v.size() == 1)) {
    apr_table_unset(request->headers_out, kRepairCachingHeader);
    std::string desired_cache_control(v[0]);
    apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                  desired_cache_control.c_str());

    // Convert our own cache-control data into an Expires header.
    apr_table_unset(request->headers_out, HttpAttributes::kExpires);
    response_headers.RemoveAll(HttpAttributes::kCacheControl);
    response_headers.Add(HttpAttributes::kCacheControl,
                         desired_cache_control);
    response_headers.ComputeCaching();
    if (response_headers.IsCacheable()) {
      int64 expire_time_ms = response_headers.CacheExpirationTimeMs();
      if (expire_time_ms > 0) {
        std::string time_string;
        if (ConvertTimeToString(expire_time_ms, &time_string)) {
          apr_table_set(request->headers_out, HttpAttributes::kExpires,
                        time_string.c_str());
        }
      }
    }
  }
  ap_remove_output_filter(filter);
  return ap_pass_brigade(filter->next, bb);
}

int instaweb_handler(request_rec* request) {
  ApacheRewriteDriverFactory* factory =
      InstawebContext::Factory(request->server);
  int ret = OK;

  // Only handle GET request
  if (request->method_number != M_GET) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Not GET request: %d.", request->method_number);
    ret = DECLINED;
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
    RewriteDriver* driver = factory->NewRewriteDriver();
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
        ret = DECLINED;
      }
    }
  }
  return ret;
}

}  // namespace net_instaweb
