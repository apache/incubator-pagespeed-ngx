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
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apache_slurp.h"
#include "net/instaweb/apache/apr_statistics.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/serf_async_callback.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

namespace net_instaweb {

namespace {

const char kStatisticsHandler[] = "mod_pagespeed_statistics";
const char kBeaconHandler[] = "mod_pagespeed_beacon";
const char kResourceUrlNote[] = "mod_pagespeed_resource";

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
  int n = arraysize(RewriteDriver::kPassThroughRequestAttributes);
  for (int i = 0; i < n; ++i) {
    const char* value = apr_table_get(
        request->headers_in,
        RewriteDriver::kPassThroughRequestAttributes[i]);
    if (value != NULL) {
      request_headers.Add(RewriteDriver::kPassThroughRequestAttributes[i],
                          value);
    }
  }
  std::string output;  // TODO(jmarantz): quit buffering resource output
  StringWriter writer(&output);
  MessageHandler* message_handler = factory->message_handler();
  SerfAsyncCallback* callback = new SerfAsyncCallback(
      &response_headers, &writer);
  bool handled = rewrite_driver->FetchResource(
      url, request_headers, callback->response_headers(), callback->writer(),
      message_handler, callback);
  if (handled) {
    AprTimer timer;
    message_handler->Message(kInfo, "Fetching resource %s...", url.c_str());
    if (!callback->done()) {
      UrlPollableAsyncFetcher* sub_resource_fetcher =
          factory->SubResourceFetcher();
      int64 max_ms = factory->fetcher_time_out_ms();
      for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
           !callback->done() && now_ms - start_ms < max_ms;
           now_ms = timer.NowMs()) {
        int64 remaining_us = max_ms - (now_ms - start_ms);
        sub_resource_fetcher->Poll(remaining_us);
      }

      if (!callback->done()) {
        message_handler->Message(kError, "Timeout on url %s", url.c_str());
      }
    }
    response_headers.SetDate(timer.NowMs());
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
  } else {
    callback->Done(false);
  }
  callback->Release();
  factory->ReleaseRewriteDriver(rewrite_driver);
  return handled;
}

void send_out_headers_and_body(
    request_rec* request,
    const SimpleMetaData& response_headers,
    const std::string& output) {
  MetaDataToApacheHeader(response_headers, request);
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

apr_status_t instaweb_handler(request_rec* request) {
  apr_status_t ret = DECLINED;
  const char* url = apr_table_get(request->notes, kResourceUrlNote);
  if (url != NULL) {
    ApacheRewriteDriverFactory* factory =
        InstawebContext::Factory(request->server);
    ret = OK;

    // Only handle GET request
    if (request->method_number != M_GET) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                    "Not GET request: %d.", request->method_number);
      ret = DECLINED;
    } else if (strcmp(request->handler, kStatisticsHandler) == 0) {
      std::string output;
      SimpleMetaData response_headers;
      StringWriter writer(&output);
      AprStatistics* statistics = factory->statistics();
      if (statistics) {
        statistics->Dump(&writer, factory->message_handler());
      }
      response_headers.SetStatusAndReason(HttpStatus::kOK);
      response_headers.set_major_version(1);
      response_headers.set_minor_version(1);
      response_headers.Add(HttpAttributes::kContentType, "text/plain");
      AprTimer timer;
      int64 now_ms = timer.NowMs();
      response_headers.SetDate(now_ms);
      response_headers.SetLastModified(now_ms);
      response_headers.Add(HttpAttributes::kCacheControl,
                           HttpAttributes::kNoCache);
      send_out_headers_and_body(request, response_headers, output);
    } else if (strcmp(request->handler, kBeaconHandler) == 0) {
      RewriteDriver* driver = factory->NewRewriteDriver();
      AddInstrumentationFilter* aif = driver->add_instrumentation_filter();
      if (aif && aif->HandleBeacon(request->unparsed_uri)) {
        ret = HTTP_NO_CONTENT;
      } else {
        ret = DECLINED;
      }
      factory->ReleaseRewriteDriver(driver);
    } else {
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
// is not documented anywhwere.
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
// kResourceUrlNote "mod_pagespeed_url" and use *that* rather than
// request->unparsed_uri (which mod_rewrite might have mangled) when
// procesing the request.
apr_status_t save_url_for_instaweb_handler(request_rec *request) {
  char* url = NULL;
  bool need_copy = true;

  /*
   * In some contexts we are seeing relative URLs passed
   * into request->unparsed_uri.  But when using mod_slurp, the rewritten
   * HTML contains complete URLs, so this construction yields the host:port
   * prefix twice.
   *
   * TODO(jmarantz): Figure out how to do this correctly at all times.
   */
  if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
    url = request->unparsed_uri;
  } else {
    url = ap_construct_url(request->pool, request->unparsed_uri, request);
    need_copy = false;
  }

  StringPiece url_piece(url);
  bool bypass_mod_rewrite = false;
  if (url_piece.ends_with(kStatisticsHandler) ||
      url_piece.ends_with(kBeaconHandler)) {
    bypass_mod_rewrite = true;
  } else {
    ApacheRewriteDriverFactory* factory =
        InstawebContext::Factory(request->server);
    RewriteDriver* rewrite_driver = factory->NewRewriteDriver();
    RewriteFilter* filter;
    scoped_ptr<OutputResource> output_resource(
        rewrite_driver->DecodeOutputResource(url, &filter));
    if (output_resource.get() != NULL) {
      bypass_mod_rewrite = true;
    }
    factory->ReleaseRewriteDriver(rewrite_driver);
  }

  if (bypass_mod_rewrite) {
    if (need_copy) {
      apr_table_set(request->notes, kResourceUrlNote, url);
    } else {
      apr_table_setn(request->notes, kResourceUrlNote, url);
    }
  }
  return DECLINED;
}

}  // namespace net_instaweb
