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

#include <string>

#include "apr_strings.h"
#include "apr_version.h"
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
#include "net/instaweb/public/version.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/string_util.h"

// Note: a very useful reference is this file, which demos many Apache module
// options:
//    http://svn.apache.org/repos/asf/httpd/httpd/trunk/modules/
//    examples/mod_example_hooks.c

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

// Instaweb directive names -- these must match install/common/pagespeed.conf.template.
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
const char* kModPagespeedForceCaching = "ModPagespeedForceCaching";
const char* kModPagespeedCssInlineMaxBytes = "ModPagespeedCssInlineMaxBytes";
const char* kModPagespeedImgInlineMaxBytes = "ModPagespeedImgInlineMaxBytes";
const char* kModPagespeedImgMaxRewritesAtOnce =
    "ModPagespeedImgMaxRewritesAtOnce";
const char* kModPagespeedJsInlineMaxBytes = "ModPagespeedJsInlineMaxBytes";
const char* kModPagespeedDomain = "ModPagespeedDomain";
const char* kModPagespeedMapRewriteDomain = "ModPagespeedMapRewriteDomain";
const char* kModPagespeedMapOriginDomain = "ModPagespeedMapOriginDomain";
const char* kModPagespeedFilterName = "MOD_PAGESPEED_OUTPUT_FILTER";
const char* kRepairHeadersFilterName = "MOD_PAGESPEED_REPAIR_HEADERS";
const char* kModPagespeedBeaconUrl = "ModPagespeedBeaconUrl";
const char* kModPagespeedAllow = "ModPagespeedAllow";
const char* kModPagespeedDisallow = "ModPagespeedDisallow";

// TODO(jmarantz): determine the version-number from SVN at build time.
const char kModPagespeedVersion[] = MOD_PAGESPEED_VERSION_STRING "-"
    LASTCHANGE_STRING;

enum RewriteOperation {REWRITE, FLUSH, FINISH};
enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(request_rec* request,
                                const ContentType& content_type,
                                const QueryParams& query_params) {
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

  // Pass-through mode.
  // TODO(jmarantz): strip the param from the URL.
  CharStarVector v;
  if (query_params.Lookup(kModPagespeed, &v) &&
      (v.size() == 1) && (strcasecmp(v[0], "off") == 0)) {
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
    const char* value = query_params.value(i);
    if (value == NULL) {
      // Empty; all our options require a value, so skip.  It might be a
      // perfectly legitimate query param for the underlying page.
      continue;
    }
    int64 int_val;
      // TODO(jmarantz): add js inlinine threshold, outline threshold.
    if (strcmp(name, kModPagespeedCssInlineMaxBytes) == 0) {
      if (StringToInt64(value, &int_val)) {
        options->set_css_inline_max_bytes(int_val);
        ++option_count;
      } else {
        handler->Message(kWarning, "Invalid integer value for %s: %s",
                         name, value);
        ret = false;
      }
    } else if (strcmp(name, kModPagespeedFilters) == 0) {
      // When using ModPagespeedFilters query param, only the
      // specified filters should be enabled.
      options->SetRewriteLevel(RewriteOptions::kPassThrough);
      if (options->EnableFiltersByCommaSeparatedList(value, handler)) {
        ++option_count;
      } else {
        ret = false;
      }
    }
  }
  return ret && (option_count > 0);
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
    custom_options->Merge(*options, *config_options);
    options = custom_options.get();
    use_custom_options = true;
  }

  if (!options->enabled() || (request->unparsed_uri == NULL)) {
    // TODO(jmarantz): consider adding Debug message if unparsed_uri is NULL,
    // possibly of request->the_request which was non-null in the case where
    // I found this in the debugger.
    return NULL;
  }

  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                "ModPagespeed OutputFilter called for request %s",
                request->unparsed_uri);

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
  if (!check_pagespeed_applicable(request, *content_type, query_params)) {
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
    merged_options->Merge(*options, query_options);
    custom_options.reset(merged_options);
    options = merged_options;
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

apr_status_t pagespeed_child_exit(void* data) {
  ApacheRewriteDriverFactory* factory =
      static_cast<ApacheRewriteDriverFactory*>(data);
  delete factory;
  return APR_SUCCESS;
}

void pagespeed_child_init(apr_pool_t* pool, server_rec* server) {
  // Create PageSpeed context used by instaweb rewrite-driver.  This is
  // per-process, so we initialize all the server's context by iterating the
  // server lists in server->next.
  server_rec* next_server = server;
  while (next_server) {
    ApacheRewriteDriverFactory* factory = InstawebContext::Factory(next_server);
    if (factory->statistics()) {
      factory->statistics()->InitVariables(pool, false);
    }
    next_server = next_server->next;
  }
}

int pagespeed_post_config(apr_pool_t* pool, apr_pool_t* plog, apr_pool_t* ptemp,
                          server_rec *server) {
  AprStatistics* statistics = new AprStatistics();
  RewriteDriverFactory::Initialize(statistics);
  SerfUrlAsyncFetcher::Initialize(statistics);
  statistics->InitVariables(pool, true);

  server_rec* next_server = server;
  while (next_server) {
    ApacheRewriteDriverFactory* factory = InstawebContext::Factory(next_server);
    if (factory->options()->enabled()) {
      factory->set_statistics(statistics);
      if (factory->filename_prefix().empty() ||
          factory->file_cache_path().empty()) {
        std::string buf("mod_pagespeed is enabled.  ");
        buf += "The following directives must not be NULL\n";
        buf += StrCat(kModPagespeedFileCachePath, "=");
        buf += StrCat(factory->file_cache_path(), "\n");
        buf += StrCat(kModPagespeedGeneratedFilePrefix, "=");
        buf += StrCat(factory->filename_prefix(), "\n");
        factory->message_handler()->Message(kError, "%s", buf.c_str());
        return HTTP_INTERNAL_SERVER_ERROR;
      }
      // TODO(jmarantz): spew the rewriters
    }
    next_server = next_server->next;
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
  log_message_handler::InstallLogMessageHandler(pool);

  // Use instaweb to handle generated resources.
  ap_hook_handler(instaweb_handler, NULL, NULL, -1);
  ap_register_output_filter(
      kModPagespeedFilterName, instaweb_out_filter, NULL, AP_FTYPE_RESOURCE);
  // We need our repair headers filter to run after mod_headers. The
  // mod_headers, which is the filter that is used to add the cache settings, is
  // AP_FTYPE_CONTENT_SET. Using (AP_FTYPE_CONTENT_SET + 2) to make sure that we
  // run after mod_headers.
  ap_register_output_filter(
      kRepairHeadersFilterName, repair_caching_header, NULL,
      static_cast<ap_filter_type>(AP_FTYPE_CONTENT_SET + 2));
  ap_hook_post_config(pagespeed_post_config, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(pagespeed_child_init, NULL, NULL, APR_HOOK_LAST);
  ap_hook_log_transaction(pagespeed_log_transaction, NULL, NULL, APR_HOOK_LAST);
}

void* mod_pagespeed_create_server_config(apr_pool_t* pool, server_rec* server) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
  if (factory == NULL) {
    factory = new ApacheRewriteDriverFactory(pool, server,
                                             kModPagespeedVersion);

    // To clean up the factory on process shutdown, we need to run
    // pagespeed_child_exit *before* the pool is destroyed.  If we run
    // that hook with apr_pool_cleanup_register then the pool will
    // already be destroyed, and the factory destruction will crash.
    // The proper way to fix this is with:
    //
    //   apr_pool_pre_cleanup_register(pool, factory, pagespeed_child_exit);
    //
    // However, this method was added in apr 1.3.  We can do a compile-time
    // check such as
    //
    //   #if ((APR_MAJOR_VERSION > 1) ||
    //     ((APR_MAJOR_VERSION == 1) && APR_MINOR_VERSION > 2))
    //
    // However, the Apache include files that we depend on during the
    // build process may not correspond to the Apache version into
    // which that mod_pagespeed.so will be dynamically loaded.  At
    // some point in the future, we may be able to make apr 1.3 a
    // minimum requirement for mod_pagespeed.  In the meantime, we
    // will not call the factory destructor and will instead rely on
    // the process memory clean up to do what's necessary.
    //
    // TODO(jmarantz): Start employing apr_pool_pre_cleanup_register when
    // it is generaly available
    //
    // TODO(jmarantz): Figure out how to segregate the pool-dependent
    // cleanups (e.g. apr_mutex) from the pool-independent cleanups
    // (e.g. memory allocated with new) so we can clean those up using
    // apr_pool_cleanup_register.
  }
  return factory;
}

template<class Options>
const char* ParseBoolOption(Options* options, cmd_parms* cmd,
                            void (Options::*fn)(bool val),
                            const char* arg) {
  const char* ret = NULL;
  if (strcasecmp(arg, "on") == 0) {
    (options->*fn)(true);
  } else if (strcasecmp(arg, "off") == 0) {
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

// Callback function that parses a single-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective(cmd_parms* cmd, void* data, const char* arg) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  MessageHandler* handler = factory->message_handler();
  const char* directive = cmd->directive->directive;
  const char* ret = NULL;
  ApacheConfig* config = static_cast<ApacheConfig*>(data);
  RewriteOptions* options = factory->options();
  if (!config->description().empty()) {
    options = config->options();
  }

  if (strcasecmp(directive, kModPagespeed) == 0) {
    ret = ParseBoolOption(options, cmd,
                          &RewriteOptions::set_enabled, arg);
  } else if (strcasecmp(directive, kModPagespeedUrlPrefix) == 0) {
    warn_deprecated(cmd, "Please remove it from your configuration.");
  } else if (strcasecmp(directive, kModPagespeedFetchProxy) == 0) {
    factory->set_fetcher_proxy(arg);
  } else if (strcasecmp(directive, kModPagespeedGeneratedFilePrefix) == 0) {
    if (!factory->set_filename_prefix(arg)) {
      ret = apr_pstrcat(cmd->pool, "Directory ", arg,
                        " does not exist and can't be created.", NULL);
    }
  } else if (strcasecmp(directive, kModPagespeedFileCachePath) == 0) {
    factory->set_file_cache_path(arg);
  } else if (strcasecmp(directive, kModPagespeedFileCacheSizeKb) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_size_kb, arg);
  } else if (strcasecmp(directive,
                        kModPagespeedFileCacheCleanIntervalMs) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_interval_ms,
        arg);
  } else if (strcasecmp(directive, kModPagespeedFetcherTimeoutMs) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_fetcher_time_out_ms, arg);
  } else if (strcasecmp(directive, kModPagespeedNumShards) == 0) {
    warn_deprecated(cmd, "Please remove it from your configuration.");
  } else if (strcasecmp(directive, kModPagespeedCssOutlineMinBytes) == 0) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_css_outline_min_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedJsOutlineMinBytes) == 0) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_js_outline_min_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedImgInlineMaxBytes) == 0) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_img_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedJsInlineMaxBytes) == 0) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_js_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedCssInlineMaxBytes) == 0) {
    ret = ParseInt64Option(options,
        cmd, &RewriteOptions::set_css_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedLRUCacheKbPerProcess) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_kb_per_process, arg);
  } else if (strcasecmp(directive, kModPagespeedLRUCacheByteLimit) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_byte_limit, arg);
  } else if (strcasecmp(directive, kModPagespeedImgMaxRewritesAtOnce) == 0) {
    ret = ParseIntOption(options,
        cmd, &RewriteOptions::set_img_max_rewrites_at_once, arg);
  } else if (strcasecmp(directive, kModPagespeedEnableFilters) == 0) {
    if (!options->EnableFiltersByCommaSeparatedList(arg, handler)) {
      ret = "Failed to enable some filters.";
    }
  } else if (strcasecmp(directive, kModPagespeedDisableFilters) == 0) {
    if (!options->DisableFiltersByCommaSeparatedList(arg, handler)) {
      ret = "Failed to disable some filters.";
    }
  } else if (strcasecmp(directive, kModPagespeedRewriteLevel) == 0) {
    RewriteOptions::RewriteLevel level = RewriteOptions::kPassThrough;
    if (RewriteOptions::ParseRewriteLevel(arg, &level)) {
      options->SetRewriteLevel(level);
    } else {
      ret = "Failed to parse RewriteLevel.";
    }
  } else if (strcasecmp(directive, kModPagespeedSlurpDirectory) == 0) {
    factory->set_slurp_directory(arg);
  } else if (strcasecmp(directive, kModPagespeedSlurpReadOnly) == 0) {
    ret = ParseBoolOption(static_cast<RewriteDriverFactory*>(factory),
        cmd, &ApacheRewriteDriverFactory::set_slurp_read_only, arg);
  } else if (strcasecmp(directive, kModPagespeedSlurpFlushLimit) == 0) {
    ret = ParseInt64Option(factory,
        cmd, &ApacheRewriteDriverFactory::set_slurp_flush_limit, arg);
  } else if (strcasecmp(directive, kModPagespeedForceCaching) == 0) {
    ret = ParseBoolOption(static_cast<RewriteDriverFactory*>(factory),
        cmd, &ApacheRewriteDriverFactory::set_force_caching, arg);
  } else if (strcasecmp(directive, kModPagespeedBeaconUrl) == 0) {
    options->set_beacon_url(arg);
  } else if (strcasecmp(directive, kModPagespeedDomain) == 0) {
    options->domain_lawyer()->AddDomain(arg, factory->message_handler());
  } else if (strcasecmp(directive, kModPagespeedAllow) == 0) {
    options->Allow(arg);
  } else if (strcasecmp(directive, kModPagespeedDisallow) == 0) {
    options->Disallow(arg);
  } else {
    return "Unknown directive.";
  }

  return ret;
}

// Callback function that parses a two-argument directive.  This is called
// by the Apache config parser.
static const char* ParseDirective2(cmd_parms* cmd, void* data,
                                   const char* arg1, const char* arg2) {
  // TODO(jmarantz): support domain lawyer for directory-specific config
  // options.
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  RewriteOptions* options = factory->options();
  const char* directive = cmd->directive->directive;
  const char* ret = NULL;
  if (strcasecmp(directive, kModPagespeedMapRewriteDomain) == 0) {
    options->domain_lawyer()->AddRewriteDomainMapping(
        arg1, arg2, factory->message_handler());
  } else if (strcasecmp(directive, kModPagespeedMapOriginDomain) == 0) {
    options->domain_lawyer()->AddOriginDomainMapping(
        arg1, arg2, factory->message_handler());
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
  APACHE_CONFIG_DIR_OPTION(kModPagespeedJsInlineMaxBytes,
        "Number of bytes below which javascript will be inlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedCssInlineMaxBytes,
        "Number of bytes below which stylesheets will be inlined."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedBeaconUrl, "URL for beacon callback"
                       " injected by add_instrumentation."),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDomain,
        "Authorize mod_pagespeed to rewrite resources in a domain."),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapRewriteDomain,
         "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION2(kModPagespeedMapOriginDomain,
         "to_domain from_domain[,from_domain]*"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedAllow,
        "wildcard_spec for urls"),
  APACHE_CONFIG_DIR_OPTION(kModPagespeedDisallow,
        "wildcard_spec for urls"),
  {NULL}
};

// Function to allow all modules to create per directory configuration
// structures.
// dir is the directory currently being processed.
// Returns the per-directory structure created.
void* create_dir_config(apr_pool_t* pool, char* dir) {
  ApacheConfig* config = new ApacheConfig(pool, dir);
  config->options()->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
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
  ApacheConfig* dir3 = new ApacheConfig(pool, StrCat(
      "Combine(", dir1->description(), ", ", dir2->description(), ")"));
  dir3->options()->Merge(*(dir1->options()), *(dir2->options()));
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
