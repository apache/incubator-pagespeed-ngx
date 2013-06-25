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
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)
//
// Register handlers, define configuration options and set up other things
// that mod_pagespeed needs to do to be an Apache module.

#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_request_context.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/apache_server_context.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/in_place_resource_recorder.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/instaweb_handler.h"
#include "net/instaweb/apache/interface_mod_spdy.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/apache/mod_spdy_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/serf_url_async_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

#include "util_filter.h"
// Note: a very useful reference is this file, which demos many Apache module
// options:
//    http://svn.apache.org/repos/asf/httpd/httpd/trunk/modules/examples/mod_example_hooks.c

// The httpd header must be after the pagepseed_server_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "ap_release.h"                                              // NOLINT
#include "apr_pools.h"                                               // NOLINT
#include "apr_strings.h"                                             // NOLINT
#include "http_config.h"                                             // NOLINT
#include "http_protocol.h"                                           // NOLINT
#include "http_request.h"                                            // NOLINT
#include "httpd.h"                                                   // NOLINT

// This include-file is order-dependent; it must come after the above apache
// includes, and not be in abc-order with the net/instaweb/... includes.
#include "net/instaweb/apache/apache_logging_includes.h"

#include "net/instaweb/apache/log_message_handler.h"
#include "unixd.h"

#if (AP_SERVER_MAJORVERSION_NUMBER == 2) && (AP_SERVER_MINORVERSION_NUMBER >= 4)
#define MPS_APACHE_24
#endif

// Apache 2.4 renames unixd_config -> ap_unixd_config
#ifdef MPS_APACHE_24
#define unixd_config ap_unixd_config
#endif

struct apr_pool_t;

namespace net_instaweb {

class Statistics;

namespace {

// Passed to CheckGlobalOption
enum VHostHandling {
  kTolerateInVHost,
  kErrorInVHost
};

// TODO(sligocki): Separate options parsing from all the other stuff here.
// Instaweb directive names -- these must match
// install/common/pagespeed.conf.template.
// If you add a new option, please add it to the #ALL_DIRECTIVES section of
// install/debug.conf.template to make sure it will parse.

const char kModPagespeedIf[] = "<ModPagespeedIf";

const char kModPagespeedAllow[] = "ModPagespeedAllow";
const char kModPagespeedBlockingRewriteRefererUrls[] =
    "ModPagespeedBlockingRewriteRefererUrls";
const char kModPagespeedCreateSharedMemoryMetadataCache[] =
    "ModPagespeedCreateSharedMemoryMetadataCache";
const char kModPagespeedCustomFetchHeader[] = "ModPagespeedCustomFetchHeader";
const char kModPagespeedDangerPermitFetchFromUnknownHosts[] =
    "ModPagespeedDangerPermitFetchFromUnknownHosts";
const char kModPagespeedDisableFilters[] = "ModPagespeedDisableFilters";
const char kModPagespeedDisableForBots[] = "ModPagespeedDisableForBots";
const char kModPagespeedDisallow[] = "ModPagespeedDisallow";
const char kModPagespeedDomain[] = "ModPagespeedDomain";
const char kModPagespeedDownstreamCachePurgeLocationPrefix[] =
    "ModPagespeedDownstreamCachePurgeLocationPrefix";
const char kModPagespeedEnableFilters[] = "ModPagespeedEnableFilters";
const char kModPagespeedFetchHttps[] = "ModPagespeedFetchHttps";
const char kModPagespeedFetchProxy[] = "ModPagespeedFetchProxy";
const char kModPagespeedFetcherTimeoutMs[] = "ModPagespeedFetcherTimeOutMs";
const char kModPagespeedFetchWithGzip[] = "ModPagespeedFetchWithGzip";
const char kModPagespeedFileCachePath[] = "ModPagespeedFileCachePath";
const char kModPagespeedForbidFilters[] = "ModPagespeedForbidFilters";
const char kModPagespeedForceCaching[] = "ModPagespeedForceCaching";
const char kModPagespeedExperimentVariable[] = "ModPagespeedExperimentVariable";
const char kModPagespeedExperimentSpec[] = "ModPagespeedExperimentSpec";
const char kModPagespeedGeneratedFilePrefix[] =
    "ModPagespeedGeneratedFilePrefix";
const char kModPagespeedImageInlineMaxBytes[] =
    "ModPagespeedImageInlineMaxBytes";
const char kModPagespeedImageMaxRewritesAtOnce[] =
    "ModPagespeedImageMaxRewritesAtOnce";
const char kModPagespeedInheritVHostConfig[] = "ModPagespeedInheritVHostConfig";
const char kModPagespeedInstallCrashHandler[] =
    "ModPagespeedInstallCrashHandler";
const char kModPagespeedLibrary[] = "ModPagespeedLibrary";
const char kModPagespeedListOutstandingUrlsOnError[] =
    "ModPagespeedListOutstandingUrlsOnError";
const char kModPagespeedLoadFromFile[] = "ModPagespeedLoadFromFile";
const char kModPagespeedLoadFromFileMatch[] = "ModPagespeedLoadFromFileMatch";
const char kModPagespeedLoadFromFileRule[] = "ModPagespeedLoadFromFileRule";
const char kModPagespeedLoadFromFileRuleMatch[] =
    "ModPagespeedLoadFromFileRuleMatch";
const char kModPagespeedLogDir[] = "ModPagespeedLogDir";
const char kModPagespeedMapOriginDomain[] = "ModPagespeedMapOriginDomain";
const char kModPagespeedMapProxyDomain[] = "ModPagespeedMapProxyDomain";
const char kModPagespeedMapRewriteDomain[] = "ModPagespeedMapRewriteDomain";
const char kModPagespeedMessageBufferSize[] = "ModPagespeedMessageBufferSize";
const char kModPagespeedNumExpensiveRewriteThreads[] =
    "ModPagespeedNumExpensiveRewriteThreads";
const char kModPagespeedNumRewriteThreads[] = "ModPagespeedNumRewriteThreads";
const char kModPagespeedNumShards[] = "ModPagespeedNumShards";
const char kModPagespeedRetainComment[] = "ModPagespeedRetainComment";
const char kModPagespeedRunExperiment[] = "ModPagespeedRunExperiment";
const char kModPagespeedShardDomain[] = "ModPagespeedShardDomain";
const char kModPagespeedSpeedTracking[] = "ModPagespeedIncreaseSpeedTracking";
const char kModPagespeedStatisticsLoggingFile[] =
    "ModPagespeedStatisticsLoggingFile";
const char kModPagespeedTrackOriginalContentLength[] =
    "ModPagespeedTrackOriginalContentLength";
const char kModPagespeedUrlPrefix[] = "ModPagespeedUrlPrefix";
const char kModPagespeedUrlValuedAttribute[] = "ModPagespeedUrlValuedAttribute";
const char kModPagespeedUsePerVHostStatistics[] =
    "ModPagespeedUsePerVHostStatistics";

// The following two are deprecated due to spelling
const char kModPagespeedImgInlineMaxBytes[] = "ModPagespeedImgInlineMaxBytes";
const char kModPagespeedImgMaxRewritesAtOnce[] =
    "ModPagespeedImgMaxRewritesAtOnce";

// The following three are deprecated because we didn't finish the feature.
const char kModPagespeedCollectRefererStatistics[] =
    "ModPagespeedCollectRefererStatistics";
const char kModPagespeedHashRefererStatistics[] =
    "ModPagespeedHashRefererStatistics";
const char kModPagespeedRefererStatisticsOutputLevel[] =
    "ModPagespeedRefererStatisticsOutputLevel";

enum RewriteOperation {REWRITE, FLUSH, FINISH};

// TODO(sligocki): Move inside PSOL.
// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(request_rec* request,
                                const ContentType& content_type) {
  // We can't operate on Content-Ranges.
  if (apr_table_get(request->headers_out, "Content-Range") != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: header Content-Range set.");
    return false;
  }

  // Only rewrite HTML-like content.
  if (!content_type.IsHtmlLike()) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: request->content_type does "
                  "not appear to be HTML (was %s)", request->content_type);
    return false;
  }

  // mod_pagespeed often creates requests while rewriting an HTML.  These
  // requests are only intended to fetch resources (images, css, javascript) but
  // in some circumstances they can end up fetching HTML.  This HTML, if
  // rewrittten, could in turn spawn more requests which could cascade into a
  // bad situation.  To mod_pagespeed, any fetched HTML is an error condition,
  // so there's no reason to rewrite it anyway.
  if (is_pagespeed_subrequest(request)) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: User-Agent appears to be "
                  "mod_pagespeed");
    return false;
  }

  return true;
}

// Create a new bucket from buf using HtmlRewriter.
// TODO(lsong): the content is copied multiple times. The buf is
// copied/processed to string output, then output is copied to new bucket.
apr_bucket* rewrite_html(InstawebContext* context, request_rec* request,
                         RewriteOperation operation, const char* buf, int len) {
  if (context == NULL) {
    LOG(DFATAL) << "Context is null";
    return NULL;
  }
  if (buf != NULL) {
    context->PopulateHeaders(request);
    context->Rewrite(buf, len);
  }
  if (operation == REWRITE) {
    return NULL;
  } else if (operation == FLUSH) {
    context->Flush();
    // If the flush happens before any rewriting, don't fallthrough and
    // replace the headers with those in the context, because they haven't
    // been populated yet so we end up with NO headers. See issue 385.
    if (context->output().empty()) {
      return NULL;
    }
  } else if (operation == FINISH) {
    context->Finish();
  }

  // Check to see if we've added in the headers already.  If not,
  // clear out the existing headers (to avoid duplication), add them,
  // and make a note of it.
  if (!context->sent_headers()) {
    ResponseHeaders* headers = context->response_headers();
    apr_table_clear(request->headers_out);
    AddResponseHeadersToRequest(headers, NULL,
                                context->modify_caching_headers(),
                                request);
    headers->Clear();
    context->set_sent_headers(true);
  }

  const GoogleString& output = context->output();
  if (output.empty()) {
    return NULL;
  }

  // Use the rewritten content. Create in heap since output will
  // be emptied for reuse.
  apr_bucket* bucket = apr_bucket_heap_create(
      output.data(), output.size(), NULL,
      request->connection->bucket_alloc);
  context->clear();
  return bucket;
}

// Apache's pool-based cleanup is not effective on process shutdown.  To allow
// valgrind to report clean results, we must take matters into our own hands.
// We employ a statically allocated class object and rely on its destructor to
// get a reliable cleanup hook.  I am, in general, strongly opposed to this
// sort of technique, and it violates the C++ Style Guide:
//   http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml
// However, it is not possible to use valgrind to track memory leaks
// in our Apache module without this approach.
//
// We also need this context hold any data needed for statistics
// collected in advance of the creation of the Statistics object, such
// as directives-parsing time.
class ApacheProcessContext {
 public:
  ApacheProcessContext() : apache_cmds_(NULL) {
    ApacheRewriteDriverFactory::Initialize();
    InstallCommands();
  }

  ~ApacheProcessContext() {
    ApacheRewriteDriverFactory::Terminate();
    delete [] apache_cmds_;
    log_message_handler::ShutDown();
    // We must reset the factory to NULL before ProcessContext's dtor
    // is called, which terminates the protobuf libraries.  It is unsafe
    // to free our structures after the protobuf library has been shut down.
    factory_.reset(NULL);
  }

  void InstallCommands();

  ApacheRewriteDriverFactory* factory(server_rec* server) {
    // We are not mutex-protecting the factory-creation for now as the
    // server_rec initialization loop appears to be single-threaded in
    // Apache.
    if (factory_.get() == NULL) {
      factory_.reset(new ApacheRewriteDriverFactory(
          server, kModPagespeedVersion));
    }
    return factory_.get();
  }

  scoped_ptr<ApacheRewriteDriverFactory> factory_;
  // Process-scoped static variable cleanups, mainly for valgrind.
  ProcessContext process_context_;
  command_rec* apache_cmds_;
  StringVector cmd_names_;
};
ApacheProcessContext apache_process_context;

typedef void (ApacheServerContext::*AddTimeFn)(int64 delta);

class ScopedTimer {
 public:
  ScopedTimer(ApacheServerContext* server_context, AddTimeFn add_time_fn)
      : server_context_(server_context),
        add_time_fn_(add_time_fn),
        start_time_us_(timer_.NowUs()) {
  }

  ~ScopedTimer() {
    int64 delta_us = timer_.NowUs() - start_time_us_;
    (server_context_->*add_time_fn_)(delta_us);
  }

 private:
  ApacheServerContext* server_context_;
  AddTimeFn add_time_fn_;
  AprTimer timer_;
  int64 start_time_us_;
};

// Builds a new context for an HTML request, returning NULL if we decide
// that we should not handle the request for various reasons.
// TODO(sligocki): Move most of these checks into non-Apache specific code.
InstawebContext* build_context_for_request(request_rec* request) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  // Escape ASAP if we're in unplugged mode.
  if (server_context->config()->unplugged()) {
    return NULL;
  }
  ApacheConfig* directory_options = static_cast<ApacheConfig*>
      ap_get_module_config(request->per_dir_config, &pagespeed_module);
  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  scoped_ptr<RewriteOptions> custom_options;

  ApacheRequestContext* apache_request = new ApacheRequestContext(
      server_context->thread_system()->NewMutex(),
      server_context->timer(),
      request);
  RequestContextPtr request_context(apache_request);
  bool using_spdy = request_context->using_spdy();
  const RewriteOptions* host_options = server_context->global_options();
  if (using_spdy && server_context->SpdyConfig() != NULL) {
    host_options = server_context->SpdyConfig();
  }
  const RewriteOptions* options = host_options;
  bool use_custom_options = false;

  server_context->FlushCacheIfNecessary();

  if ((directory_options != NULL) && directory_options->modified()) {
    custom_options.reset(factory->NewRewriteOptions());
    custom_options->Merge(*host_options);
    custom_options->Merge(*directory_options);
    server_context->ComputeSignature(custom_options.get());
    options = custom_options.get();
    use_custom_options = true;
  }

  if (request->unparsed_uri == NULL) {
    // TODO(jmarantz): consider adding Debug message if unparsed_uri is NULL,
    // possibly of request->the_request which was non-null in the case where
    // I found this in the debugger.
    ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, request,
                  "Request not rewritten because: "
                  "request->unparsed_uri == NULL");
    return NULL;
  }

  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                "ModPagespeed OutputFilter called for request %s",
                request->unparsed_uri);

  // Requests with a non-NULL main pointer are internal requests created by
  // apache (or other modules in apache).  We don't need to process them.
  // E.g. An included header file will be processed as a separate request.
  // mod_pagespeed needs to process only the "completed" page with the header
  // inlined, not the separate header request.
  // See http://httpd.apache.org/dev/apidoc/apidoc_request_rec.html for
  // request documentation.
  if (request->main != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: request->main != NULL");
    return NULL;
  }

  // TODO(sligocki): Should we rewrite any other statuses?
  // Maybe 206 Partial Content?
  // TODO(sligocki): Make this decision inside PSOL.
  if (request->status != 200) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: request->status != 200 "
                  "(was %d)", request->status);
    return NULL;
  }

  const ContentType* content_type =
      MimeTypeToContentType(request->content_type);
  // TODO(sligocki): Move inside PSOL.
  if (content_type == NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: request->content_type was "
                  "not a recognized type (was %s)", request->content_type);
    return NULL;
  }

  // Check if pagespeed optimization is applicable.
  // TODO(sligocki): Put other checks in this function.
  if (!check_pagespeed_applicable(request, *content_type)) {
    return NULL;
  }

  // Check if mod_instaweb has already rewritten the HTML.  If the server is
  // setup as both the original and the proxy server, mod_pagespeed filter may
  // be applied twice. To avoid this, skip the content if it is already
  // optimized by mod_pagespeed.
  // TODO(sligocki): Move inside PSOL.
  if (apr_table_get(request->headers_out, kModPagespeedHeader) != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: X-Mod-Pagespeed header set.");
    return NULL;
  }

  // Determine the absolute URL for this request.
  const char* absolute_url = InstawebContext::MakeRequestUrl(*options, request);
  apache_request->set_url(absolute_url);

  // The final URL.  This is same as absolute_url but with ModPagespeed* query
  // params, if any, stripped.
  GoogleString final_url;

  scoped_ptr<RequestHeaders> request_headers(new RequestHeaders);
  ResponseHeaders response_headers;
  {
    // TODO(mmohabey): Add a hook which strips off the PageSpeed* query
    // (instead of stripping them here) params before content generation.
    GoogleUrl gurl(absolute_url);
    ApacheRequestToRequestHeaders(*request, request_headers.get());

    // Copy headers_out and err_headers_out into response_headers.
    // Note that err_headers_out will come after the headers_out in the list of
    // headers. Because of this, err_headers_out will effectively override
    // headers_out when we call GetQueryOptions as it applies the header options
    // in order.
    ApacheRequestToResponseHeaders(*request, &response_headers,
                                   &response_headers);
    int num_response_attributes = response_headers.NumAttributes();
    ServerContext::OptionsBoolPair query_options_success =
        server_context->GetQueryOptions(&gurl, request_headers.get(),
                                        &response_headers);
    ServerContext::ScanSplitHtmlRequest(request_context, options, &gurl);

    if (!query_options_success.second) {
      ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, request,
                    "Request not rewritten because PageSpeed "
                    "query-params or headers are invalid.");
      return NULL;
    }
    if (query_options_success.first != NULL) {
      use_custom_options = true;
      // TODO(sriharis): Can we use ServerContext::GetCustomOptions(
      //   request_headers.get(), NULL, query_options_success.first) here?
      // The only issue will be the XmlHttpRequest disabling of filters that
      // insert js, that is done there.
      scoped_ptr<RewriteOptions> query_options(query_options_success.first);
      RewriteOptions* merged_options = factory->NewRewriteOptions();
      merged_options->Merge(*options);
      merged_options->Merge(*query_options.get());
      // Don't run any experiments if we're handling a query params request.
      merged_options->set_running_experiment(false);
      server_context->ComputeSignature(merged_options);
      custom_options.reset(merged_options);
      options = merged_options;

      if (gurl.is_valid()) {
        // Set final url to gurl which has PageSpeed* query params
        // stripped.
        final_url = gurl.Spec().as_string();
      }

      // Write back the modified response headers if any have been stripped by
      // GetQueryOptions (which indicates that options were found).
      // Note: GetQueryOptions should not add or mutate headers, only remove
      // them.
      DCHECK(response_headers.NumAttributes() <= num_response_attributes);
      if (response_headers.NumAttributes() < num_response_attributes) {
        // Something was stripped, but we don't know if it came from
        // headers_out or err_headers_out.  We need to treat them separately.
        if (apr_is_empty_table(request->err_headers_out)) {
          // We know that response_headers were all from request->headers_out
          apr_table_clear(request->headers_out);
          AddResponseHeadersToRequest(&response_headers, NULL,
                                      options->modify_caching_headers(),
                                      request);
        } else if (apr_is_empty_table(request->headers_out)) {
          // We know that response_headers were all from err_headers_out
          apr_table_clear(request->err_headers_out);
          AddResponseHeadersToRequest(NULL, &response_headers,
                                      options->modify_caching_headers(),
                                      request);

        } else {
          // We don't know which table changed, so scan them individually and
          // write them both back. This should be a rare case and could be
          // optimized a bit if we find that we're spending time here.
          ResponseHeaders tmp_err_resp_headers, tmp_resp_headers;
          ThreadSystem* thread_system = server_context->thread_system();
          ApacheConfig unused_opts1(thread_system), unused_opts2(thread_system);

          ApacheRequestToResponseHeaders(*request, &tmp_resp_headers,
                                         &tmp_err_resp_headers);

          // Use ScanHeader's parsing logic to find and strip the PageSpeed
          // options from the headers. Use NULL for device_properties as no
          // device property information is needed for the stripping.
          RewriteQuery::ScanHeader(
              &tmp_err_resp_headers, NULL /* device_properties */,
              &unused_opts1, factory->message_handler());
          RewriteQuery::ScanHeader(
              &tmp_resp_headers, NULL  /* device_properties */, &unused_opts2,
              factory->message_handler());

          // Write the stripped headers back to the Apache record.
          apr_table_clear(request->err_headers_out);
          apr_table_clear(request->headers_out);
          AddResponseHeadersToRequest(&tmp_resp_headers, &tmp_err_resp_headers,
                                      options->modify_caching_headers(),
                                      request);
        }
      }
    }
  }

  if (final_url.empty()) {
    final_url = absolute_url;
  }

  // TODO(sligocki): Move inside PSOL.
  // Is PageSpeed turned off? We check after parsing query params so that
  // they can override .conf settings.
  if (!options->enabled()) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: PageSpeed is off");
    return NULL;
  }

  // TODO(sligocki): Move inside PSOL.
  // Do Disallow statements restrict us from rewriting this URL?
  if (!options->IsAllowed(final_url)) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: ModPagespeedDisallow");
    return NULL;
  }

  InstawebContext* context = new InstawebContext(
      request, request_headers.release(), *content_type, server_context,
      final_url, request_context, use_custom_options, *options);

  // TODO(sligocki): Move inside PSOL.
  InstawebContext::ContentEncoding encoding = context->content_encoding();
  if ((encoding == InstawebContext::kGzip) ||
      (encoding == InstawebContext::kDeflate)) {
    // Unset the content encoding because the InstawebContext will decode the
    // content before parsing.
    apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);
    apr_table_unset(request->err_headers_out, HttpAttributes::kContentEncoding);
  } else if (encoding == InstawebContext::kOther) {
    // We don't know the encoding, so we cannot rewrite the HTML.
    const char* encoding = apr_table_get(request->headers_out,
                                         HttpAttributes::kContentEncoding);
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Request not rewritten because: Content-Encoding is "
                  "unsupported (was %s)", encoding);
    return NULL;
  }

  // Set X-Mod-Pagespeed header.
  // TODO(sligocki): Move inside PSOL.
  apr_table_set(request->headers_out,
                kModPagespeedHeader, options->x_header_value().c_str());

  apr_table_unset(request->headers_out, HttpAttributes::kContentLength);
  apr_table_unset(request->headers_out, "Content-MD5");
  apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);

  // Make sure compression is enabled for this response.
  ap_add_output_filter("DEFLATE", NULL, request, request->connection);

  if (options->modify_caching_headers()) {
    ap_add_output_filter(kModPagespeedFixHeadersName, NULL, request,
                         request->connection);
  }

  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                "Request accepted.");
  return context;
}

// This returns 'false' if the output filter should stop its loop over
// the brigade and return an error.
bool process_bucket(ap_filter_t* filter, request_rec* request,
                    InstawebContext* context, apr_bucket* bucket,
                    apr_status_t* return_code) {
  // Remove the bucket from the old brigade. We will create new bucket or
  // reuse the bucket to insert into the new brigade.
  APR_BUCKET_REMOVE(bucket);
  *return_code = APR_SUCCESS;
  apr_bucket_brigade* context_bucket_brigade = context->bucket_brigade();
  apr_bucket* new_bucket = NULL;
  if (!APR_BUCKET_IS_METADATA(bucket)) {
    const char* buf = NULL;
    size_t bytes = 0;
    *return_code = apr_bucket_read(bucket, &buf, &bytes, APR_BLOCK_READ);
    if (*return_code == APR_SUCCESS) {
      new_bucket = rewrite_html(context, request, REWRITE, buf, bytes);
    } else {
      ap_log_rerror(APLOG_MARK, APLOG_ERR, *return_code, request,
                    "Reading bucket failed (rcode=%d)", *return_code);
      apr_bucket_delete(bucket);
      return false;
    }
    // Processed the bucket, now delete it.
    apr_bucket_delete(bucket);
    if (new_bucket != NULL) {
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
    }
  } else if (APR_BUCKET_IS_EOS(bucket)) {
    new_bucket = rewrite_html(context, request, FINISH, NULL, 0);
    if (new_bucket != NULL) {
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
    }
    // Insert the EOS bucket to the new brigade.
    APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
    // OK, we have seen the EOS. Time to pass it along down the chain.
    *return_code = ap_pass_brigade(filter->next, context_bucket_brigade);
    return false;
  } else if (APR_BUCKET_IS_FLUSH(bucket)) {
    new_bucket = rewrite_html(context, request, FLUSH, NULL, 0);
    if (new_bucket != NULL) {
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
    }
    APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
    // OK, Time to flush, pass it along down the chain.
    *return_code = ap_pass_brigade(filter->next, context_bucket_brigade);
    if (*return_code != APR_SUCCESS) {
      return false;
    }
  } else {
    // TODO(lsong): remove this log.
    ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                  "Unknown meta data");
    APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
  }
  return true;
}

// Entry point from Apache for streaming HTML-like content.
apr_status_t instaweb_out_filter(ap_filter_t* filter, apr_bucket_brigade* bb) {
  // Do nothing if there is nothing, and stop passing to other filters.
  if (APR_BRIGADE_EMPTY(bb)) {
    return APR_SUCCESS;
  }

  request_rec* request = filter->r;
  InstawebContext* context = static_cast<InstawebContext*>(filter->ctx);

  // Initialize per-request context structure.  Note that instaweb_out_filter
  // may get called multiple times per HTTP request, and this occurs only
  // on the first call.
  if (context == NULL) {
    context = build_context_for_request(request);
    if (context == NULL) {
      ap_remove_output_filter(filter);
      return ap_pass_brigade(filter->next, bb);
    }
    filter->ctx = context;
  }

  ApacheServerContext* server_context = context->apache_server_context();
  ScopedTimer timer(server_context, &ApacheServerContext::AddHtmlRewriteTimeUs);

  apr_status_t return_code = APR_SUCCESS;
  while (!APR_BRIGADE_EMPTY(bb)) {
    apr_bucket* bucket = APR_BRIGADE_FIRST(bb);
    if (!process_bucket(filter, request, context, bucket, &return_code)) {
      return return_code;
    }
  }

  apr_brigade_cleanup(bb);
  return return_code;
}

// This is called when mod_pagespeed rewrites HTML.  At this time we do
// not want rewritten HTML to be cached, though we may relax that policy
// with some pagespeed.conf settings in the future.
//
// This function removes any expires or cache-control settings added
// by the user's .conf files, and puts in headers to disable caching.
//
// We expect this to run after mod_headers and mod_expires, triggered
// by the call to ap_add_output_filter(kModPagespeedFixHeadersName...)
// in build_context_for_request.
//
// NOTE: This is disabled if users set "ModifyCachingHeaders false".
apr_status_t instaweb_fix_headers_filter(
    ap_filter_t* filter, apr_bucket_brigade* bb) {
  request_rec* request = filter->r;

  // Escape ASAP if we're in unplugged mode.
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  if (server_context->config()->unplugged()) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  // TODO(sligocki): Consider moving inside PSOL.  Note that this is a
  // little thornier than it looks because PSOL headers are different
  // from Apache headers and to share code easily we'd have to
  // translate.  We can do that easily but it seems like a waste of
  // CPU time since this will occur on every HTML request.  However,
  // there is hope in pagespeed/kernel/http/caching_headers.h, which
  // provides an abstracted interface to any underlying representation.
  // We could build on that pattern to do platform-independent header
  // manipulations in PSOL rather than direct calls to ResponseHeaders.
  //
  // TODO(jmarantz): merge this logic with that in
  // ResponseHeaders::CacheControlValuesToPreserve and
  // ServerContext::ApplyInputCacheControl
  DisableCaching(request);

  // TODO(sligocki): Why remove ourselves? Is it to assure that this filter
  // only looks at the first bucket in the brigade?
  ap_remove_output_filter(filter);
  return ap_pass_brigade(filter->next, bb);
}

// Entry point from Apache for recording resources for IPRO.
// Modeled loosely after ap_content_length_filter() in protocol.c.
// TODO(sligocki): Perhaps we can merge this filter with ApacheToMpsFilter().
apr_status_t instaweb_in_place_filter(ap_filter_t* filter,
                                      apr_bucket_brigade* bb) {
  // Do nothing if there is nothing, and stop passing to other filters.
  if (APR_BRIGADE_EMPTY(bb)) {
    return APR_SUCCESS;
  }

  request_rec* request = filter->r;

  // Escape ASAP if we're in unplugged mode.
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  if (server_context->config()->unplugged()) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  // This should always be set by handle_as_in_place() in instaweb_handler.cc.
  InPlaceResourceRecorder* recorder =
      static_cast<InPlaceResourceRecorder*>(filter->ctx);
  CHECK(recorder != NULL);
  MessageHandler* handler = recorder->handler();
  handler->Message(kInfo, "Attempting to save resource in-place: %s",
                   recorder->url().c_str());

  // Iterate through all buckets, saving content in the recorder and passing
  // the buckets along when there is a flush.
  for (apr_bucket* bucket = APR_BRIGADE_FIRST(bb);
       bucket != APR_BRIGADE_SENTINEL(bb);
       bucket = APR_BUCKET_NEXT(bucket)) {
    if (!APR_BUCKET_IS_METADATA(bucket)) {
      // Content bucket.
      const char* buf = NULL;
      size_t bytes = 0;
      // Note: Each call to apr_bucket_read() on a FILE bucket will pull in
      // some of the file into a HEAP bucket. Since we do not pass those
      // buckets to the next filter until the end of this function, we are
      // basically buffering up the entire size of the file into memory.
      //
      // Apache documentation says not to do this because of the memory issues:
      //   http://httpd.apache.org/docs/developer/output-filters.html#filtering
      // ... but since our whole point here is to load the resource into
      // memory, it seems reasonable.
      //
      // TODO(sligocki): Should we do an APR_NONBLOCK_READ? mod_content_length
      // seems to do that, but has to deal with APR_STATUS_IS_EAGAIN() and
      // splitting the brigade, etc.
      apr_status_t return_code = apr_bucket_read(bucket, &buf, &bytes,
                                                 APR_BLOCK_READ);
      StringPiece contents(buf, bytes);
      if (return_code == APR_SUCCESS) {
        // Ignore headers for now. They are checked by
        // instaweb_in_place_check_headers_filter.
        recorder->Write(contents, recorder->handler());
      } else {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, return_code, request,
                      "Reading bucket failed (rcode=%d)", return_code);
        recorder->Fail();
        return return_code;
      }
    } else if (APR_BUCKET_IS_FLUSH(bucket)) {
      recorder->Flush(recorder->handler());
    } else if (APR_BUCKET_IS_EOS(bucket)) {
      // instaweb_in_place_check_headers_filter calls
      // recorder->DoneAndSetHeaders().
      break;
    }
  }

  return ap_pass_brigade(filter->next, bb);
}

// Runs after mod_headers and other filters which muck with the headers.
// We cannot run instaweb_in_place_filter after them because by then the
// content is gzipped.
// TODO(sligocki): Run as a single filter after mod_headers, etc. using
// an inflater to gunzip the file? Or storing the gzipped version in cache?
//
// The sole purpose of this filter is to pass the finalized headers to recorder.
apr_status_t instaweb_in_place_check_headers_filter(ap_filter_t* filter,
                                                    apr_bucket_brigade* bb) {
  // Do nothing if there is nothing, and stop passing to other filters.
  if (APR_BRIGADE_EMPTY(bb)) {
    return APR_SUCCESS;
  }

  request_rec* request = filter->r;
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(request->server);
  // Escape ASAP if we're in unplugged mode.
  if (server_context->config()->unplugged()) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  // This should always be set by handle_as_in_place() in instaweb_handler.cc.
  InPlaceResourceRecorder* recorder =
      static_cast<InPlaceResourceRecorder*>(filter->ctx);
  CHECK(recorder != NULL);

  // Although headers come in first bucket, we do not want to call Done
  // until last bucket comes in, so iterate to EOS bucket.
  for (apr_bucket* bucket = APR_BRIGADE_FIRST(bb);
       bucket != APR_BRIGADE_SENTINEL(bb);
       bucket = APR_BUCKET_NEXT(bucket)) {
    if (APR_BUCKET_IS_EOS(bucket)) {
      ResponseHeaders response_headers;
      ApacheRequestToResponseHeaders(*request, &response_headers, NULL);

      // Note: For some reason Apache never actually sets the Date header in
      // request->headers_out, but without it set we consider it uncacheable,
      // so we set it here.
      // TODO(sligocki): Perhaps we should stop requiring Date header to
      // consider resources cacheable?
      AprTimer timer;
      response_headers.SetDate(timer.NowMs());
      response_headers.ComputeCaching();

      recorder->DoneAndSetHeaders(&response_headers);
    }
  }

  return ap_pass_brigade(filter->next, bb);
}

void pagespeed_child_init(apr_pool_t* pool, server_rec* server) {
  // Escape ASAP if we're in unplugged mode.
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(server);
  if (server_context->config()->unplugged()) {
    return;
  }

  // Create PageSpeed context used by instaweb rewrite-driver.  This is
  // per-process, so we initialize all the server's context by iterating the
  // server lists in server->next.
  ApacheRewriteDriverFactory* factory = apache_process_context.factory(server);
  factory->ChildInit();
  for (; server != NULL; server = server->next) {
    ApacheServerContext* server_context =
        InstawebContext::ServerContextFromServerRec(server);
    DCHECK(server_context != NULL);
    DCHECK(server_context->initialized());
  }
}

bool give_dir_apache_user_permissions(ApacheRewriteDriverFactory* factory,
                                      const GoogleString& path) {
  // (Apache will not switch from current euid if it's not root --- see
  //  http://httpd.apache.org/docs/2.2/mod/mpm_common.html#user).
  if (geteuid() != 0) {
    return true;
  }

  // .user_id, .group_id default to -1 if they haven't been parsed yet.
  if ((unixd_config.user_id == 0) ||
      (unixd_config.user_id == static_cast<uid_t>(-1)) ||
      (unixd_config.group_id == 0) ||
      (unixd_config.group_id == static_cast<gid_t>(-1))) {
    return true;
  }

  if (chown(path.c_str(), unixd_config.user_id,
            unixd_config.group_id) != 0) {
    factory->message_handler()->Message(
        kError, "Unable to set proper ownership of %s (%s)",
        path.c_str(), strerror(errno));
    return false;
  }
  return true;
}

// If we are running as root, hands over the ownership of data directories
// we made to the eventual Apache uid/gid.
bool give_apache_user_permissions(ApacheRewriteDriverFactory* factory) {
  const StringSet& created_dirs = factory->created_directories();
  bool ret = true;
  for (StringSet::iterator i = created_dirs.begin();
       i != created_dirs.end(); ++i) {
    ret &= give_dir_apache_user_permissions(factory, *i);
  }
  return ret;
}

// Create directory and make sure permissions are set correctly so that
// Apache processes can read and write from it.
const char* init_dir(ApacheServerContext* server_context,
                     apr_pool_t* pool,
                     const char* directive_name,
                     const char* path) {
  const char* ret = NULL;
  if (*path != '/') {
    ret = apr_pstrcat(pool, directive_name, " ", path,
                      " must start with a slash.", NULL);
  } else {
    if (!server_context->InitPath(path) ||
        !give_apache_user_permissions(server_context->apache_factory())) {
      ret = apr_pstrcat(pool, "Directory ", path, " could not be created "
                        "or permissions could not be set.", NULL);
    }
  }
  return ret;
}

// Hook from Apache for initialization after config is read.
// Initialize statistics, set appropriate directory permissions, etc.
int pagespeed_post_config(apr_pool_t* pool, apr_pool_t* plog, apr_pool_t* ptemp,
                          server_rec* server_list) {
  // This routine is complicated by the fact that statistics use inter-process
  // mutexes and have static data, which co-mingles poorly with this otherwise
  // re-entrant module.  The situation that gets interesting is when there are
  // multiple VirtualHosts, some of which have statistics enabled and some of
  // which don't.  We don't want the behavior to be order-dependent so we
  // do multiple passes.
  //
  // TODO(jmarantz): test VirtualHost

  ApacheRewriteDriverFactory* factory = apache_process_context.factory(
      server_list);

  // In the first pass, we see whether any of the servers have
  // statistics enabled, if found, do the static initialization of
  // statistics to establish global memory segments.
  Statistics* statistics = NULL;
  std::set<ApacheServerContext*> server_contexts_covered;
  for (server_rec* server = server_list; server != NULL;
       server = server->next) {
    ApacheServerContext* server_context =
        InstawebContext::ServerContextFromServerRec(server);
    if (server_contexts_covered.insert(server_context).second) {
      CHECK(server_context != NULL);
      server_context->CollapseConfigOverlaysAndComputeSignatures();
      ApacheConfig* config = server_context->config();

      // Escape ASAP if we're in unplugged mode.
      if (config->unplugged()) {
        continue;
      }

      if (config->enabled()) {
        GoogleString file_cache_path = config->file_cache_path();
        if (file_cache_path.empty()) {
          server_context->message_handler()->Message(
              kError, "mod_pagespeed is enabled. %s must not be empty:"
              " defn_name=%s"
              " defn_line_number=%d"
              " server_hostname=%s"
              " port=%d",
              kModPagespeedFileCachePath,
              server->defn_name, server->defn_line_number,
              server->server_hostname, server->port);
          return HTTP_INTERNAL_SERVER_ERROR;
        }
      }

      // Lazily create shared-memory statistics if enabled in any
      // config, even when mod_pagespeed is totally disabled.  This
      // allows statistics to work if mod_pagespeed gets turned on via
      // .htaccess or query param.
      if ((statistics == NULL) && config->statistics_enabled()) {
        statistics = factory->MakeGlobalSharedMemStatistics(config);
      }

      // If config has statistics on and we have per-vhost statistics on
      // as well, then set it up.
      if (config->statistics_enabled() && factory->use_per_vhost_statistics()) {
        server_context->CreateLocalStatistics(statistics);
      }
    }
  }

  // chown any directories we created. We may have to do it here in
  // post_config since we may not have our user/group yet during parse
  // (example: Fedora 11).
  //
  // We also have to do it during the parse, however, since if we're started
  // to /just/ check the config with -t (as opposed to doing it as a
  // preliminary for a proper startup) we won't get a post_config!
  give_apache_user_permissions(factory);

  // If no shared-mem statistics are enabled, then init using the default
  // NullStatistics.
  if (statistics == NULL) {
    statistics = factory->statistics();
    ApacheRewriteDriverFactory::InitStats(statistics);
  }

  factory->RootInit();

  return OK;
}

// Here log transaction will wait for all the asynchronous resource fetchers to
// finish.
apr_status_t pagespeed_log_transaction(request_rec* request) {
  return DECLINED;
}

// Called by Apache via hook once all modules have been loaded & configured
// to let us attach to their optional functions.
void pagespeed_fetch_optional_fns() {
  attach_mod_spdy();
}

int pagespeed_modify_request(request_rec* r) {
  // Escape ASAP if we're in unplugged mode.
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(r->server);
  if (server_context->config()->unplugged()) {
    return OK;
  }

  // This method is based in part on mod_remoteip.
  conn_rec* c = r->connection;

  // Detect local requests from us.
  const char* ua = apr_table_get(r->headers_in, HttpAttributes::kUserAgent);
  if (ua != NULL &&
      strstr(ua, " mod_pagespeed/" MOD_PAGESPEED_VERSION_STRING) != NULL) {
#ifdef MPS_APACHE_24
    apr_sockaddr_t* client_addr = c->client_addr;
#else
    apr_sockaddr_t* client_addr = c->remote_addr;
#endif

    if (LoopbackRouteFetcher::IsLoopbackAddr(client_addr)) {
      // Rewrite the client IP in Apache's records to 224.0.0.0, which is a
      // multicast address that should hence not be used by anyone, and at the
      // very least is clearly not 127.0.0.1.
      apr_sockaddr_t* untrusted_sockaddr = NULL;

      // This builds a sockaddr object corresponding to 224.0.0.0
      CHECK_EQ(APR_SUCCESS,
               apr_sockaddr_info_get(&untrusted_sockaddr, "224.0.0.0", APR_INET,
                                     80, 0, client_addr->pool));

      char* untrusted_ip_str = apr_pstrdup(client_addr->pool, "224.0.0.0");
#ifdef MPS_APACHE_24
      r->useragent_ip = untrusted_ip_str;
      r->useragent_addr = untrusted_sockaddr;
#else
      c->remote_ip = untrusted_ip_str;
      c->remote_addr = untrusted_sockaddr;
#endif

      // We set the remote host header to be an empty string --- Apache uses
      // that if there is an error, so it shouldn't pass through any ACLs.
      c->remote_host = apr_pstrdup(client_addr->pool, "");
    }
  }
  return OK;
}

// This function is a callback and it declares what
// other functions should be called for request
// processing and configuration requests. This
// callback function declares the Handlers for
// other events.
void mod_pagespeed_register_hooks(apr_pool_t* pool) {
  // Enable logging using pagespeed style
  log_message_handler::Install(pool);

  // Use instaweb to handle generated resources.
  ap_hook_handler(instaweb_handler, NULL, NULL, APR_HOOK_FIRST - 1);

  // Try to provide more accurate IP information for requests we create.
  ap_hook_post_read_request(pagespeed_modify_request, NULL, NULL,
                            APR_HOOK_FIRST);

  // We register our output filter at (AP_FTYPE_RESOURCE + 1) so that
  // mod_pagespeed runs after mod_include.  See Issue
  // http://code.google.com/p/modpagespeed/issues/detail?id=182
  // and httpd/src/modules/filters/mod_include.c, which initializes
  // server-side-includes with ap_register_output_filter(...AP_FTYPE_RESOURCE).
  ap_register_output_filter(
      kModPagespeedFilterName, instaweb_out_filter, NULL,
      static_cast<ap_filter_type>(AP_FTYPE_RESOURCE + 1));

  // For HTML rewrites, we must apply our caching semantics later
  // in the filter-chain than mod_headers or mod_expires.  See:
  //   APACHE_DIST/src/modules/metadata/mod_headers.c:857
  //         --> mod_headers is installed at AP_FTYPE_CONTENT_SET
  //   APACHE_DIST/src/modules/metadata/mod_expires.c:554
  //         --> mod_expires is installed at AP_FTYPE_CONTENT_SET - 2
  // Thus we can override its settings by installing at +1.
  ap_register_output_filter(
      kModPagespeedFixHeadersName, instaweb_fix_headers_filter, NULL,
      static_cast<ap_filter_type>(AP_FTYPE_CONTENT_SET + 1));

  // Run after contents are set, but before mod_deflate, which runs at
  // AP_FTYPE_CONTENT_SET.
  ap_register_output_filter(
      kModPagespeedInPlaceFilterName, instaweb_in_place_filter, NULL,
      static_cast<ap_filter_type>(AP_FTYPE_CONTENT_SET - 1));
  // Run after headers are set by mod_headers, mod_expires, etc. and
  // after Content-Type has been set (which appears to be at
  // AP_FTYPE_PROTOCOL).
  ap_register_output_filter(
      kModPagespeedInPlaceCheckHeadersName,
      instaweb_in_place_check_headers_filter, NULL,
      static_cast<ap_filter_type>(AP_FTYPE_PROTOCOL + 1));

  ap_hook_post_config(pagespeed_post_config, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(pagespeed_child_init, NULL, NULL, APR_HOOK_LAST);
  ap_hook_log_transaction(pagespeed_log_transaction, NULL, NULL, APR_HOOK_LAST);

  // mod_rewrite damages the URLs written by mod_pagespeed.  See
  // Issues 63 & 72.  To defend against this, we must either add
  // additional mod_rewrite rules to exclude pagespeed resources or
  // pre-scan for pagespeed resources before mod_rewrite runs and copy
  // the URL somewhere safe (a request->note) before mod_rewrite
  // corrupts it.  The latter is easier to deploy as it does not
  // require users editing their rewrite rules for mod_pagespeed.
  // mod_rewrite registers at APR_HOOK_FIRST.  We'd like to leave
  // space for user modules at APR_HOOK_FIRST-1, so we go to
  // APR_HOOK_FIRST - 2.
  ap_hook_translate_name(save_url_hook, NULL, NULL,
                         APR_HOOK_FIRST - 2);

  // By default, apache imposes limitations on URL segments of around
  // 256 characters that appear to correspond to filename limitations.
  // To prevent that, we hook map_to_storage for our own purposes.
  ap_hook_map_to_storage(instaweb_map_to_storage, NULL, NULL,
                         APR_HOOK_FIRST - 2);

  // Hook which will let us connect to optional functions mod_spdy
  // exports.
  ap_hook_optional_fn_retrieve(
      pagespeed_fetch_optional_fns,  // hook function to be called
      NULL,                          // predecessors
      NULL,                          // successors
      APR_HOOK_MIDDLE);              // position

  ModSpdyFetcher::Initialize();
}

apr_status_t pagespeed_child_exit(void* data) {
  ApacheServerContext* server_context = static_cast<ApacheServerContext*>(data);
  if (server_context->PoolDestroyed()) {
    // When the last server context is destroyed, it's important that we also
    // clean up the factory, so we don't end up with dangling pointers in case
    // we are not unloaded fully on a config check (e.g. on Ubuntu 11).
    apache_process_context.factory_.reset(NULL);
  }
  return APR_SUCCESS;
}

void* mod_pagespeed_create_server_config(apr_pool_t* pool, server_rec* server) {
  // Note: when statically loaded server->module_config is NULL when
  // initializing and this is called for the first time.
  ApacheServerContext* server_context =
      server->module_config == NULL
          ? NULL
          : InstawebContext::ServerContextFromServerRec(server);

  if (server_context == NULL) {
    ApacheRewriteDriverFactory* factory = apache_process_context.factory(
        server);
    server_context = factory->MakeApacheServerContext(server);
    apr_pool_cleanup_register(pool, server_context, pagespeed_child_exit,
                              apr_pool_cleanup_null);
  }
  return server_context;
}

const char kBoolHint[] = " on|off";
const char kEnabledEnumHint[] = " on|off|unplugged";
const char kInt64Hint[] = " must specify a 64-bit integer";
const char kIntHint[] = " must specify a 32-bit integer";

const char* ParseHint(bool x) { return kBoolHint; }
const char* ParseHint(int x) { return kIntHint; }
const char* ParseHint(int64 x) { return kInt64Hint; }
const char* ParseHint(RewriteOptions::EnabledEnum x) {
  return kEnabledEnumHint;
}

template<typename OptType, typename Options>
const char* ParseOption(Options* options, cmd_parms* cmd,
                            void (Options::*fn)(OptType val),
                            const char* arg) {
  const char* ret = NULL;
  OptType parsed;
  if (RewriteOptions::ParseFromString(arg, &parsed)) {
    (options->*fn)(parsed);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive,
                      ParseHint(parsed), NULL);
  }
  return ret;
}

template<class Options>
const char* ParseIntBoundedOption(Options* options, cmd_parms* cmd,
                                  void (Options::*fn)(int val),
                                  const char* arg,
                                  int lower, int upper) {
  int val;
  const char* ret = NULL;
  if (StringToInt(arg, &val) &&
      val >= lower &&
      val <= upper) {
    (options->*fn)(val);
  } else {
    GoogleString message = StringPrintf(
        " must specify a 32-bit integer between %d and %d",
        lower, upper);
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive, message.c_str(),
                      NULL);
  }
  return ret;
}

void warn_deprecated(cmd_parms* cmd, const char* remedy) {
  ap_log_error(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, cmd->server,
               "%s is deprecated.  %s",
               cmd->directive->directive, remedy);
}

// Determines the Option structure into which to write a parsed directive.
// If the directive was parsed from the default pagespeed.conf file then
// we will write the information into the factory's RewriteOptions. In that
// case, it's also possible that an overlay config for SPDY should be used,
// in which case we will store it inside the directive object.
//
// However, if this was parsed from a Directory scope or .htaccess file then we
// will be using the RewriteOptions structure from a tree of ApacheConfig
// objects that is built up per-request.
//
// Returns NULL if successful, error string otherwise.
// Writes out the ApacheConfig* into *config_out.
static const char* CmdOptions(const cmd_parms* cmd, void* data,
                              ApacheConfig** config_out) {
  ApacheConfig* config = static_cast<ApacheConfig*>(data);
  if (config == NULL) {
    // See if there is an overlay config.
    if (cmd->directive->data != NULL) {
      config = static_cast<ApacheConfig*>(cmd->directive->data);
    } else {
      ApacheServerContext* server_context =
          InstawebContext::ServerContextFromServerRec(cmd->server);
      config = server_context->config();
    }
  } else {
    // If we're here, we are inside path-specific configuration, so we should
    // not see SPDY vs. non-SPDY distinction.
    if (cmd->directive->data != NULL) {
      *config_out = NULL;
      return "Can't use <ModPagespeedIf except at top-level or VirtualHost "
             "context";
    }
  }
  *config_out = config;
  return NULL;
}

// This should be called for global options to see if they were used properly.
// In particular, it returns an error string if a global option is inside a
// <ModPagespeedIf. It also either warns or errors out if we're using a global
// option inside a virtual host, depending on "mode".
//
// Returns NULL if successful, error string otherwise.
static char* CheckGlobalOption(const cmd_parms* cmd,
                               VHostHandling mode,
                               MessageHandler* handler) {
  if (cmd->server->is_virtual) {
    char* vhost_error = apr_pstrcat(
        cmd->pool, "Directive ", cmd->directive->directive,
        " used inside a <VirtualHost> but applies globally.",
        (mode == kTolerateInVHost ?
            " Accepting for backwards compatibility. " :
            NULL),
        NULL);
    if (mode == kErrorInVHost) {
      return vhost_error;
    } else {
      handler->Message(kWarning, "%s", vhost_error);
    }
  }
  if (cmd->directive->data != NULL) {
    return apr_pstrcat(
        cmd->pool, "Global directive ", cmd->directive->directive,
        " invalid inside conditional.", NULL);
  }
  return NULL;
}

// Returns true if standard parsing handled the option and sets *err_msg to NULL
// if OK, and to the error string managed in cmd->pool otherwise.
bool StandardParsingHandled(
    cmd_parms* cmd, RewriteOptions::OptionSettingResult result,
    const GoogleString& msg, const char** err_msg) {
  switch (result) {
    case RewriteOptions::kOptionOk:
      *err_msg = NULL;  // No error.
      return true;
    case RewriteOptions::kOptionNameUnknown:
      // RewriteOptions didn't recognize the option, but we might do so
      // with our own code.
      return false;
    case RewriteOptions::kOptionValueInvalid:
      // The option is recognized, but the value is not. Return the error
      // message.
      *err_msg = apr_pstrdup(cmd->pool, msg.c_str());
      return true;
  }
  LOG(DFATAL) << "Should be unreachable";
  return true;
}

// Callback function that parses a single-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective(cmd_parms* cmd, void* data, const char* arg) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(cmd->server);
  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  MessageHandler* handler = factory->message_handler();
  StringPiece directive(cmd->directive->directive);
  StringPiece prefix(RewriteQuery::kModPagespeed);
  const char* ret = NULL;
  ApacheConfig* config;
  ret = CmdOptions(cmd, data, &config);
  if (ret != NULL) {
    return ret;
  }

  // We have "FileCachePath" mapped in gperf, but here we do more than just
  // setting the option. This must precede the call to SetOptionFromName which
  // would catch this directive but miss the call to
  // give_apache_user_permissions.
  if (StringCaseEqual(directive, kModPagespeedFileCachePath)) {
    ret = init_dir(server_context, cmd->pool, kModPagespeedFileCachePath, arg);
    if (ret == NULL) {
      config->set_file_cache_path(arg);
    }
    return ret;
  }
  if (StringCaseEqual(directive, kModPagespeedLogDir)) {
    ret = init_dir(server_context, cmd->pool, kModPagespeedLogDir, arg);
    if (ret == NULL) {
      config->set_log_dir(arg);
    }
    return ret;
  }

  // Rename deprecated options so lookup below will succeed.
  if (StringCaseEqual(directive, kModPagespeedImgInlineMaxBytes)) {
    directive = kModPagespeedImageInlineMaxBytes;
  } else if (StringCaseEqual(directive, kModPagespeedImgMaxRewritesAtOnce)) {
    directive = kModPagespeedImageMaxRewritesAtOnce;
  }

  // See whether generic RewriteOptions name handling can figure this one out.
  if (directive.starts_with(prefix)) {
    GoogleString msg;
    RewriteOptions::OptionSettingResult result =
        config->ParseAndSetOptionFromName1(
            directive.substr(prefix.size()), arg, &msg, handler);
    if (StandardParsingHandled(cmd, result, msg, &ret)) {
      return ret;
    }
  }

  // Options which we handle manually.
  if (StringCaseEqual(directive, RewriteQuery::kModPagespeed)) {
    ret = ParseOption<RewriteOptions::EnabledEnum>(
        static_cast<RewriteOptions*>(config), cmd, &RewriteOptions::set_enabled,
        arg);
  } else if (StringCaseEqual(directive,
                             kModPagespeedDangerPermitFetchFromUnknownHosts)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_disable_loopback_routing, arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedFetchHttps)) {
    ret = CheckGlobalOption(cmd, kTolerateInVHost, handler);
    if (ret == NULL) {
      GoogleString error_message;
      if (!factory->SetHttpsOptions(arg, &error_message)) {
        ret = apr_pstrcat(cmd->pool, "Invalid argument '", arg, "' to ",
                          cmd->directive->directive, ": ",
                          error_message.c_str(), NULL);
      }
    }
  } else if (StringCaseEqual(directive, kModPagespeedFetchWithGzip)) {
    ret = CheckGlobalOption(cmd, kTolerateInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd, &ApacheRewriteDriverFactory::set_fetch_with_gzip, arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedForceCaching)) {
    ret = CheckGlobalOption(cmd, kTolerateInVHost, handler);
    if (ret == NULL) {
        ret = ParseOption<bool>(static_cast<RewriteDriverFactory*>(factory),
                              cmd, &RewriteDriverFactory::set_force_caching,
                              arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedInheritVHostConfig)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_inherit_vhost_config, arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedInstallCrashHandler)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_install_crash_handler, arg);
    }
  } else if (StringCaseEqual(directive,
                             kModPagespeedListOutstandingUrlsOnError)) {
    ret = CheckGlobalOption(cmd, kTolerateInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd,
          &ApacheRewriteDriverFactory::list_outstanding_urls_on_error, arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedMessageBufferSize)) {
    ret = CheckGlobalOption(cmd, kTolerateInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<int>(factory, cmd,
                           &ApacheRewriteDriverFactory::set_message_buffer_size,
                           arg);
    }
  } else if (StringCaseEqual(directive, kModPagespeedNumRewriteThreads)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<int>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_num_rewrite_threads, arg);
    }
  } else if (StringCaseEqual(directive,
                             kModPagespeedNumExpensiveRewriteThreads)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<int>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_num_expensive_rewrite_threads, arg);
    }
  } else if (StringCaseEqual(directive,
                             kModPagespeedTrackOriginalContentLength)) {
    ret = ParseOption<bool>(
        factory, cmd,
        &ApacheRewriteDriverFactory::set_track_original_content_length, arg);
  } else if (StringCaseEqual(directive,
                             kModPagespeedCollectRefererStatistics) ||
             StringCaseEqual(directive, kModPagespeedDisableForBots) ||
             StringCaseEqual(directive, kModPagespeedGeneratedFilePrefix) ||
             StringCaseEqual(directive, kModPagespeedHashRefererStatistics) ||
             StringCaseEqual(directive, kModPagespeedNumShards) ||
             StringCaseEqual(directive, kModPagespeedStatisticsLoggingFile) ||
             StringCaseEqual(directive,
                             kModPagespeedRefererStatisticsOutputLevel) ||
             StringCaseEqual(directive, kModPagespeedUrlPrefix)) {
    warn_deprecated(cmd, "Please remove it from your configuration.");
  } else if (StringCaseEqual(directive, kModPagespeedUsePerVHostStatistics)) {
    ret = CheckGlobalOption(cmd, kErrorInVHost, handler);
    if (ret == NULL) {
      ret = ParseOption<bool>(
          factory, cmd,
          &ApacheRewriteDriverFactory::set_use_per_vhost_statistics, arg);
    }
  } else {
    ret = apr_pstrcat(cmd->pool, "Unknown directive ",
                      directive.as_string().c_str(), NULL);
  }

  return ret;
}

// Recursively walks the configuration we've parsed inside a
// <ModPagespeedIf> block, checking to make sure it's sane, and stashing
// pointers to the overlay ApacheConfig's we will use once Apache actually
// bothers calling our ParseDirective* methods. Returns NULL if OK, error string
// on error.
static const char* ProcessParsedScope(ApacheServerContext* server_context,
                                      ap_directive_t* root, bool for_spdy) {
  for (ap_directive_t* cur = root; cur != NULL; cur = cur->next) {
    StringPiece directive(cur->directive);
    if (!StringCaseStartsWith(directive, RewriteQuery::kModPagespeed)) {
      return "Only mod_pagespeed directives should be inside <ModPagespeedIf "
             "blocks";
    }
    if (StringCaseStartsWith(directive, kModPagespeedIf)) {
      return "Can't nest <ModPagespeedIf> blocks";
    }

    if (cur->first_child != NULL) {
      const char* kid_result = ProcessParsedScope(
          server_context, cur->first_child, for_spdy);
      if (kid_result != NULL) {
        return kid_result;
      }
    }

    // Store the appropriate config to use in the ap_directive_t's
    // module data pointer, so we can retrieve it in CmdOptions when executing
    // parsing callback for it.
    cur->data = for_spdy ?
        server_context->SpdyConfigOverlay() :
        server_context->NonSpdyConfigOverlay();
  }
  return NULL;  // All OK.
}

// Callback that parses <ModPagespeedIf>. Unlike with ParseDirective*, we're
// supposed to make a new directive tree, and return it out via *mconfig. It
// will have its directives parsed by Apache at some point later.
static const char* ParseScope(cmd_parms* cmd, ap_directive_t** mconfig,
                              const char* arg) {
  StringPiece mode(arg);
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(cmd->server);

  bool for_spdy = false;
  if (StringCaseEqual(mode, "spdy>")) {
    for_spdy = true;
  } else if (StringCaseEqual(mode, "!spdy>")) {
    for_spdy = false;
  } else {
    return "Conditional must be spdy or !spdy.";
  }

  // We need to manually check nesting since Apache's code doesn't seem to catch
  // violations for sections that parse blocks like <ModPagespeedIf>
  // (technically, commands with EXEC_ON_READ set).
  //
  // Unfortunately, ap_check_cmd_context doesn't work entirely
  // right, either, so we do our own handling inside CmdOptions as well; this is
  // kept mostly to produce a nice complaint in case someone puts
  // a <ModPagespeedIf> inside a <Limit>.
  const char* ret =
      ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE | NOT_IN_LIMIT);
  if (ret != NULL) {
    return ret;
  }

  // Recursively parse this section. This is basically copy-pasted from
  // mod_version.c in Apache sources.
  ap_directive_t* parent = NULL;
  ap_directive_t* current = NULL;

  ret = ap_build_cont_config(cmd->pool, cmd->temp_pool, cmd,
                             &current, &parent,
                             apr_pstrdup(cmd->pool, kModPagespeedIf));
  *mconfig = current;

  // Do our syntax checking and stash some ApacheConfig pointers.
  if (ret == NULL) {
    ret = ProcessParsedScope(server_context, current, for_spdy);
  }

  return ret;
}

// Callback function that parses a two-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective2(cmd_parms* cmd, void* data,
                                   const char* arg1, const char* arg2) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(cmd->server);
  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  MessageHandler* handler = factory->message_handler();

  ApacheConfig* config;
  const char* ret = CmdOptions(cmd, data, &config);
  if (ret != NULL) {
    return ret;
  }

  StringPiece prefix(RewriteQuery::kModPagespeed);
  StringPiece directive = cmd->directive->directive;
  // Go through generic path first.
  if (directive.starts_with(prefix)) {
    GoogleString msg;
    RewriteOptions::OptionSettingResult result =
        config->ParseAndSetOptionFromName2(
            directive.substr(prefix.size()), arg1, arg2, &msg, handler);
    if (StandardParsingHandled(cmd, result, msg, &ret)) {
      return ret;
    }
  }

  if (StringCaseEqual(directive,
                      kModPagespeedCreateSharedMemoryMetadataCache)) {
    int64 kb = 0;
    if (!StringToInt64(arg2, &kb) || kb < 0) {
      return apr_pstrcat(cmd->pool, cmd->directive->directive,
                         " size_kb must be a positive 64-bit integer", NULL);
    }
    GoogleString message;
    bool ok = factory->caches()->CreateShmMetadataCache(arg1, kb, &message);
    if (!ok) {
      return apr_pstrdup(cmd->pool, message.c_str());
    }
  } else {
    return "Unknown directive.";
  }
  return ret;
}

// Callback function that parses a three-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective3(
    cmd_parms* cmd, void* data,
    const char* arg1, const char* arg2, const char* arg3) {
  ApacheServerContext* server_context =
      InstawebContext::ServerContextFromServerRec(cmd->server);
  ApacheRewriteDriverFactory* factory = server_context->apache_factory();
  MessageHandler* handler = factory->message_handler();
  ApacheConfig* config;
  const char* ret = CmdOptions(cmd, data, &config);
  if (ret != NULL) {
    return ret;
  }

  StringPiece prefix(RewriteQuery::kModPagespeed);
  StringPiece directive = cmd->directive->directive;
  // Go through generic path first.
  if (directive.starts_with(prefix)) {
    GoogleString msg;
    RewriteOptions::OptionSettingResult result =
        config->ParseAndSetOptionFromName3(
            directive.substr(prefix.size()), arg1, arg2, arg3, &msg, handler);
    if (StandardParsingHandled(cmd, result, msg, &ret)) {
      return ret;
    }
  }

  return apr_pstrcat(cmd->pool, cmd->directive->directive,
                     " unknown directive.", NULL);
}

// Setting up Apache options is cumbersome for several reasons:
//
// 1. Apache appears to require the option table be entirely constructed
//    using static data.  So we cannot use helper functions to create the
//    helper table, so that we can populate it from another table.
// 2. You have to fill in the table with a function pointer with a K&R
//    C declaration that does not specify its argument types.  There appears
//    to be a type-correct union hidden behind an ifdef for
//    AP_HAVE_DESIGNATED_INITIALIZER, but that doesn't work.  It gives a
//    syntax error; its comments indicate it is there for Doxygen.
// 3. Although you have to pre-declare all the options, you need to again
//    dispatch based on the name of the options.  You could, conceivably,
//    provide a different function pointer for each call.  This might look
//    feasible with the 'mconfig' argument to AP_INIT_TAKE1, but mconfig
//    must be specified in a static initializer.  So it wouldn't be that easy
//    to, say, create a C++ object for each config parameter.
//
// Googling for AP_MODULE_DECLARE_DATA didn't shed any light on how to do this
// using a style suitable for programming after 1980.  So all we can do is make
// this a little less ugly with wrapper macros and helper functions.
//
// TODO(jmarantz): investigate usage of RSRC_CONF -- perhaps many of these
// options should be allowable inside a Directory or Location by ORing in
// ACCESS_CONF to RSRC_CONF.

#define APACHE_CONFIG_OPTION(name, help) \
  AP_INIT_TAKE1(name, reinterpret_cast<const char*(*)()>(ParseDirective), \
                NULL, RSRC_CONF, help)
#define APACHE_CONFIG_DIR_OPTION(name, help) \
  AP_INIT_TAKE1(name, reinterpret_cast<const char*(*)()>(ParseDirective), \
                NULL, OR_ALL, help)

// For stuff similar to <IfVersion>, and the like.
// Note that Apache does not seem to apply RSRC_CONF (only global/vhost)
// enforcement for these, so they require manual checking.
#define APACHE_SCOPE_OPTION(name, help) \
  AP_INIT_TAKE1(name, reinterpret_cast<const char*(*)()>(ParseScope), \
                NULL, RSRC_CONF | EXEC_ON_READ, help)

// Like APACHE_CONFIG_OPTION, but gets 2 arguments.
#define APACHE_CONFIG_OPTION2(name, help) \
  AP_INIT_TAKE2(name, reinterpret_cast<const char*(*)()>(ParseDirective2), \
                NULL, RSRC_CONF, help)
#define APACHE_CONFIG_DIR_OPTION2(name, help) \
  AP_INIT_TAKE2(name, reinterpret_cast<const char*(*)()>(ParseDirective2), \
                NULL, OR_ALL, help)

// APACHE_CONFIG_OPTION for 3 arguments
#define APACHE_CONFIG_DIR_OPTION3(name, help) \
  AP_INIT_TAKE3(name, reinterpret_cast<const char*(*)()>(ParseDirective3), \
                NULL, OR_ALL, help)

// APACHE_CONFIG_OPTION for 2 or 3 arguments
#define APACHE_CONFIG_DIR_OPTION23(name, help) \
  AP_INIT_TAKE23(name, reinterpret_cast<const char*(*)()>(ParseDirective3), \
                 NULL, OR_ALL, help)

static const command_rec mod_pagespeed_filter_cmds[] = {
  // Special conditional op.
  APACHE_SCOPE_OPTION(
      kModPagespeedIf, "Conditionally apply some mod_pagespeed options. "
      "Possible arguments: spdy, !spdy"),

  APACHE_CONFIG_DIR_OPTION(RewriteQuery::kModPagespeed, "Enable instaweb"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedAllow,
        "wildcard_spec for urls"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisableFilters,
        "Comma-separated list of disabled filters"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisallow,
        "wildcard_spec for urls"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisableForBots, "No longer used."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDomain,
        "Authorize mod_pagespeed to rewrite resources in a domain."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDownstreamCachePurgeLocationPrefix,
        "The host:port/path prefix to be used for purging requests from "
        "the downstream cache."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedEnableFilters,
        "Comma-separated list of enabled filters"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedForbidFilters,
        "Comma-separated list of forbidden filters"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedExperimentVariable,
         "Specify the custom variable slot with which to run experiments."
         "Defaults to 1."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedExperimentSpec,
         "Configuration for one side of an experiment in the form: "
         "'id= ;enabled= ;disabled= ;ga= ;percent= ...'"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedListOutstandingUrlsOnError,
        "Adds an error message into the log for every URL fetch in "
        "flight when the HTTP stack encounters a system error, e.g. "
        "Connection Refused"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedRetainComment,
        "Retain HTML comments matching wildcard, even with remove_comments "
        "enabled"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedRunExperiment,
         "Run an experiment to test the effectiveness of rewriters."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedSpeedTracking,
        "Increase the percentage of sites that have Google Analytics page "
        "speed tracking"),

  // All one parameter deprecated options.
  APACHE_CONFIG_DIR_OPTION(kModPagespeedImgInlineMaxBytes,
        "DEPRECATED, use ModPagespeedImageInlineMaxBytes."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedCollectRefererStatistics,
        "Deprecated.  Does nothing."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedHashRefererStatistics,
        "Deprecated.  Does nothing."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedRefererStatisticsOutputLevel,
        "Deprecated.  Does nothing."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedStatisticsLoggingFile,
        "Deprecated.  Does nothing."),

  // All one parameter options that can only be specified at the server level.
  // (Not in <Directory> blocks.)
  APACHE_CONFIG_OPTION(kModPagespeedDangerPermitFetchFromUnknownHosts,
        "Disable security checks that prohibit fetching from hostnames "
        "mod_pagespeed does not know about"),
  APACHE_CONFIG_OPTION(kModPagespeedFetcherTimeoutMs,
        "Set internal fetcher timeout in milliseconds"),
  APACHE_CONFIG_OPTION(kModPagespeedFetchHttps,
        "Controls direct fetching of HTTPS resources.  Value is "
        "comma-separated list of keywords: " SERF_HTTPS_KEYWORDS),
  APACHE_CONFIG_OPTION(kModPagespeedFetchProxy, "Set the fetch proxy"),
  APACHE_CONFIG_OPTION(kModPagespeedFetchWithGzip,
                       "Request http content from origin servers using gzip"),
  APACHE_CONFIG_OPTION(kModPagespeedForceCaching,
        "Ignore HTTP cache headers and TTLs"),
  APACHE_CONFIG_OPTION(kModPagespeedGeneratedFilePrefix, "No longer used."),
  APACHE_CONFIG_OPTION(kModPagespeedImgMaxRewritesAtOnce,
        "DEPRECATED, use ModPagespeedImageMaxRewritesAtOnce."),
  APACHE_CONFIG_OPTION(kModPagespeedInheritVHostConfig,
        "Inherit global configuration into VHosts."),
  APACHE_CONFIG_OPTION(kModPagespeedInstallCrashHandler,
         "Try to dump backtrace on crashes. For developer use"),
  APACHE_CONFIG_OPTION(kModPagespeedMessageBufferSize,
        "Set the size of buffer used for /mod_pagespeed_message."),
  APACHE_CONFIG_OPTION(kModPagespeedNumRewriteThreads,
        "Number of threads to use for inexpensive portions of "
        "resource-rewriting. <= 0 to auto-detect"),
  APACHE_CONFIG_OPTION(kModPagespeedNumExpensiveRewriteThreads,
        "Number of threads to use for computation-intensive portions of "
        "resource-rewriting. <= 0 to auto-detect"),
  APACHE_CONFIG_OPTION(kModPagespeedNumShards, "No longer used."),
  APACHE_CONFIG_OPTION(kModPagespeedTrackOriginalContentLength,
        "Add X-Original-Content-Length headers to rewritten resources"),
  APACHE_CONFIG_OPTION(kModPagespeedUrlPrefix, "No longer used."),
  APACHE_CONFIG_OPTION(kModPagespeedUsePerVHostStatistics,
        "If true, keep track of statistics per VHost and not just globally"),
  APACHE_CONFIG_OPTION(kModPagespeedBlockingRewriteRefererUrls,
                       "wildcard_spec for referer urls which trigger blocking "
                       "rewrites"),

  // All two parameter options that are allowed in <Directory> blocks.
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedCustomFetchHeader,
        "custom_header_name custom_header_value"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapOriginDomain,
        "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION23(kModPagespeedMapProxyDomain,
        "proxy_domain origin_domain [to_domain]"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapRewriteDomain,
        "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedShardDomain,
        "from_domain shard_domain1[,shard_domain2]*"),

  // All two parameter options that can only be specified at the server level.
  // (Not in <Directory> blocks.)
  APACHE_CONFIG_OPTION2(kModPagespeedCreateSharedMemoryMetadataCache,
        "name size_kb"),
  APACHE_CONFIG_OPTION2(kModPagespeedLoadFromFile,
        "url_prefix filename_prefix"),
  APACHE_CONFIG_OPTION2(kModPagespeedLoadFromFileMatch,
        "url_regexp filename_prefix"),
  APACHE_CONFIG_OPTION2(kModPagespeedLoadFromFileRule,
        "<Allow|Disallow> filename_prefix"),
  APACHE_CONFIG_OPTION2(kModPagespeedLoadFromFileRuleMatch,
        "<Allow|Disallow> filename_regexp"),

  // All three parameter options that are allowed in <Directory> blocks.
  APACHE_CONFIG_DIR_OPTION3(kModPagespeedUrlValuedAttribute,
        "Specify an additional url-valued attribute."),
  APACHE_CONFIG_DIR_OPTION3(kModPagespeedLibrary,
        "Specify size, md5, and canonical url for JavaScript library, "
        "separated by spaces.\n"
        "These values may be obtained by running:\n"
        "  js_minify --print_size_and_hash library.js\n"
        "Yielding an entry like:\n"
        "  ModPagespeedLibrary 105527 ltVVzzYxo0 "
        "//ajax.googleapis.com/ajax/libs/1.6.1.0/prototype.js"),
};  // Do not null terminate; we use arraysize for initialization.

// We use pool-based cleanup for ApacheConfigs.  This is 99% effective.
// There is at least one base config which is created with create_dir_config,
// but whose pool is never freed.  To allow clean valgrind reports, we
// must delete that config too.  So we keep a backup cleanup-set for
// configs at end-of-process, and keep that set up-to-date when the
// pool deletion does work.
apr_status_t delete_config(void* data) {
  ApacheConfig* config = static_cast<ApacheConfig*>(data);
  delete config;
  return APR_SUCCESS;
}

// Function to allow all modules to create per directory configuration
// structures.
// dir is the directory currently being processed.
// Returns the per-directory structure created.
void* create_dir_config(apr_pool_t* pool, char* dir) {
  if (dir == NULL) {
    return NULL;
  }
  ThreadSystem* thread_system =
      apache_process_context.factory_->thread_system();
  ApacheConfig* config = new ApacheConfig(dir, thread_system);
  config->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  apr_pool_cleanup_register(pool, config, delete_config, apr_pool_cleanup_null);
  return config;
}

// Function to allow all modules to merge the per directory configuration
// structures for two directories.
// base_conf is the directory structure created for the parent directory.
// new_conf is the directory structure currently being processed.
// This function returns the new per-directory structure created
void* merge_dir_config(apr_pool_t* pool, void* base_conf, void* new_conf) {
  ApacheConfig* dir1 = static_cast<ApacheConfig*>(base_conf);
  ApacheConfig* dir2 = static_cast<ApacheConfig*>(new_conf);

  // To make it easier to debug the merged configurations, we store
  // the name of both input configurations as the description for
  // the merged configuration.
  ApacheConfig* dir3 = new ApacheConfig(
      StrCat(
          "Combine(", dir1->description(), ", ", dir2->description(), ")"),
      dir1->thread_system());

  // Apache does not notify us when it is done adding directives to a
  // configuration, so we don't have a good opportunity to Freeze it
  // until it use used as a merge source.  We don't want to do this in
  // Merge because, for C++ cleanliness/readability, we want to let
  // Merge take a const RewrieOptions&, so we must Freeze at the call site.
  dir1->Freeze();
  dir3->Merge(*dir1);
  dir2->Freeze();
  dir3->Merge(*dir2);
  apr_pool_cleanup_register(pool, dir3, delete_config, apr_pool_cleanup_null);
  return dir3;
}

void* merge_server_config(apr_pool_t* pool, void* base_conf, void* new_conf) {
  ApacheServerContext* global_context =
      static_cast<ApacheServerContext*>(base_conf);
  ApacheServerContext* vhost_context =
      static_cast<ApacheServerContext*>(new_conf);
  if (global_context->apache_factory()->inherit_vhost_config()) {
    scoped_ptr<ApacheConfig> merged_config(global_context->config()->Clone());
    merged_config->Merge(*vhost_context->config());
    // Note that we don't need to do any special handling of cache paths here,
    // since it's all related to actually creating the directories + giving
    // permissions, so doing it at top-level is sufficient.
    vhost_context->reset_global_options(merged_config.release());

    // Merge the overlays, if any exist.
    if (global_context->has_spdy_config_overlay() ||
        vhost_context->has_spdy_config_overlay()) {
      scoped_ptr<ApacheConfig> new_spdy_overlay(
          global_context->SpdyConfigOverlay()->Clone());
      new_spdy_overlay->Merge(*vhost_context->SpdyConfigOverlay());
      vhost_context->set_spdy_config_overlay(new_spdy_overlay.release());
    }

    if (global_context->has_non_spdy_config_overlay() ||
        vhost_context->has_non_spdy_config_overlay()) {
      scoped_ptr<ApacheConfig> new_non_spdy_overlay(
          global_context->NonSpdyConfigOverlay()->Clone());
      new_non_spdy_overlay->Merge(*vhost_context->NonSpdyConfigOverlay());
      vhost_context->set_non_spdy_config_overlay(
          new_non_spdy_overlay.release());
    }
  }

  return new_conf;
}

}  // namespace

}  // namespace net_instaweb

extern "C" {
// Export our module so Apache is able to load us.
// See http://gcc.gnu.org/wiki/Visibility for more information.
#if defined(__linux)
#pragma GCC visibility push(default)
#endif

// Declare and populate the module's data structure.  The
// name of this structure ('pagespeed_module') is important - it
// must match the name of the module.  This structure is the
// only "glue" between the httpd core and the module.
module AP_MODULE_DECLARE_DATA pagespeed_module = {
  // Only one callback function is provided.  Real
  // modules will need to declare callback functions for
  // server/directory configuration, configuration merging
  // and other tasks.
  STANDARD20_MODULE_STUFF,
  net_instaweb::create_dir_config,
  net_instaweb::merge_dir_config,
  net_instaweb::mod_pagespeed_create_server_config,
  net_instaweb::merge_server_config,
  NULL,  // directives initialized via static ctor calling InstallCommands().
  net_instaweb::mod_pagespeed_register_hooks,
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"

namespace net_instaweb {

// Runs via static construction and module-load time, so that it can
// install the Apache command-table in the module-record before Apache
// initializes the module.
void ApacheProcessContext::InstallCommands() {
  // Similar to the instantiation in ApacheConfig::AddProperties(), we
  // instantiate an ApacheConfig with a null thread system as we
  // are only using it to populate a static table which must be
  // established very early when mod_pagespeed.so is dynamically loaded,
  // to build the Apache directives parse-table before Apache attempts
  // to initialize our module.
  ApacheConfig config_template(NULL);
  const RewriteOptions::OptionBaseVector& v = config_template.all_options();
  int num_cmds = arraysize(net_instaweb::mod_pagespeed_filter_cmds);

  // Allocate memory for all the rewrite_options, even though we
  // will only initialize the ones with non-null help.  We could
  // also do a 2-pass to count how many we will allocate.  +1 to
  // leave room for a NULL terminator.
  apache_cmds_ = new command_rec[num_cmds + v.size() + 1];
  memcpy(apache_cmds_, net_instaweb::mod_pagespeed_filter_cmds,
         num_cmds * sizeof(*apache_cmds_));
  command_rec* cmd = apache_cmds_ + num_cmds;
  cmd_names_.resize(v.size());

  for (int i = 0, n = v.size(); i < n; ++i) {
    RewriteOptions::OptionBase* option = v[i];

    // Skip entries with null documentation -- entries lacking doc
    // are an indication that the option is not available for MPS.
    if (option->help_text() != NULL) {
      // Store the fully-qualified option name in a string-array that
      // lasts until the module is destructed.
      StrAppend(&cmd_names_[i], "ModPagespeed",
                RewriteOptions::LookupOptionEnum(option->option_enum()));
      cmd->name = cmd_names_[i].c_str();
      cmd->func = reinterpret_cast<const char*(*)()>(ParseDirective);
      cmd->cmd_data = NULL;
      switch (option->scope()) {
        case RewriteOptions::kDirectoryScope:
          cmd->req_override = OR_ALL;
          break;
        case RewriteOptions::kProcessScope:
        case RewriteOptions::kServerScope:
          cmd->req_override = RSRC_CONF;
          break;
      }
      cmd->args_how = TAKE1;
      cmd->errmsg = option->help_text();
      ++cmd;
    }
  }
  cmd->name = NULL;
  cmd->func = 0;
  cmd->cmd_data = NULL;
  cmd->req_override = 0;
  cmd->args_how = RAW_ARGS;
  cmd->errmsg = NULL;
  pagespeed_module.cmds = apache_cmds_;
}

}  // namespace net_instaweb
