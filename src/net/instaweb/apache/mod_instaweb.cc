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

#include <set>
#include <string>

#include "apr_strings.h"
#include "apr_timer.h"
#include "apr_version.h"
#include "http_request.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/log_message_handler.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/instaweb_handler.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/apr_statistics.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string_util.h"

// Note: a very useful reference is this file, which demos many Apache module
// options:
//    http://svn.apache.org/repos/asf/httpd/httpd/trunk/modules/examples/mod_example_hooks.c

// The httpd header must be after the pagepseed_server_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
// When HAVE_SYSLOG, syslog.h is included and #defined LOG_*, which conflicts
// with log_message_handler.
#undef HAVE_SYSLOG
#include "http_log.h"
#include "http_protocol.h"
#if USE_FIXUP_HOOK
#include "http_request.h"  // NOLINT
#endif

extern "C" {
  extern module AP_MODULE_DECLARE_DATA pagespeed_module;
}

namespace net_instaweb {

namespace {

// Instaweb directive names -- these must match
// install/common/pagespeed.conf.template.
const char* kModPagespeed = "ModPagespeed";
const char* kModPagespeedUrlPrefix = "ModPagespeedUrlPrefix";
const char* kModPagespeedFetchProxy = "ModPagespeedFetchProxy";
const char* kModPagespeedGeneratedFilePrefix =
    "ModPagespeedGeneratedFilePrefix";
const char* kModPagespeedFileCachePath = "ModPagespeedFileCachePath";
const char* kModPagespeedFileCacheSizeKb = "ModPagespeedFileCacheSizeKb";
const char* kModPagespeedFileCacheCleanIntervalMs
    = "ModPagespeedFileCacheCleanIntervalMs";
const char* kModPagespeedLRUCacheKbPerProcess =
    "ModPagespeedLRUCacheKbPerProcess";
const char* kModPagespeedLRUCacheByteLimit = "ModPagespeedLRUCacheByteLimit";
const char* kModPagespeedFetcherTimeoutMs = "ModPagespeedFetcherTimeOutMs";
const char* kModPagespeedNumShards = "ModPagespeedNumShards";
const char* kModPagespeedCssOutlineMinBytes = "ModPagespeedCssOutlineMinBytes";
const char* kModPagespeedJsOutlineMinBytes = "ModPagespeedJsOutlineMinBytes";
const char* kModPagespeedFilters = "ModPagespeedFilters";
const char* kModPagespeedRewriteLevel = "ModPagespeedRewriteLevel";
const char* kModPagespeedEnableFilters = "ModPagespeedEnableFilters";
const char* kModPagespeedDisableFilters = "ModPagespeedDisableFilters";
const char* kModPagespeedSlurpDirectory = "ModPagespeedSlurpDirectory";
const char* kModPagespeedSlurpReadOnly = "ModPagespeedSlurpReadOnly";
const char* kModPagespeedSlurpFlushLimit = "ModPagespeedSlurpFlushLimit";
const char* kModPagespeedTestProxy = "ModPagespeedTestProxy";
const char* kModPagespeedForceCaching = "ModPagespeedForceCaching";
const char* kModPagespeedCssInlineMaxBytes = "ModPagespeedCssInlineMaxBytes";
const char* kModPagespeedImgInlineMaxBytes = "ModPagespeedImgInlineMaxBytes";
const char* kModPagespeedImgMaxRewritesAtOnce =
    "ModPagespeedImgMaxRewritesAtOnce";
const char* kModPagespeedJsInlineMaxBytes = "ModPagespeedJsInlineMaxBytes";
const char* kModPagespeedMaxSegmentLength = "ModPagespeedMaxSegmentLength";
const char* kModPagespeedLogRewriteTiming = "ModPagespeedLogRewriteTiming";
const char* kModPagespeedDomain = "ModPagespeedDomain";
const char* kModPagespeedMapRewriteDomain = "ModPagespeedMapRewriteDomain";
const char* kModPagespeedMapOriginDomain = "ModPagespeedMapOriginDomain";
const char* kModPagespeedFilterName = "MOD_PAGESPEED_OUTPUT_FILTER";
const char* kModPagespeedBeaconUrl = "ModPagespeedBeaconUrl";
const char* kModPagespeedAllow = "ModPagespeedAllow";
const char* kModPagespeedDisallow = "ModPagespeedDisallow";
const char* kModPagespeedStatistics = "ModPagespeedStatistics";
const char* kModPagespeedCombineAcrossPaths = "ModPagespeedCombineAcrossPaths";
const char* kModPagespeedLowercaseHtmlNames = "ModPagespeedLowercaseHtmlNames";
const char* kModPagespeedShardDomain = "ModPagespeedShardDomain";

// TODO(jmarantz): determine the version-number from SVN at build time.
const char kModPagespeedVersion[] = MOD_PAGESPEED_VERSION_STRING "-"
    LASTCHANGE_STRING;

enum RewriteOperation {REWRITE, FLUSH, FINISH};
enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(request_rec* request,
                                const ContentType& content_type) {
  // We can't operate on Content-Ranges.
  if (apr_table_get(request->headers_out, "Content-Range") != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Content-Range is not available");
    return false;
  }

  // Only rewrite HTML-like content.
  if (!content_type.IsHtmlLike()) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Content-Type=%s Host=%s Uri=%s",
                  request->content_type, request->hostname,
                  request->unparsed_uri);
    return false;
  }

  // mod_pagespeed often creates requests while rewriting an HTML.  These
  // requests are only intended to fetch resources (images, css, javascript) but
  // in some circumstances they can end up fetching HTML.  This HTML, if
  // rewrittten, could in turn spawn more requests which could cascade into a
  // bad situation.  To mod_pagespeed, any fetched HTML is an error condition,
  // so there's no reason to rewrite it anyway.
  const char* user_agent = apr_table_get(request->headers_in,
                                         HttpAttributes::kUserAgent);
  // TODO(abliss): unify this string literal with the one in
  // serf_url_async_fetcher.cc
  if ((user_agent != NULL) && strstr(user_agent, "mod_pagespeed")) {
    ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                  "Not rewriting mod_pagespeed's own fetch");
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
      context->Rewrite(buf, len);
  }
  if (operation == REWRITE) {
    return NULL;
  } else if (operation == FLUSH) {
    context->Flush();
  } else if (operation == FINISH) {
    context->Finish();
  }

  const std::string& output = context->output();
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

// To support query-specific rewriter sets, scan the query parameters to
// see whether we have any options we want to set.  We will only allow
// a limited number of options to be set.  In particular, some options are
// risky to set per query, such as image inline threshold, which exposes
// a DOS vulnerability and a risk of poisoning our internal cache.  Domain
// adjustments can potentially introduce a security vulnerability.
//
// So we will check for explicit parameters we want to support.
bool ScanQueryParamsForRewriterOptions(RewriteDriverFactory* factory,
                                       const QueryParams& query_params,
                                       RewriteOptions* options) {
  MessageHandler* handler = factory->message_handler();
  bool ret = true;
  int option_count = 0;
  for (int i = 0; i < query_params.size(); ++i) {
    const char* name = query_params.name(i);
    const std::string* value = query_params.value(i);
    if (value == NULL) {
      // Empty; all our options require a value, so skip.  It might be a
      // perfectly legitimate query param for the underlying page.
      continue;
    }
    int64 int_val;
    if (strcmp(name, kModPagespeed) == 0) {
      bool is_on = (value->compare("on") == 0);
      if (is_on || (value->compare("off") == 0)) {
        options->set_enabled(is_on);
        ++option_count;
      } else {
        // TODO(sligocki): Return 404s instead of logging server errors here
        // and below.
        handler->Message(kWarning, "Invalid value for %s: %s "
                         "(should be on or off)", name, value->c_str());
        ret = false;
      }
    } else if (strcmp(name, kModPagespeedFilters) == 0) {
      // When using ModPagespeedFilters query param, only the
      // specified filters should be enabled.
      options->SetRewriteLevel(RewriteOptions::kPassThrough);
      if (options->EnableFiltersByCommaSeparatedList(*value, handler)) {
        options->DisableAllFiltersNotExplicitlyEnabled();
        ++option_count;
      } else {
        handler->Message(kWarning,
                         "Invalid filter name in %s: %s", name, value->c_str());
        ret = false;
      }
    // TODO(jmarantz): add js inlinine threshold, outline threshold.
    } else if (strcmp(name, kModPagespeedCssInlineMaxBytes) == 0) {
      if (StringToInt64(*value, &int_val)) {
        options->set_css_inline_max_bytes(int_val);
        ++option_count;
      } else {
        handler->Message(kWarning, "Invalid integer value for %s: %s",
                         name, value->c_str());
        ret = false;
      }
    }
  }
  return ret && (option_count > 0);
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
  ApacheProcessContext()
      : merge_time_us_(NULL),
        parse_time_us_(NULL),
        html_rewrite_time_us_(NULL),
        stored_parse_time_us_(0) {
  }

  ~ApacheProcessContext() {
    STLDeleteElements(&factories_);
    STLDeleteElements(&configs_);
    statistics_.reset(NULL);
    ApacheRewriteDriverFactory::Terminate();
    log_message_handler::ShutDown();
  }

  // Delete the specified factory on process exit.
  void add_factory(ApacheRewriteDriverFactory* factory) {
    factories_.insert(factory);
  }

  // Do not delete the specified factory on process exit -- it
  // is being deleted on a pool hook.
  void remove_factory(ApacheRewriteDriverFactory* factory) {
    factories_.erase(factory);
  }

  // Delete the specified config on process exit.
  void add_config(ApacheConfig* config) {
    configs_.insert(config);
  }

  // Do not delete the specified config on process exit -- it
  // is being deleted on a pool hook.
  void remove_config(ApacheConfig* config) {
    configs_.erase(config);
  }

  AprStatistics* InitStatistics(const StringPiece& filename_prefix) {
    if (statistics_.get() == NULL) {
      statistics_.reset(new AprStatistics(filename_prefix));
      RewriteDriverFactory::Initialize(statistics_.get());
      SerfUrlAsyncFetcher::Initialize(statistics_.get());
      statistics_->AddVariable("merge_time_us");
      statistics_->AddVariable("parse_time_us");
      statistics_->AddVariable("html_rewrite_time_us");
      statistics_->InitVariables(true);
      merge_time_us_ = statistics_->GetVariable("merge_time_us");
      parse_time_us_ = statistics_->GetVariable("parse_time_us");
      html_rewrite_time_us_ = statistics_->GetVariable("html_rewrite_time_us");
      parse_time_us_->Add(stored_parse_time_us_);
      stored_parse_time_us_ = 0;
      statistics_->InitVariables(true);
    }
    return statistics_.get();
  }

  void AddMergeTimeUs(int64 merge_time_us) {
    if (statistics_.get() != NULL) {
      merge_time_us_->Add(merge_time_us);
    }
  }

  void AddHtmlRewriteTimeUs(int64 rewrite_time_us) {
    if (statistics_.get() != NULL) {
      html_rewrite_time_us_->Add(rewrite_time_us);
    }
  }

  // Accumulating the time spent parsing directives requires special handling,
  // because the parsing of directives precedes the initialization of the
  // statistics object, which cannot be created until the file_prefix setting
  // is parsed.
  //
  // Thus we need a place to store the accumulated parsing time, so we
  // store it hera in the ApacheProcessContext, which gets statically
  // initialized.
  void AddParseTimeUs(int64 parse_time_us) {
    if (parse_time_us_ != NULL) {
      parse_time_us_->Add(parse_time_us);
    } else {
      stored_parse_time_us_ += parse_time_us;
    }
  }

  std::set<ApacheRewriteDriverFactory*> factories_;
  std::set<ApacheConfig*> configs_;
  scoped_ptr<AprStatistics> statistics_;
  Variable* merge_time_us_;
  Variable* parse_time_us_;
  Variable* html_rewrite_time_us_;
  int64 stored_parse_time_us_;
};
ApacheProcessContext apache_process_context;

typedef void (ApacheProcessContext::*AddTimeFn)(int64 delta);

class ScopedTimer {
 public:
  explicit ScopedTimer(AddTimeFn add_time_fn)
      : add_time_fn_(add_time_fn),
        start_time_us_(timer_.NowUs()) {
  }

  ~ScopedTimer() {
    int64 delta_us = timer_.NowUs() - start_time_us_;
    (apache_process_context.*add_time_fn_)(delta_us);
  }

 private:
  AddTimeFn add_time_fn_;
  AprTimer timer_;
  int64 start_time_us_;
};

void MergeOptions(const RewriteOptions& a, const RewriteOptions& b,
                  RewriteOptions* out) {
  ScopedTimer timer(&ApacheProcessContext::AddMergeTimeUs);
  out->Merge(a, b);
}

// Builds a new context for an HTTP request, returning NULL if we decide
// that we should not handle the request.
InstawebContext* build_context_for_request(request_rec* request) {
  ApacheConfig* config = static_cast<ApacheConfig*>
      ap_get_module_config(request->per_dir_config, &pagespeed_module);
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(
      request->server);
  scoped_ptr<RewriteOptions> custom_options;
  const RewriteOptions* options = factory->options();
  const RewriteOptions* config_options = config->options();
  bool use_custom_options = false;

  if (config_options->modified()) {
    custom_options.reset(new RewriteOptions);
    MergeOptions(*options, *config_options, custom_options.get());
    options = custom_options.get();
    use_custom_options = true;
  }

  if (request->unparsed_uri == NULL) {
    // TODO(jmarantz): consider adding Debug message if unparsed_uri is NULL,
    // possibly of request->the_request which was non-null in the case where
    // I found this in the debugger.
    return NULL;
  }

  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                "ModPagespeed OutputFilter called for request %s",
                request->unparsed_uri);

  // TODO(sligocki): Should we rewrite any other statuses?
  // Maybe 206 Partial Content?
  if (request->status != 200) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "ModPagespeed not rewriting HTML because status is %d",
                  request->status);
    return NULL;
  }

  QueryParams query_params;
  if (request->parsed_uri.query != NULL) {
    query_params.Parse(request->parsed_uri.query);
  }

  const ContentType* content_type =
      MimeTypeToContentType(request->content_type);
  if (content_type == NULL) {
    return NULL;
  }

  // Check if pagespeed optimization is applicable.
  if (!check_pagespeed_applicable(request, *content_type)) {
    return NULL;
  }

  // Check if mod_instaweb has already rewritten the HTML.  If the server is
  // setup as both the original and the proxy server, mod_pagespeed filter may
  // be applied twice. To avoid this, skip the content if it is already
  // optimized by mod_pagespeed.
  if (apr_table_get(request->headers_out, kModPagespeedHeader) != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "URL %s already has been processed by mod_pagespeed",
                  request->unparsed_uri);
    return NULL;
  }

  // Determine the absolute URL for this request, which might take on different
  // forms in the request structure depending on whether this request comes
  // from a browser proxy, or whether mod_proxy is enabled.
  std::string absolute_url;
  if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
    absolute_url = request->unparsed_uri;
  } else {
    absolute_url = ap_construct_url(request->pool, request->unparsed_uri,
                                    request);
  }
  if ((request->filename != NULL) &&
      (strncmp(request->filename, "proxy:", 6) == 0)) {
    absolute_url.assign(request->filename + 6, strlen(request->filename) - 6);
  }

  RewriteOptions query_options;
  query_options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  if (ScanQueryParamsForRewriterOptions(
          factory, query_params, &query_options)) {
    use_custom_options = true;
    RewriteOptions* merged_options = new RewriteOptions;
    MergeOptions(*options, query_options, merged_options);
    custom_options.reset(merged_options);
    options = merged_options;
  }

  // Is ModPagespeed turned off? We check after parsing query params so that
  // they can override .conf settings.
  if (!options->enabled()) {
    return NULL;
  }

  // Do ModPagespeedDisallow restrict us from rewriting this URL?
  if (!options->IsAllowed(absolute_url)) {
    return NULL;
  }

  InstawebContext* context = new InstawebContext(
      request, *content_type, factory, absolute_url,
      use_custom_options, *options);

  InstawebContext::ContentEncoding encoding =
      context->content_encoding();
  if ((encoding == InstawebContext::kGzip) ||
      (encoding == InstawebContext::kDeflate)) {
    // Unset the content encoding because the InstawebContext will decode the
    // content before parsing.
    apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);
    apr_table_unset(request->err_headers_out, HttpAttributes::kContentEncoding);
  } else if (encoding == InstawebContext::kOther) {
    // We don't know the encoding, so we cannot rewrite the HTML.
    return NULL;
  }

  apr_table_setn(request->headers_out, kModPagespeedHeader,
                 kModPagespeedVersion);

  // Turn off caching for the HTTP requests, and remove any filters
  // that might run downstream of us and mess up our caching headers.
  apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                HttpAttributes::kNoCache);
  apr_table_unset(request->headers_out, HttpAttributes::kExpires);
  apr_table_unset(request->headers_out, HttpAttributes::kEtag);
  apr_table_unset(request->headers_out, HttpAttributes::kLastModified);
  DisableDownstreamHeaderFilters(request);

  apr_table_unset(request->headers_out, HttpAttributes::kContentLength);
  apr_table_unset(request->headers_out, "Content-MD5");
  apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);

  // Make sure compression is enabled for this response.
  ap_add_output_filter("DEFLATE", NULL, request, request->connection);
  return context;
}

// This returns 'false' if the output filter should stop its loop over
// the brigade and return an error.
bool process_bucket(ap_filter_t *filter, request_rec* request,
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

apr_status_t instaweb_out_filter(ap_filter_t *filter, apr_bucket_brigade *bb) {
  ScopedTimer timer(&ApacheProcessContext::AddHtmlRewriteTimeUs);

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

void pagespeed_child_init(apr_pool_t* pool, server_rec* server) {
  // Create PageSpeed context used by instaweb rewrite-driver.  This is
  // per-process, so we initialize all the server's context by iterating the
  // server lists in server->next.
  server_rec* next_server = server;
  while (next_server) {
    ApacheRewriteDriverFactory* factory = InstawebContext::Factory(next_server);
    if (factory->statistics()) {
      factory->statistics()->InitVariables(false);
    }
    next_server = next_server->next;
  }
}

int pagespeed_post_config(apr_pool_t* pool, apr_pool_t* plog, apr_pool_t* ptemp,
                          server_rec *server_list) {
  AprStatistics* statistics = NULL;

  // This routine is complicated by the fact that statistics use inter-process
  // mutexes and have static data, which co-mingles poorly with this otherwise
  // re-entrant module.  The situation that gets interesting is when there are
  // multiple VirtualHosts, some of which have statistics enabled and some of
  // which don't.  We don't want the behavior to be order-dependent so we
  // do multiple passes.
  //
  // TODO(jmarantz): test VirtualHost

  // In the first pass, we see whether any of the servers have
  // statistics enabled, if found, do the static initialization of
  // statistics to establish global memory segments.
  for (server_rec* server = server_list; server != NULL;
       server = server->next) {
    ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
    if (factory->options()->enabled()) {
      if (factory->filename_prefix().empty() ||
          factory->file_cache_path().empty()) {
        std::string buf = StrCat(
            "mod_pagespeed is enabled.  "
            "The following directives must not be NULL\n",
            kModPagespeedFileCachePath, "=",
            StrCat(
                factory->file_cache_path(), "\n",
                kModPagespeedGeneratedFilePrefix, "=",
                factory->filename_prefix(), "\n"));
        factory->message_handler()->Message(kError, "%s", buf.c_str());
        return HTTP_INTERNAL_SERVER_ERROR;
      }
      if ((factory->statistics_enabled() && (statistics == NULL))) {
        statistics = apache_process_context.InitStatistics(
            factory->filename_prefix());
      }
    }
  }

  // Next we do the instance-independent static initialization, once we have
  // established whether *any* of the servers of stats enabled.
  RewriteDriverFactory::Initialize(statistics);
  SerfUrlAsyncFetcher::Initialize(statistics);

  // Do a final pass over the servers and init the server-specific statistics.
  for (server_rec* server = server_list; server != NULL;
       server = server->next) {
    ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
    factory->set_statistics(factory->statistics_enabled() ? statistics : NULL);
  }
  return OK;
}

// Here log transaction will wait for all the asynchronous resource fetchers to
// finish.
apr_status_t pagespeed_log_transaction(request_rec *request) {
  server_rec* server = request->server;
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
  if (factory == NULL) {
    return DECLINED;
  }
  return DECLINED;
}

// This function is a callback and it declares what
// other functions should be called for request
// processing and configuration requests. This
// callback function declares the Handlers for
// other events.
void mod_pagespeed_register_hooks(apr_pool_t *pool) {
  // Enable logging using pagespeed style
  log_message_handler::Install(pool);

  // Use instaweb to handle generated resources.
  ap_hook_handler(instaweb_handler, NULL, NULL, APR_HOOK_FIRST - 1);
  ap_register_output_filter(
      kModPagespeedFilterName, instaweb_out_filter, NULL, AP_FTYPE_RESOURCE);
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
  ap_hook_translate_name(save_url_for_instaweb_handler, NULL, NULL,
                         APR_HOOK_FIRST - 2);

  // By default, apache imposes limitations on URL segments of around
  // 256 characters that appear to correspond to filename limitations.
  // To prevent that, we hook map_to_storage for our own purposes.
  ap_hook_map_to_storage(instaweb_map_to_storage, NULL, NULL,
                         APR_HOOK_FIRST - 2);
}

apr_status_t pagespeed_child_exit(void* data) {
  ApacheRewriteDriverFactory* factory =
      static_cast<ApacheRewriteDriverFactory*>(data);
  // avoid double-destructing from the cleanup handler on process exit
  apache_process_context.remove_factory(factory);
  delete factory;
  return APR_SUCCESS;
}

void* mod_pagespeed_create_server_config(apr_pool_t* pool, server_rec* server) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
  if (factory == NULL) {
    factory = new ApacheRewriteDriverFactory(server, kModPagespeedVersion);

    apr_pool_cleanup_register(pool, factory, pagespeed_child_exit,
                              apr_pool_cleanup_null);

    // The pool-based cleanup hooks do not appear to be effective when exiting
    // the process.  The pagespeed_child_exit will *not* be called when the
    // apache process is shut down.  However, the static
    // apache_process_context's destructor will be.
    //
    // This approach is needed to clean up our memory so that valgrind can
    // report real memory leaks.
    apache_process_context.add_factory(factory);
  }
  return factory;
}

template<class Options>
const char* ParseBoolOption(Options* options, cmd_parms* cmd,
                            void (Options::*fn)(bool val),
                            const char* arg) {
  const char* ret = NULL;
  if (StringCaseEqual(arg, "on")) {
    (options->*fn)(true);
  } else if (StringCaseEqual(arg, "off")) {
    (options->*fn)(false);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive, " on|off", NULL);
  }
  return ret;
}

template<class Options>
const char* ParseInt64Option(Options* options, cmd_parms* cmd,
                             void (Options::*fn)(int64 val),
                             const char* arg) {
  int64 val;
  const char* ret = NULL;
  if (StringToInt64(arg, &val)) {
    (options->*fn)(val);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive,
                      " must specify a 64-bit integer", NULL);
  }
  return ret;
}

template<class Options>
const char* ParseIntOption(Options* options, cmd_parms* cmd,
                           void (Options::*fn)(int val),
                           const char* arg) {
  int val;
  const char* ret = NULL;
  if (StringToInt(arg, &val)) {
    (options->*fn)(val);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive,
                      " must specify a 32-bit integer", NULL);
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
// we will write the information into the factory's RewriteOptions.  However,
// if this was parsed from a Directory scope or .htaccess file then we will
// be using the RewriteOptions structure from a tree of ApacheConfig objects
// that is built up per-request.
static RewriteOptions* CmdOptions(cmd_parms* cmd, void* data) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  ApacheConfig* config = static_cast<ApacheConfig*>(data);
  RewriteOptions* options = factory->options();
  if (!config->description().empty()) {
    options = config->options();
  }
  return options;
}

// Callback function that parses a single-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective(cmd_parms* cmd, void* data, const char* arg) {
  ScopedTimer timer(&ApacheProcessContext::AddParseTimeUs);
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  MessageHandler* handler = factory->message_handler();
  const char* directive = cmd->directive->directive;
  const char* ret = NULL;
  RewriteOptions* options = CmdOptions(cmd, data);

  if (StringCaseEqual(directive, kModPagespeed)) {
    ret = ParseBoolOption(options, cmd,
                          &RewriteOptions::set_enabled, arg);
  } else if (StringCaseEqual(directive, kModPagespeedCombineAcrossPaths)) {
    ret = ParseBoolOption(options, cmd,
                          &RewriteOptions::set_combine_across_paths, arg);
  } else if (StringCaseEqual(directive, kModPagespeedLowercaseHtmlNames)) {
    ret = ParseBoolOption(options, cmd,
                          &RewriteOptions::set_lowercase_html_names, arg);
  } else if (StringCaseEqual(directive, kModPagespeedUrlPrefix)) {
    warn_deprecated(cmd, "Please remove it from your configuration.");
  } else if (StringCaseEqual(directive, kModPagespeedFetchProxy)) {
    factory->set_fetcher_proxy(arg);
  } else if (StringCaseEqual(directive, kModPagespeedGeneratedFilePrefix)) {
    if (!factory->set_filename_prefix(arg)) {
      ret = apr_pstrcat(cmd->pool, "Directory ", arg,
                        " does not exist and can't be created.", NULL);
    }
  } else if (StringCaseEqual(directive, kModPagespeedFileCachePath)) {
    factory->set_file_cache_path(arg);
  } else if (StringCaseEqual(directive, kModPagespeedFileCacheSizeKb)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_size_kb, arg);
  } else if (StringCaseEqual(directive,
                        kModPagespeedFileCacheCleanIntervalMs)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_interval_ms,
        arg);
  } else if (StringCaseEqual(directive, kModPagespeedFetcherTimeoutMs)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_fetcher_time_out_ms, arg);
  } else if (StringCaseEqual(directive, kModPagespeedNumShards)) {
    warn_deprecated(cmd, "Please remove it from your configuration.");
  } else if (StringCaseEqual(directive, kModPagespeedCssOutlineMinBytes)) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_css_outline_min_bytes, arg);
  } else if (StringCaseEqual(directive, kModPagespeedJsOutlineMinBytes)) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_js_outline_min_bytes, arg);
  } else if (StringCaseEqual(directive, kModPagespeedImgInlineMaxBytes)) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_img_inline_max_bytes, arg);
  } else if (StringCaseEqual(directive, kModPagespeedJsInlineMaxBytes)) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_js_inline_max_bytes, arg);
  } else if (StringCaseEqual(directive, kModPagespeedCssInlineMaxBytes)) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_css_inline_max_bytes, arg);
  } else if (StringCaseEqual(directive, kModPagespeedMaxSegmentLength)) {
    // TODO(sligocki): Convert to ParseInt64Option for consistency?
    ret = ParseIntOption(options,
        cmd, &RewriteOptions::set_max_url_segment_size, arg);
  } else if (StringCaseEqual(directive, kModPagespeedLRUCacheKbPerProcess)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_kb_per_process, arg);
  } else if (StringCaseEqual(directive, kModPagespeedLRUCacheByteLimit)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_byte_limit, arg);
  } else if (StringCaseEqual(directive, kModPagespeedImgMaxRewritesAtOnce)) {
    // TODO(sligocki): Convert to ParseInt64Option for consistency?
    ret = ParseIntOption(options,
        cmd, &RewriteOptions::set_img_max_rewrites_at_once, arg);
  } else if (StringCaseEqual(directive, kModPagespeedLogRewriteTiming)) {
    ret = ParseBoolOption(
        options, cmd, &RewriteOptions::set_log_rewrite_timing, arg);
  } else if (StringCaseEqual(directive, kModPagespeedEnableFilters)) {
    if (!options->EnableFiltersByCommaSeparatedList(arg, handler)) {
      ret = "Failed to enable some filters.";
    }
  } else if (StringCaseEqual(directive, kModPagespeedDisableFilters)) {
    if (!options->DisableFiltersByCommaSeparatedList(arg, handler)) {
      ret = "Failed to disable some filters.";
    }
  } else if (StringCaseEqual(directive, kModPagespeedRewriteLevel)) {
    RewriteOptions::RewriteLevel level = RewriteOptions::kPassThrough;
    if (RewriteOptions::ParseRewriteLevel(arg, &level)) {
      options->SetRewriteLevel(level);
    } else {
      ret = "Failed to parse RewriteLevel.";
    }
  } else if (StringCaseEqual(directive, kModPagespeedSlurpDirectory)) {
    factory->set_slurp_directory(arg);
  } else if (StringCaseEqual(directive, kModPagespeedSlurpReadOnly)) {
    ret = ParseBoolOption(static_cast<RewriteDriverFactory*>(factory),
        cmd, &ApacheRewriteDriverFactory::set_slurp_read_only, arg);
  } else if (StringCaseEqual(directive, kModPagespeedSlurpFlushLimit)) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_slurp_flush_limit, arg);
  } else if (StringCaseEqual(directive, kModPagespeedTestProxy)) {
    ret = ParseBoolOption(factory,
        cmd, &ApacheRewriteDriverFactory::set_test_proxy, arg);
  } else if (StringCaseEqual(directive, kModPagespeedForceCaching)) {
    ret = ParseBoolOption(static_cast<RewriteDriverFactory*>(factory),
        cmd, &ApacheRewriteDriverFactory::set_force_caching, arg);
  } else if (StringCaseEqual(directive, kModPagespeedBeaconUrl)) {
    options->set_beacon_url(arg);
  } else if (StringCaseEqual(directive, kModPagespeedDomain)) {
    options->domain_lawyer()->AddDomain(arg, factory->message_handler());
  } else if (StringCaseEqual(directive, kModPagespeedAllow)) {
    options->Allow(arg);
  } else if (StringCaseEqual(directive, kModPagespeedDisallow)) {
    options->Disallow(arg);
  } else if (StringCaseEqual(directive, kModPagespeedStatistics)) {
    ret = ParseBoolOption(factory, cmd,
        &ApacheRewriteDriverFactory::set_statistics_enabled, arg);
  } else {
    return "Unknown directive.";
  }

  return ret;
}

// Callback function that parses a two-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective2(cmd_parms* cmd, void* data,
                                   const char* arg1, const char* arg2) {
  ScopedTimer timer(&ApacheProcessContext::AddParseTimeUs);
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  RewriteOptions* options = CmdOptions(cmd, data);
  const char* directive = cmd->directive->directive;
  const char* ret = NULL;
  if (StringCaseEqual(directive, kModPagespeedMapRewriteDomain)) {
    options->domain_lawyer()->AddRewriteDomainMapping(
        arg1, arg2, factory->message_handler());
  } else if (StringCaseEqual(directive, kModPagespeedMapOriginDomain)) {
    options->domain_lawyer()->AddOriginDomainMapping(
        arg1, arg2, factory->message_handler());
  } else if (StringCaseEqual(directive, kModPagespeedShardDomain)) {
    options->domain_lawyer()->AddShard(arg1, arg2, factory->message_handler());
  } else {
    return "Unknown directive.";
  }
  return ret;
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

// Like APACHE_CONFIG_OPTION, but gets 2 arguments.
#define APACHE_CONFIG_DIR_OPTION2(name, help) \
  AP_INIT_TAKE2(name, reinterpret_cast<const char*(*)()>(ParseDirective2), \
                NULL, OR_ALL, help)

static const command_rec mod_pagespeed_filter_cmds[] = {
  APACHE_CONFIG_DIR_OPTION(kModPagespeed, "Enable instaweb"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedCombineAcrossPaths,
                           "Allow combining resources from different paths"),
  APACHE_CONFIG_OPTION(kModPagespeedUrlPrefix, "Set the url prefix"),
  APACHE_CONFIG_OPTION(kModPagespeedFetchProxy, "Set the fetch proxy"),
  APACHE_CONFIG_OPTION(kModPagespeedGeneratedFilePrefix,
                       "Set generated file's prefix"),
  APACHE_CONFIG_OPTION(kModPagespeedFileCachePath,
                       "Set the path for file cache"),
  APACHE_CONFIG_OPTION(kModPagespeedFileCacheSizeKb,
        "Set the target size (in kilobytes) for file cache"),
  APACHE_CONFIG_OPTION(kModPagespeedFileCacheCleanIntervalMs,
        "Set the interval (in ms) for cleaning the file cache"),
  APACHE_CONFIG_OPTION(kModPagespeedFetcherTimeoutMs,
        "Set internal fetcher timeout in milliseconds"),
  APACHE_CONFIG_OPTION(kModPagespeedNumShards, "Set number of shards"),
  APACHE_CONFIG_OPTION(kModPagespeedLRUCacheKbPerProcess,
        "Set the total size, in KB, of the per-process "
        "in-memory LRU cache"),
  APACHE_CONFIG_OPTION(kModPagespeedLRUCacheByteLimit,
        "Set the maximum byte size entry to store in the per-process "
        "in-memory LRU cache"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedRewriteLevel,
        "Base level of rewriting (PassThrough, CoreFilters)"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedEnableFilters,
        "Comma-separated list of enabled filters"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisableFilters,
        "Comma-separated list of disabled filters"),
  APACHE_CONFIG_OPTION(kModPagespeedSlurpDirectory,
        "Directory from which to read slurped resources"),
  APACHE_CONFIG_OPTION(kModPagespeedSlurpReadOnly,
        "Only read from the slurped directory, fail to fetch "
        "URLs not already in the slurped directory"),
  APACHE_CONFIG_OPTION(kModPagespeedSlurpFlushLimit,
        "Set the maximum byte size for the slurped content to hold before "
        "a flush"),
  APACHE_CONFIG_OPTION(kModPagespeedTestProxy,
        "Act as a proxy without maintaining a slurp dump."),
  APACHE_CONFIG_OPTION(kModPagespeedForceCaching,
        "Ignore HTTP cache headers and TTLs"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedCssOutlineMinBytes,
        "Number of bytes above which inline "
        "CSS resources will be outlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedJsOutlineMinBytes,
        "Number of bytes above which inline "
        "Javascript resources will be outlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedImgInlineMaxBytes,
        "Number of bytes below which images will be inlined."),
  APACHE_CONFIG_OPTION(kModPagespeedImgMaxRewritesAtOnce,
        "Set bound on number of images being rewritten at one time "
        "(0 = unbounded)."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedJsInlineMaxBytes,
        "Number of bytes below which javascript will be inlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedCssInlineMaxBytes,
        "Number of bytes below which stylesheets will be inlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedMaxSegmentLength,
        "Maximum size of a URL segment."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedLogRewriteTiming,
        "Whether or not to report timing information about HtmlParse."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedBeaconUrl,
        "URL for beacon callback injected by add_instrumentation."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDomain,
        "Authorize mod_pagespeed to rewrite resources in a domain."),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapRewriteDomain,
         "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapOriginDomain,
         "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedShardDomain,
         "from_domain shard_domain1[,shard_domain2]*"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedAllow,
        "wildcard_spec for urls"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisallow,
        "wildcard_spec for urls"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedStatistics,
        "Whether to collect cross-process statistics."),
  {NULL}
};

// We use pool-based cleanup for ApacheConfigs.  This is 99% effective.
// There is at least one base config which is created with create_dir_config,
// but whose pool is never freed.  To allow clean valgrind reports, we
// must delete that config too.  So we keep a backup cleanup-set for
// configs at end-of-process, and keep that set up-to-date when the
// pool deletion does work.
apr_status_t delete_config(void* data) {
  ApacheConfig* config = static_cast<ApacheConfig*>(data);
  // avoid double-destructing from the cleanup handler on process exit
  apache_process_context.remove_config(config);
  delete config;
  return APR_SUCCESS;
}

// Function to allow all modules to create per directory configuration
// structures.
// dir is the directory currently being processed.
// Returns the per-directory structure created.
void* create_dir_config(apr_pool_t* pool, char* dir) {
  ApacheConfig* config = new ApacheConfig(dir);
  config->options()->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  apache_process_context.add_config(config);
  apr_pool_cleanup_register(pool, config, delete_config, apr_pool_cleanup_null);
  apache_process_context.add_config(config);
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
  ApacheConfig* dir3 = new ApacheConfig(StrCat(
      "Combine(", dir1->description(), ", ", dir2->description(), ")"));
  MergeOptions(*(dir1->options()), *(dir2->options()),  dir3->options());
  apr_pool_cleanup_register(pool, dir3, delete_config, apr_pool_cleanup_null);
  apache_process_context.add_config(dir3);
  return dir3;
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
  NULL,  // merge per-server config structures
  net_instaweb::mod_pagespeed_filter_cmds,
  net_instaweb::mod_pagespeed_register_hooks,
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"
