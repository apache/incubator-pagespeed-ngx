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

#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apache_request_context.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/apache_server_context.h"
#include "net/instaweb/apache/apache_slurp.h"
#include "net/instaweb/apache/apache_writer.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/in_place_resource_recorder.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"

#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "net/instaweb/apache/apache_logging_includes.h"

namespace net_instaweb {

namespace {

const char kAdminHandler[] = "pagespeed_admin";
const char kGlobalAdminHandler[] = "pagespeed_global_admin";
const char kStatisticsHandler[] = "mod_pagespeed_statistics";
const char kTempStatisticsGraphsHandler[] =
    "mod_pagespeed_temp_statistics_graphs";
const char kConsoleHandler[] = "pagespeed_console";
const char kGlobalStatisticsHandler[] = "mod_pagespeed_global_statistics";
const char kMessageHandler[] = "mod_pagespeed_message";
const char kLogRequestHeadersHandler[] = "mod_pagespeed_log_request_headers";
const char kGenerateResponseWithOptionsHandler[] =
    "mod_pagespeed_response_options_handler";
const char kResourceUrlNote[] = "mod_pagespeed_resource";
const char kResourceUrlNo[] = "<NO>";
const char kResourceUrlYes[] = "<YES>";

// Set the maximum size we allow for processing a POST body. The limit of 128k
// is based on a best guess for the maximum size of beacons required for
// critical CSS.
// TODO(jud): Factor this out, potentially into an option, and pass the value to
// any filters using beacons with POST requests (CriticalImagesBeaconFilter for
// instance).
const size_t kMaxPostSizeBytes = 131072;

}  // namespace

ApacheFetch::ApacheFetch(const GoogleString& mapped_url,
                         ServerContext* server_context,
                         request_rec* request,
                         const RequestContextPtr& request_context,
                         const RewriteOptions* options)
    : AsyncFetchUsingWriter(request_context, &apache_writer_),
      mapped_url_(mapped_url),
      apache_writer_(request),
      server_context_(server_context),
      mutex_(server_context->thread_system()->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      done_(false),
      handle_error_(true),
      status_ok_(false),
      is_proxy_(false),
      options_(options),
      blocking_fetch_timeout_ms_(options_->blocking_fetch_timeout_ms()) {
  // We are proxying content, and the caching in the http configuration
  // should not apply; we want to use the caching from the proxy.
  apache_writer_.set_disable_downstream_header_filters(true);
  apache_writer_.set_strip_cookies(true);
  ApacheRequestToRequestHeaders(*request, request_headers());
  request_headers()->RemoveAll(HttpAttributes::kCookie);
  request_headers()->RemoveAll(HttpAttributes::kCookie2);
}

ApacheFetch::~ApacheFetch() {
}

void ApacheFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  status_ok_ = (status_code != 0) && (status_code < 400);

  if (handle_error_ || status_ok_) {
    // 304 and 204 responses aren't expected to have Content-Types.
    // All other responses should.
    if (status_code != HttpStatus::kNotModified &&
        status_code != HttpStatus::kNoContent) {
      DCHECK(response_headers()->Has(HttpAttributes::kContentType));
    }

    int64 now_ms = server_context_->timer()->NowMs();
    response_headers()->SetDate(now_ms);
    response_headers()->SetLastModified(now_ms);

    // http://msdn.microsoft.com/en-us/library/ie/gg622941(v=vs.85).aspx
    // Script and styleSheet elements will reject responses with
    // incorrect MIME types if the server sends the response header
    // "X-Content-Type-Options: nosniff". This is a security feature
    // that helps prevent attacks based on MIME-type confusion.
    if (!is_proxy_) {
      response_headers()->Add("X-Content-Type-Options", "nosniff");
    }

    // TODO(sligocki): Add X-Mod-Pagespeed header.
    if (content_length_known()) {
      apache_writer_.set_content_length(content_length());
    }

    // Default cache-control to nocache.
    if (!response_headers()->Has(HttpAttributes::kCacheControl)) {
      response_headers()->Add(HttpAttributes::kCacheControl,
                              HttpAttributes::kNoCacheMaxAge0);
    }
    response_headers()->ComputeCaching();

    apache_writer_.OutputHeaders(response_headers());
  }
}

void ApacheFetch::HandleDone(bool success) {
  ScopedMutex lock(mutex_.get());
  done_ = true;
  if (status_ok_ && !success) {
    server_context_->message_handler()->Message(
        kWarning,
        "Response for url %s issued with status %d %s but "
        "failed to complete.",
        mapped_url_.c_str(), response_headers()->status_code(),
        response_headers()->reason_phrase());
  }
  condvar_->Signal();
}

void ApacheFetch::Wait() {
  MessageHandler* handler = server_context_->message_handler();
  Timer* timer = server_context_->timer();
  int64 start_ms = timer->NowMs();
  {
    ScopedMutex lock(mutex_.get());
    while (!done_) {
      condvar_->TimedWait(blocking_fetch_timeout_ms_);
      if (!done_) {
        int64 elapsed_ms = timer->NowMs() - start_ms;
        handler->Message(
            kWarning, "Waiting for completion of URL %s for %g sec",
            mapped_url_.c_str(), elapsed_ms / 1000.0);
      }
    }
  }
}

bool ApacheFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      mapped_url_, *options_, request_context(), headers);
}

InstawebHandler::InstawebHandler(request_rec* request)
    : request_(request),
      server_context_(InstawebContext::ServerContextFromServerRec(
          request->server)),
      rewrite_driver_(NULL),
      num_response_attributes_(0) {
  apache_request_context_ = server_context_->NewApacheRequestContext(request);
  request_context_.reset(apache_request_context_);
  options_ = server_context_->global_config();
  if (apache_request_context_->using_spdy() &&
      (server_context_->SpdyGlobalConfig() != NULL)) {
    options_ = server_context_->SpdyGlobalConfig();
  }
  request_headers_.reset(new RequestHeaders);
  ApacheRequestToRequestHeaders(*request, request_headers_.get());
  original_url_ = InstawebContext::MakeRequestUrl(*options_, request);
  apache_request_context_->set_url(original_url_);
  ComputeCustomOptions();
}

InstawebHandler::~InstawebHandler() {
  WaitForFetch();
}

void InstawebHandler::WaitForFetch() {
  if (fetch_.get() != NULL) {
    fetch_->Wait();
  }
}

void InstawebHandler::SetupSpdyConnectionIfNeeded() {
  apache_request_context_->SetupSpdyConnectionIfNeeded(request_);
}

// Makes a driver from the request_context and options.  Note that
// this can only be called once, as it potentially mutates the options
// as it transfers ownership of custom_options.
RewriteDriver* InstawebHandler::MakeDriver() {
  DCHECK(rewrite_driver_ == NULL)
      << "We can only call MakeDriver once per InstawebHandler:"
      << original_url_;

  rewrite_driver_ = ResourceFetch::GetDriver(
      stripped_gurl_, custom_options_.release(), server_context_,
      request_context_);

  // If there were custom options, the ownership of the memory has
  // now been transferred to the driver, but options_ still points
  // to the same object, so it can still be used as long as the
  // driver is alive.  However, for Karma, and in case some other
  // option-merging is added to the driver someday, let's use the
  // pointer from the driver now.
  options_ = SystemRewriteOptions::DynamicCast(rewrite_driver_->options());
  return rewrite_driver_;
}

ApacheFetch* InstawebHandler::MakeFetch(const GoogleString& url) {
  DCHECK(fetch_.get() == NULL);
  fetch_.reset(new ApacheFetch(url, server_context_, request_, request_context_,
                               options_));
  return fetch_.get();
}

/* static */
bool InstawebHandler::IsCompressibleContentType(const char* content_type) {
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

/* static */
void InstawebHandler::send_out_headers_and_body(
    request_rec* request,
    const ResponseHeaders& response_headers,
    const GoogleString& output) {
  // We always disable downstream header filters when sending out
  // pagespeed resources, since we've captured them in the origin fetch.
  ResponseHeadersToApacheRequest(response_headers, request);
  request->status = response_headers.status_code();
  DisableDownstreamHeaderFilters(request);
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
// options and query-param/request-header options. Stores computed options
// in custom_options_ if needed.  Sets options_ to point to the correct
// options to use.
void InstawebHandler::ComputeCustomOptions() {
  // Set directory specific options.  These will be the options for the
  // directory the resource is in, which under some configurations will be
  // different from the options for the directory that the referencing html is
  // in.  This can lead to us using different options here when regenerating
  // the resource than would be used if the resource were generated as part of
  // a rewrite kicked off by a request for the referencing html file.  This is
  // hard to fix, so instead we're documenting that you must make sure the
  // configuration for your resources matches the configuration for your html
  // files.
  ApacheConfig* directory_options = static_cast<ApacheConfig*>
      ap_get_module_config(request_->per_dir_config, &pagespeed_module);
  if ((directory_options != NULL) && directory_options->modified()) {
    custom_options_.reset(
        server_context_->apache_factory()->NewRewriteOptions());
    custom_options_->Merge(*options_);
    directory_options->Freeze();
    custom_options_->Merge(*directory_options);
  }

  // TODO(sligocki): Move inside PSOL.
  // Merge in query-param or header-based options.
  // Note: We do not generally get response headers in the resource flow,
  // so NULL is passed in instead.
  stripped_gurl_.Reset(original_url_);

  // Copy headers_out and err_headers_out into response_headers.
  // Note that err_headers_out will come after the headers_out in the list of
  // headers. Because of this, err_headers_out will effectively override
  // headers_out when we call GetQueryOptions as it applies the header options
  // in order.
  ApacheRequestToResponseHeaders(*request_, &response_headers_,
                                 &response_headers_);
  num_response_attributes_ = response_headers_.NumAttributes();

  if (!server_context_->GetQueryOptions(&stripped_gurl_, request_headers_.get(),
                                        &response_headers_, &rewrite_query_)) {
    server_context_->message_handler()->Message(
        kWarning, "Invalid PageSpeed query params or headers for "
        "request %s. Serving with default options.",
        stripped_gurl_.spec_c_str());
  }
  const RewriteOptions* query_options = rewrite_query_.options();
  if (query_options != NULL) {
    if (custom_options_.get() == NULL) {
      custom_options_.reset(
          server_context_->apache_factory()->NewRewriteOptions());
      custom_options_->Merge(*options_);
    }
    custom_options_->Merge(*query_options);
    // Don't run any experiments if we're handling a customized request, unless
    // EnrollExperiment is on.
    if (!custom_options_->enroll_experiment()) {
      custom_options_->set_running_experiment(false);
    }
  }
  if (custom_options_.get() != NULL) {
    options_ = custom_options_.get();
  }
}

void InstawebHandler::RemoveStrippedResponseHeadersFromApacheReequest() {
  // Write back the modified response headers if any have been stripped by
  // GetQueryOptions (which indicates that options were found).
  // Note: GetQueryOptions should not add or mutate headers, only remove
  // them.
  DCHECK(response_headers_.NumAttributes() <= num_response_attributes_);
  if (response_headers_.NumAttributes() < num_response_attributes_) {
    // Something was stripped, but we don't know if it came from
    // headers_out or err_headers_out.  We need to treat them separately.
    if (apr_is_empty_table(request_->err_headers_out)) {
      // We know that response_headers were all from request->headers_out
      apr_table_clear(request_->headers_out);
      ResponseHeadersToApacheRequest(response_headers_, request_);
    } else if (apr_is_empty_table(request_->headers_out)) {
      // We know that response_headers were all from err_headers_out
      apr_table_clear(request_->err_headers_out);
      ErrorHeadersToApacheRequest(response_headers_, request_);
    } else {
      // We don't know which table changed, so scan them individually and
      // write them both back. This should be a rare case and could be
      // optimized a bit if we find that we're spending time here.
      ResponseHeaders tmp_err_resp_headers, tmp_resp_headers;
      ThreadSystem* thread_system = server_context_->thread_system();
      ApacheConfig unused_opts1("unused_options1", thread_system),
          unused_opts2("unused_options2", thread_system);

      ApacheRequestToResponseHeaders(*request_, &tmp_resp_headers,
                                     &tmp_err_resp_headers);

      // Use ScanHeader's parsing logic to find and strip the PageSpeed
      // options from the headers. Use NULL for device_properties as no
      // device property information is needed for the stripping.
      RewriteQuery::ScanHeader(
          &tmp_err_resp_headers, NULL /* device_properties */,
          &unused_opts1, server_context_->message_handler());
      RewriteQuery::ScanHeader(
          &tmp_resp_headers, NULL  /* device_properties */, &unused_opts2,
          server_context_->message_handler());

      // Write the stripped headers back to the Apache record.
      apr_table_clear(request_->err_headers_out);
      apr_table_clear(request_->headers_out);
      ResponseHeadersToApacheRequest(tmp_resp_headers, request_);
      ErrorHeadersToApacheRequest(tmp_err_resp_headers, request_);
      // Note that the ordering here matches the comment above the
      // call to ApacheRequestToResponseHeaders in
      // ComputeCustomOptions.
    }
  }
}

// Handle url as .pagespeed. rewritten resource.
void InstawebHandler::HandleAsPagespeedResource() {
  RewriteDriver* driver = MakeDriver();
  GoogleString output;  // TODO(jmarantz): Quit buffering resource output.
  StringWriter writer(&output);

  SyncFetcherAdapterCallback* callback = new SyncFetcherAdapterCallback(
      server_context_->thread_system(), &writer, request_context_);
  callback->SetRequestHeadersTakingOwnership(request_headers_.release());

  if (ResourceFetch::BlockingFetch(stripped_gurl_, server_context_, driver,
                                   callback)) {
    ResponseHeaders* response_headers = callback->response_headers();
    // TODO(sligocki): Check that this is already done in ResourceFetch
    // and remove redundant setting here.
    response_headers->SetDate(server_context_->timer()->NowMs());
    // ResourceFetch adds X-Page-Speed header, old mod_pagespeed code
    // did not. For now, we remove that header for consistency.
    // TODO(sligocki): Consistently use X- headers in MPS and PSOL.
    // I think it would be good to change X-Mod-Pagespeed -> X-Page-Speed
    // and use that for all HTML and resource requests.
    response_headers->RemoveAll(kPageSpeedHeader);
    send_out_headers_and_body(request_, *response_headers, output);
  } else {
    server_context_->ReportResourceNotFound(original_url_, request_);
  }

  callback->Release();
}

static apr_status_t DeleteInPlaceRecorder(void* object) {
  InPlaceResourceRecorder* recorded =
      static_cast<InPlaceResourceRecorder*>(object);
  delete recorded;
  return APR_SUCCESS;
}

// Handle url with In Place Resource Optimization (IPRO) flow.
bool InstawebHandler::HandleAsInPlace() {
  bool handled = false;

  // We need to see if the origin request has cookies, so examine the
  // Apache request directly, as request_headers_ has been stripped of
  // headers we don't want to transmit for resource fetches.
  //
  // Note that apr_table_get is case insensitive. See
  // http://apr.apache.org/docs/apr/2.0/group__apr__tables.html#ga4db13e3915c6b9a3142b175d4c15d915
  RequestHeaders::Properties request_properties(
      apr_table_get(request_->headers_in, HttpAttributes::kCookie) != NULL,
      apr_table_get(request_->headers_in, HttpAttributes::kCookie2) != NULL,
      (apr_table_get(request_->headers_in, HttpAttributes::kAuthorization)
       != NULL) || (request_->user != NULL));

  RewriteDriver* driver = MakeDriver();
  MakeFetch(original_url_);
  fetch_->set_handle_error(false);
  driver->FetchInPlaceResource(stripped_gurl_, false /* proxy_mode */,
                               fetch_.get());
  WaitForFetch();
  if (fetch_->status_ok()) {
    server_context_->rewrite_stats()->ipro_served()->Add(1);
    handled = true;
  } else if ((fetch_->response_headers()->status_code() ==
              CacheUrlAsyncFetcher::kNotInCacheStatus) &&
             !request_->header_only) {
    server_context_->rewrite_stats()->ipro_not_in_cache()->Add(1);
    // This URL was not found in cache (neither the input resource nor
    // a ResourceNotCacheable entry) so we need to get it into cache
    // (or at least a note that it cannot be cached stored there).
    // We do that using an Apache output filter.
    //
    // We use stripped_gurl_.Spec() rather than 'original_url_' for
    // InPlaceResourceRecorder as we want any ?ModPagespeed query-params to
    // be stripped from the cache key before we store the result in HTTPCache.
    InPlaceResourceRecorder* recorder = new InPlaceResourceRecorder(
        request_context_,
        stripped_gurl_.Spec(),
        driver->CacheFragment(),
        request_properties,
        options_->respect_vary(),
        options_->ipro_max_response_bytes(),
        options_->ipro_max_concurrent_recordings(),
        options_->implicit_cache_ttl_ms(),
        server_context_->http_cache(),
        server_context_->statistics(),
        server_context_->message_handler());
    ap_add_output_filter(kModPagespeedInPlaceFilterName, recorder,
                         request_, request_->connection);
    ap_add_output_filter(kModPagespeedInPlaceCheckHeadersName, recorder,
                         request_, request_->connection);
    // Add a contingency cleanup path in case some module munches
    // (or doesn't produce at all) an EOS bucket. If everything
    // goes well, we will just remove it befoe cleaning up ourselves.
    apr_pool_cleanup_register(
        request_->pool, recorder, DeleteInPlaceRecorder, apr_pool_cleanup_null);
  } else {
    server_context_->rewrite_stats()->ipro_not_rewritable()->Add(1);
  }
  driver->Cleanup();

  return handled;
}

bool InstawebHandler::HandleAsProxy() {
  bool handled = false;
  // Consider Issue 609: proxying an external CSS file via MapProxyDomain, and
  // the CSS file makes reference to a font file, which mod_pagespeed does not
  // know anything about, and does not know how to absolutify.  We need to
  // handle the request for the external font file here, even if IPRO (in place
  // resource optimization) is off.
  bool is_proxy = false;
  GoogleString mapped_url;
  GoogleString host_header;
  if (options_->domain_lawyer()->MapOriginUrl(stripped_gurl_, &mapped_url,
                                              &host_header, &is_proxy) &&
      is_proxy) {
    RewriteDriver* driver = MakeDriver();
    MakeFetch(mapped_url);
    fetch_->set_is_proxy(true);
    driver->SetRequestHeaders(*fetch_->request_headers());
    server_context_->proxy_fetch_factory()->StartNewProxyFetch(
        mapped_url, fetch_.get(), driver, NULL, NULL);
    WaitForFetch();
    handled = true;
  }

  return handled;
}

// Determines whether the url can be handled as a mod_pagespeed or in-place
// optimized resource, and handles it, returning true.  Success status is
// written to the status code in the response headers.
/* static */
bool InstawebHandler::handle_as_resource(ApacheServerContext* server_context,
                                         request_rec* request,
                                         GoogleUrl* gurl) {
  if (!gurl->IsWebValid()) {
    return false;
  }

  InstawebHandler instaweb_handler(request);
  instaweb_handler.SetupSpdyConnectionIfNeeded();
  scoped_ptr<RequestHeaders> request_headers(new RequestHeaders);
  const RewriteOptions* options = instaweb_handler.options();

  // Finally, do the actual handling.
  bool handled = false;
  if (server_context->IsPagespeedResource(*gurl)) {
    handled = true;
    instaweb_handler.HandleAsPagespeedResource();
  } else if (instaweb_handler.HandleAsProxy()) {
    handled = true;
  } else if (options->in_place_rewriting_enabled() && options->enabled() &&
             options->IsAllowed(gurl->Spec())) {
    handled = instaweb_handler.HandleAsInPlace();
  }

  return handled;
}

// Write response headers and send out headers and output, including the option
// for a custom Content-Type.
//
// TODO(jmarantz): consider deleting this helper method putting all responses
// through ApacheFetch.
/* static */
void InstawebHandler::write_handler_response(const StringPiece& output,
                                             request_rec* request,
                                             ContentType content_type,
                                             const StringPiece& cache_control) {
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.set_major_version(1);
  response_headers.set_minor_version(1);

  response_headers.Add(HttpAttributes::kContentType, content_type.mime_type());
  // http://msdn.microsoft.com/en-us/library/ie/gg622941(v=vs.85).aspx
  // Script and styleSheet elements will reject responses with
  // incorrect MIME types if the server sends the response header
  // "X-Content-Type-Options: nosniff". This is a security feature
  // that helps prevent attacks based on MIME-type confusion.
  response_headers.Add(HttpAttributes::kXContentTypeOptions,
                       HttpAttributes::kNosniff);
  AprTimer timer;
  int64 now_ms = timer.NowMs();
  response_headers.SetDate(now_ms);
  response_headers.SetLastModified(now_ms);
  response_headers.Add(HttpAttributes::kCacheControl, cache_control);
  send_out_headers_and_body(request, response_headers, output.as_string());
}

/* static */
void InstawebHandler::write_handler_response(const StringPiece& output,
                                             request_rec* request) {
  write_handler_response(output, request,
                         kContentTypeHtml, HttpAttributes::kNoCacheMaxAge0);
}

// Returns request URL if it was a .pagespeed. rewritten resource URL.
// Otherwise returns NULL. Since other Apache modules can change request->uri,
// we stow the original request URL in a note. This method reads that note
// and thus should return the URL that the browser actually requested (rather
// than a mod_rewrite altered URL).
/* static */
const char* InstawebHandler::get_instaweb_resource_url(
    request_rec* request, ApacheServerContext* server_context) {
  const char* resource = apr_table_get(request->notes, kResourceUrlNote);

  // If our translate_name hook, save_url_hook, failed to run because some
  // other module's translate_hook returned OK first, then run it now. The
  // main reason we try to do this early is to save our URL before mod_rewrite
  // mutates it.
  if (resource == NULL) {
    InstawebHandler::save_url_in_note(request, server_context);
    resource = apr_table_get(request->notes, kResourceUrlNote);
  }

  if (resource != NULL && strcmp(resource, kResourceUrlNo) == 0) {
    return NULL;
  }

  const char* url = apr_table_get(request->notes, kPagespeedOriginalUrl);
  return url;
}

namespace {

// Used by log_request_headers for testing only.
struct HeaderLoggingData {
  HeaderLoggingData(StringWriter* writer_in, MessageHandler* handler_in)
      : writer(writer_in), handler(handler_in) {}
  StringWriter* writer;
  MessageHandler* handler;
};

}  // namespace

// Helper function to support the LogRequestHeadersHandler.  Called once for
// each header to write header data in a form suitable for javascript inlining.
// Used only for tests.
/* static */
int InstawebHandler::log_request_headers(void* logging_data,
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

/* static */
void InstawebHandler::instaweb_static_handler(
    request_rec* request, ApacheServerContext* server_context) {
  StaticAssetManager* static_asset_manager =
      server_context->static_asset_manager();
  StringPiece request_uri_path = request->parsed_uri.path;
  // Strip out the common prefix url before sending to StaticAssetManager.
  StringPiece file_name = request_uri_path.substr(
      server_context->apache_factory()->static_asset_prefix().length());
  StringPiece file_contents;
  StringPiece cache_header;
  ContentType content_type;
  if (static_asset_manager->GetAsset(
      file_name, &file_contents, &content_type, &cache_header)) {
    write_handler_response(file_contents, request, content_type, cache_header);
  } else {
    server_context->ReportResourceNotFound(request->parsed_uri.path, request);
  }
}

// Append the query params from a request into data. This just parses the query
// params from a request URL. For parsing the query params from a POST body, use
// parse_body_from_post(). Return true if successful, otherwise, returns false
// and sets ret to the appropriate status.
/* static */
bool InstawebHandler::parse_query_params(const request_rec* request,
                                         GoogleString* data,
                                         apr_status_t* ret) {
  // Add a dummy host (www.example.com) to the request URL to make it absolute
  // so that GoogleUrl can be used for parsing.
  GoogleUrl base("http://www.example.com");
  GoogleUrl url(base, request->unparsed_uri);

  if (!url.IsWebValid() || !url.has_query()) {
    *ret = HTTP_BAD_REQUEST;
    return false;
  }

  url.Query().AppendToString(data);
  return true;
}

// Read the body from a POST request and append to data. Return true if
// successful, otherwise, returns false and sets ret to the appropriate status.
bool InstawebHandler::parse_body_from_post(const request_rec* request,
                                           GoogleString* data,
                                           apr_status_t* ret) {
  if (request->method_number != M_POST) {
    *ret = HTTP_METHOD_NOT_ALLOWED;
    return false;
  }

  // Verify that the request has the correct content type for a form POST
  // submission. Ideally, we could use request->content_type here, but that is
  // coming back as NULL, even when the header was set correctly.
  const char* content_type = apr_table_get(request->headers_in,
                                           HttpAttributes::kContentType);
  if (content_type == NULL) {
    *ret = HTTP_BAD_REQUEST;
    return false;
  }
  GoogleString mime_type;
  GoogleString charset;
  if (!ParseContentType(content_type, &mime_type, &charset)) {
    *ret = HTTP_BAD_REQUEST;
    return false;
  }
  if (!StringCaseEqual(mime_type, "application/x-www-form-urlencoded")) {
    *ret = HTTP_BAD_REQUEST;
    return false;
  }

  // Setup the number of bytes to try to read from the POST body. If the
  // Content-Length header is set, use it, otherwise try to pull up to
  // kMaxPostSizeBytes.
  int content_len = kMaxPostSizeBytes;
  const char* content_len_str = apr_table_get(request->headers_in,
                                              HttpAttributes::kContentLength);
  if (content_len_str != NULL) {
    if (!StringToInt(content_len_str, &content_len)) {
      *ret = HTTP_BAD_REQUEST;
      return false;
    }
    if (static_cast<size_t>(content_len) > kMaxPostSizeBytes) {
      *ret = HTTP_REQUEST_ENTITY_TOO_LARGE;
      return false;
    }
  }

  // Parse the incoming brigade and add the contents to data. In apache 2.4 we
  // could just use ap_parse_form_data. See the example at
  // http://httpd.apache.org/docs/2.4/developer/modguide.html#snippets.
  apr_bucket_brigade* bbin =
      apr_brigade_create(request->pool, request->connection->bucket_alloc);

  bool eos = false;

  while (!eos) {
    apr_status_t rv = ap_get_brigade(request->input_filters, bbin,
                                     AP_MODE_READBYTES, APR_BLOCK_READ,
                                     content_len);
    if (rv != APR_SUCCESS) {
      // Form input read failed.
      *ret = HTTP_INTERNAL_SERVER_ERROR;
      return false;
    }
    for (apr_bucket* bucket = APR_BRIGADE_FIRST(bbin);
         bucket != APR_BRIGADE_SENTINEL(bbin);
         bucket = APR_BUCKET_NEXT(bucket) ) {
      if (!APR_BUCKET_IS_METADATA(bucket)) {
        const char* buf = NULL;
        size_t bytes = 0;
        rv = apr_bucket_read(bucket, &buf, &bytes, APR_BLOCK_READ);
        if (rv != APR_SUCCESS) {
          *ret = HTTP_INTERNAL_SERVER_ERROR;
          return false;
        }
        if (data->length() + bytes > kMaxPostSizeBytes) {
          *ret = HTTP_REQUEST_ENTITY_TOO_LARGE;
          return false;
        }
        data->append(buf, bytes);
      } else if (APR_BUCKET_IS_EOS(bucket)) {
        eos = true;
        break;
      }
    }
    apr_brigade_cleanup(bbin);
  }

  // No need to modify ret as it is only used if reading the POST failed.
  return true;
}

/* static */
apr_status_t InstawebHandler::instaweb_beacon_handler(
    request_rec* request, ApacheServerContext* server_context) {
  GoogleString data;
  apr_status_t ret = DECLINED;
  if (request->method_number == M_GET) {
    if (!parse_query_params(request, &data, &ret)) {
      return ret;
    }
  } else if (request->method_number == M_POST) {
    GoogleString query_param_data, post_data;
    // Even if the beacon is a POST, the originating url should be in the query
    // params, not the POST body.
    if (!parse_query_params(request, &query_param_data, &ret)) {
      return ret;
    }
    if (!parse_body_from_post(request, &post_data, &ret)) {
      return ret;
    }
    StrAppend(&data, query_param_data, "&", post_data);
  } else {
    return HTTP_METHOD_NOT_ALLOWED;
  }
  RequestContextPtr request_context(
      server_context->NewApacheRequestContext(request));
  StringPiece user_agent = apr_table_get(request->headers_in,
                                         HttpAttributes::kUserAgent);
  server_context->HandleBeacon(data, user_agent, request_context);
  apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                HttpAttributes::kNoCacheMaxAge0);
  return HTTP_NO_CONTENT;
}

/* static */
bool InstawebHandler::IsBeaconUrl(const RewriteOptions::BeaconUrl& beacons,
                                  const GoogleUrl& gurl) {
  // Check if the full path without query parameters equals the beacon URL,
  // either the http or https version (we're too lazy to check specifically).
  // This handles both GETs, which include query parameters, and POSTs,
  // which will only have the originating url in the query params.
  if (!gurl.IsWebValid()) {
    return false;
  }
  // Ignore query params in the beacon URLs. Normally the beacon URL won't have
  // a query param, but it could have been added using ModPagespeedBeaconUrl.
  return (gurl.PathSansQuery() == beacons.http_in ||
          gurl.PathSansQuery() == beacons.https_in);
}

/* static */
bool InstawebHandler::is_pagespeed_subrequest(request_rec* request) {
  StringPiece user_agent = apr_table_get(request->headers_in,
                                         HttpAttributes::kUserAgent);
  return (user_agent.find(kModPagespeedSubrequestUserAgent) != user_agent.npos);
}

/* static */
apr_status_t InstawebHandler::instaweb_handler(request_rec* request) {
  apr_status_t ret = DECLINED;
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  ApacheConfig* global_config = server_context->global_config();
  // Escape ASAP if we're in unplugged mode.
  if (global_config->unplugged()) {
    return DECLINED;
  }

  // Flushing the cache mutates global_options, so this has to happen before we
  // construct the options that we use to decide whether IPRO is enabled.  Note
  // that the global_config might be altered by this, but the pointer will not
  // change.
  server_context->FlushCacheIfNecessary();

  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  ApacheMessageHandler* message_handler = factory->apache_message_handler();
  StringPiece request_handler_str = request->handler;

  bool is_global_statistics = (request_handler_str == kGlobalStatisticsHandler);
  if (request_handler_str == kStatisticsHandler || is_global_statistics) {
    InstawebHandler instaweb_handler(request);
    server_context->StatisticsPage(is_global_statistics,
                                   instaweb_handler.query_params(),
                                   instaweb_handler.options(),
                                   instaweb_handler.MakeFetch());
    return OK;
  } else if ((request_handler_str == kAdminHandler) ||
             (request_handler_str == kGlobalAdminHandler)) {
    InstawebHandler instaweb_handler(request);
    server_context->AdminPage((request_handler_str == kGlobalAdminHandler),
                              instaweb_handler.stripped_gurl(),
                              instaweb_handler.query_params(),
                              instaweb_handler.options(),
                              instaweb_handler.MakeFetch());
    ret = OK;
  } else if (request_handler_str == kTempStatisticsGraphsHandler) {
    // TODO(sligocki): Merge this into kConsoleHandler.
    GoogleString output;
    StringWriter writer(&output);
    server_context->StatisticsGraphsHandler(&writer);
    write_handler_response(output, request);
    ret = OK;
  } else if (request_handler_str == kConsoleHandler) {
    InstawebHandler instaweb_handler(request);
    server_context->ConsoleHandler(*instaweb_handler.options(),
                                   SystemServerContext::kOther,
                                   instaweb_handler.query_params(),
                                   instaweb_handler.MakeFetch());
    ret = OK;
  } else if (request_handler_str == kMessageHandler) {
    InstawebHandler instaweb_handler(request);
    server_context->MessageHistoryHandler(SystemServerContext::kOther,
                                          instaweb_handler.MakeFetch());
    ret = OK;
  } else if (request_handler_str == kLogRequestHeadersHandler) {
    // For testing CustomFetchHeader.
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
      apr_table_add(request->headers_out, "PageSpeed", "off");
    } else if (strstr(request->parsed_uri.query, "headers_errout") != NULL) {
      apr_table_add(request->err_headers_out, "PageSpeed", "off");
    } else if (strstr(request->parsed_uri.query, "headers_override") != NULL) {
      apr_table_add(request->headers_out, "PageSpeed", "off");
      apr_table_add(request->headers_out, "PageSpeedFilters",
                    "-remove_comments");
      apr_table_add(request->err_headers_out, "PageSpeed", "on");
      apr_table_add(request->err_headers_out, "PageSpeedFilters",
                    "+remove_comments");
    } else if (strstr(request->parsed_uri.query, "headers_combine") != NULL) {
      apr_table_add(request->headers_out, "PageSpeed", "on");
      apr_table_add(request->err_headers_out, "PageSpeedFilters",
                    "+remove_comments");
    }
  } else {
    const char* url = InstawebContext::MakeRequestUrl(*global_config, request);
    // Do not try to rewrite our own sub-request.
    if (url != NULL) {
      GoogleUrl gurl(url);
      if (!gurl.IsWebValid()) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                      "Ignoring invalid URL: %s", gurl.spec_c_str());
      } else if (IsBeaconUrl(global_config->beacon_url(), gurl)) {
        ret = instaweb_beacon_handler(request, server_context);
      // For the beacon accept any method; for all others only allow GETs.
      } else if (request->method_number != M_GET) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                      "Not rewriting non-GET %d of %s",
                      request->method_number, gurl.spec_c_str());
      } else if (gurl.PathSansLeaf() ==
                 server_context->apache_factory()->static_asset_prefix()) {
        instaweb_static_handler(request, server_context);
        ret = OK;
      } else if (!is_pagespeed_subrequest(request) &&
                 handle_as_resource(server_context, request, &gurl)) {
        ret = OK;
      }
    }

    // Check for HTTP_NO_CONTENT here since that's the status used for a
    // successfully handled beacon.
    if (ret != OK && ret != HTTP_NO_CONTENT &&
        (global_config->slurping_enabled() || global_config->test_proxy())) {
      SlurpUrl(server_context, request);
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
/* static */
apr_status_t InstawebHandler::save_url_hook(request_rec *request) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  return save_url_in_note(request, server_context);
}

/* static */
apr_status_t InstawebHandler::save_url_in_note(
    request_rec *request, ApacheServerContext* server_context) {
  // Escape ASAP if we're in unplugged mode.
  if (server_context->global_config()->unplugged()) {
    return DECLINED;
  }

  // This call to MakeRequestUrl() not only returns the url but also
  // saves it for future use so that if another module changes the
  // url in the request, we still have the original one.
  const char* url = InstawebContext::MakeRequestUrl(
      *server_context->global_options(), request);
  GoogleUrl gurl(url);

  bool bypass_mod_rewrite = false;
  if (gurl.IsWebValid()) {
    // Note: We cannot use request->handler because it may not be set yet :(
    // TODO(sligocki): Make this robust to custom statistics and beacon URLs.
    StringPiece leaf = gurl.LeafSansQuery();
    if (leaf == kStatisticsHandler || leaf == kConsoleHandler ||
        leaf == kGlobalStatisticsHandler || leaf == kMessageHandler ||
        leaf == kAdminHandler ||
        gurl.PathSansLeaf() ==
          server_context->apache_factory()->static_asset_prefix() ||
        IsBeaconUrl(server_context->global_options()->beacon_url(), gurl) ||
        server_context->IsPagespeedResource(gurl)) {
      bypass_mod_rewrite = true;
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
/* static */
apr_status_t InstawebHandler::instaweb_map_to_storage(request_rec* request) {
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
  if (server_context->global_config()->unplugged()) {
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

/* static */
void InstawebHandler::AboutToBeDoneWithRecorder(
    request_rec* request, InPlaceResourceRecorder* recorder) {
  apr_pool_cleanup_kill(request->pool, recorder, DeleteInPlaceRecorder);
}

}  // namespace net_instaweb
