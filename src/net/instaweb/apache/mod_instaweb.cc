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
#include "base/string_util.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/log_message_handler.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/instaweb_handler.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/apr_statistics.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/query_params.h"

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

namespace net_instaweb {

namespace {

// Instaweb directive names -- these must match install/instaweb.conf.template.
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
const char* kModPagespeedJsInlineMaxBytes = "ModPagespeedJsInlineMaxBytes";
const char* kModPagespeedDomain = "ModPagespeedDomain";
const char* kModPagespeedFilterName = "MOD_PAGESPEED_OUTPUT_FILTER";
const char* kModPagespeedBeaconUrl = "ModPagespeedBeaconUrl";

// TODO(jmarantz): determine the version-number from SVN at build time.
const char kModPagespeedVersion[] = MOD_PAGESPEED_VERSION_STRING "-"
    LASTCHANGE_STRING;
const char kModPagespeedHeader[] = "X-Mod-Pagespeed";

enum RewriteOperation {REWRITE, FLUSH, FINISH};
enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

// Determine the resource type from a Content-Type string
bool is_html_content(const char* content_type) {
  if (content_type != NULL &&
      StartsWithASCII(content_type, "text/html", false)) {
    return true;
  }
  return false;
}

// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(ap_filter_t* filter, apr_bucket_brigade* bb,
                                const QueryParams& query_params) {
  request_rec* request = filter->r;
  // We can't operate on Content-Ranges.
  if (apr_table_get(request->headers_out, "Content-Range") != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Content-Range is not available");
    return false;
  }

  // Only rewrite text/html.
  if (!is_html_content(request->content_type)) {
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

  return true;
}

// Create a new bucket from buf using HtmlRewriter.
// TODO(lsong): the content is copied multiple times. The buf is
// copied/processed to string output, then output is copied to new bucket.
apr_bucket* rewrite_html(ap_filter_t *filter, RewriteOperation operation,
                         const char* buf, int len) {
  request_rec* request = filter->r;
  InstawebContext* context =
      static_cast<InstawebContext*>(filter->ctx);
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
      handler->Message(kWarning, "Empty value for %s", name);
      ret = false;
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

apr_status_t instaweb_out_filter(ap_filter_t *filter, apr_bucket_brigade *bb) {
  // Check if pagespeed is enabled.
  request_rec* request = filter->r;
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(
      request->server);
  if (!factory->enabled()) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  // Do nothing if there is nothing, and stop passing to other filters.
  if (APR_BRIGADE_EMPTY(bb)) {
    return APR_SUCCESS;
  }

  QueryParams query_params;
  query_params.Parse(request->parsed_uri.query);

  // Check if pagespeed optimization applicable and get the resource type.
  if (!check_pagespeed_applicable(filter, bb, query_params)) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  InstawebContext* context =
      static_cast<InstawebContext*>(filter->ctx);

  LOG(INFO) << "ModPagespeed OutputFilter called for request "
            << request->unparsed_uri;

  // Initialize per-request context structure.  Note that instaweb_out_filter
  // may get called multiple times per HTTP request, and this occurs only
  // on the first call.
  if (context == NULL) {
    // Check if mod_instaweb has already rewritten the HTML.  If the server is
    // setup as both the original and the proxy server, mod_pagespeed filter may
    // be applied twice. To avoid this, skip the content if it is already
    // optimized by mod_pagespeed.
    if (apr_table_get(request->headers_out, kModPagespeedHeader) != NULL) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                    "Already has x-mod-pagespeed");
      ap_remove_output_filter(filter);
      return ap_pass_brigade(filter->next, bb);
    }

    std::string absolute_url;
    if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
      absolute_url = request->unparsed_uri;
    } else {
      absolute_url = ap_construct_url(request->pool, request->unparsed_uri,
                                  request);
    }
    LOG(INFO) << "unparsed=" << request->unparsed_uri
              << ", absolute_url=" << absolute_url;

    RewriteOptions custom_options;
    bool use_custom_options = ScanQueryParamsForRewriterOptions(
        factory, query_params, &custom_options);
    context = new InstawebContext(request, factory, absolute_url,
                                  use_custom_options, custom_options);
    filter->ctx = context;

    InstawebContext::ContentEncoding encoding =
        context->content_encoding();
    if (encoding == InstawebContext::kGzip) {
      // Unset the content encoding because the InstawebContext will decode the
      // content before parsing.
      apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);
      apr_table_unset(request->err_headers_out,
                      HttpAttributes::kContentEncoding);
    } else if (encoding == InstawebContext::kOther) {
      // We don't know the encoding, so we cannot rewrite the HTML.
      ap_remove_output_filter(filter);
      return ap_pass_brigade(filter->next, bb);
    }

    SimpleMetaData request_headers, response_headers;
    ApacheHeaderToMetaData(request->headers_in, 0,
                           request->proto_num, &request_headers);
    LOG(INFO) << "Request headers:\n" << request_headers.ToString();

    // Hack for mod_proxy to figure out where it's proxying from
    LOG(INFO) << "request->filename=" << request->filename << ", uri="
              << request->unparsed_uri;
    if ((request->filename != NULL) &&
        (strncmp(request->filename, "proxy:", 6) == 0)) {
      absolute_url.assign(request->filename + 6, strlen(request->filename) - 6);
    }

    apr_table_setn(request->headers_out, kModPagespeedHeader,
                   kModPagespeedVersion);
    apr_table_unset(request->headers_out, HttpAttributes::kContentLength);
    apr_table_unset(request->headers_out, "Content-MD5");
    apr_table_unset(request->headers_out, HttpAttributes::kContentEncoding);

    // Note that downstream output filters may further mutate the response
    // headers, and this will not show those mutations.
    ApacheHeaderToMetaData(request->headers_out, request->status,
                           request->proto_num, &response_headers);
    LOG(INFO) << "ModPagespeed Response headers:\n"
              << response_headers.ToString();

    // Make sure compression is enabled for this response.
    ap_add_output_filter("DEFLATE", NULL, request, request->connection);
  }

  apr_bucket* new_bucket = NULL;
  apr_bucket_brigade* context_bucket_brigade = context->bucket_brigade();
  while (!APR_BRIGADE_EMPTY(bb)) {
    apr_bucket* bucket = APR_BRIGADE_FIRST(bb);
    // Remove the bucket from the old brigade. We will create new bucket or
    // reuse the bucket to insert into the new brigade.
    APR_BUCKET_REMOVE(bucket);
    if (!APR_BUCKET_IS_METADATA(bucket)) {
      const char* buf = NULL;
      size_t bytes = 0;
      apr_status_t ret_code =
          apr_bucket_read(bucket, &buf, &bytes, APR_BLOCK_READ);
      if (ret_code == APR_SUCCESS) {
        new_bucket = rewrite_html(filter, REWRITE, buf, bytes);
      } else {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, ret_code, request,
                      "Reading bucket failed (rcode=%d)", ret_code);
        apr_bucket_delete(bucket);
        return ret_code;
      }
      // Processed the bucket, now delete it.
      apr_bucket_delete(bucket);
      if (new_bucket != NULL) {
        APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
      }
    } else if (APR_BUCKET_IS_EOS(bucket)) {
      new_bucket = rewrite_html(filter, FINISH, NULL, 0);
      if (new_bucket != NULL) {
        APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
      }
      // Insert the EOS bucket to the new brigade.
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
      // OK, we have seen the EOS. Time to pass it along down the chain.
      return ap_pass_brigade(filter->next, context_bucket_brigade);
    } else if (APR_BUCKET_IS_FLUSH(bucket)) {
      new_bucket = rewrite_html(filter, FLUSH, NULL, 0);
      if (new_bucket != NULL) {
        APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, new_bucket);
      }
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
      // OK, Time to flush, pass it along down the chain.
      apr_status_t ret_code =
          ap_pass_brigade(filter->next, context_bucket_brigade);
      if (ret_code != APR_SUCCESS) {
        return ret_code;
      }
    } else {
      // TODO(lsong): remove this log.
      ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                    "Unknown meta data");
      APR_BRIGADE_INSERT_TAIL(context_bucket_brigade, bucket);
    }
  }

  apr_brigade_cleanup(bb);
  return APR_SUCCESS;
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
    if (factory->enabled()) {
      factory->set_statistics(statistics);
      if (factory->url_prefix().empty() ||
          factory->filename_prefix().empty() ||
          factory->file_cache_path().empty()) {
        LOG(ERROR) << "Page speed is enabled.  "
                   << "The following directives must not be NULL";
        LOG(ERROR) << kModPagespeedUrlPrefix << "=" << factory->url_prefix();
        LOG(ERROR) << kModPagespeedFileCachePath << "="
                   << factory->file_cache_path();
        LOG(ERROR) << kModPagespeedGeneratedFilePrefix << "="
                   << factory->filename_prefix();
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
  InstallLogMessageHandler(pool);

  // Use instaweb to handle generated resources.
  ap_hook_handler(instaweb_handler, NULL, NULL, -1);
  ap_register_output_filter(kModPagespeedFilterName,
                            instaweb_out_filter,
                            NULL,
                            AP_FTYPE_RESOURCE);
  ap_hook_post_config(pagespeed_post_config, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(pagespeed_child_init, NULL, NULL, APR_HOOK_LAST);
  ap_hook_log_transaction(pagespeed_log_transaction, NULL, NULL, APR_HOOK_LAST);
}

void* mod_pagespeed_create_server_config(apr_pool_t* pool, server_rec* server) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(server);
  if (factory == NULL) {
    factory = new ApacheRewriteDriverFactory(pool);

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

typedef void (ApacheRewriteDriverFactory::*SetBoolFn)(bool val);
typedef void (ApacheRewriteDriverFactory::*SetInt64Fn)(int64 val);
typedef void (ApacheRewriteDriverFactory::*SetIntFn)(int val);

const char* ParseBoolOption(cmd_parms* cmd, SetBoolFn fn, const char* arg) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  const char* ret = NULL;
  if (strcasecmp(arg, "on") == 0) {
    (factory->*fn)(true);
  } else if (strcasecmp(arg, "off") == 0) {
    (factory->*fn)(false);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive, " on|off", NULL);
  }
  return ret;
}

const char* ParseInt64Option(cmd_parms* cmd, SetInt64Fn fn, const char* arg) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  int64 val;
  const char* ret = NULL;
  if (StringToInt64(arg, &val)) {
    (factory->*fn)(val);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive,
                      " must specify a 64-bit integer", NULL);
  }
  return ret;
}

const char* ParseIntOption(cmd_parms* cmd, SetIntFn fn, const char* arg) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  int val;
  const char* ret = NULL;
  if (StringToInt(arg, &val)) {
    (factory->*fn)(val);
  } else {
    ret = apr_pstrcat(cmd->pool, cmd->directive->directive,
                      " must specify a 32-bit integer", NULL);
  }
  return ret;
}

static const char* ParseDirective(cmd_parms* cmd, void* data, const char* arg) {
  ApacheRewriteDriverFactory* factory = InstawebContext::Factory(cmd->server);
  const char* directive = cmd->directive->directive;
  const char* ret = NULL;
  if (strcasecmp(directive, kModPagespeed) == 0) {
    ret = ParseBoolOption(cmd, &ApacheRewriteDriverFactory::set_enabled, arg);
  } else if (strcasecmp(directive, kModPagespeedUrlPrefix) == 0) {
    factory->set_url_prefix(arg);
  } else if (strcasecmp(directive, kModPagespeedFetchProxy) == 0) {
    factory->set_fetcher_proxy(arg);
  } else if (strcasecmp(directive, kModPagespeedGeneratedFilePrefix) == 0) {
    factory->set_filename_prefix(arg);
  } else if (strcasecmp(directive, kModPagespeedFileCachePath) == 0) {
    factory->set_file_cache_path(arg);
  } else if (strcasecmp(directive, kModPagespeedFileCacheSizeKb) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_size_kb, arg);
  } else if (strcasecmp(directive,
                        kModPagespeedFileCacheCleanIntervalMs) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_file_cache_clean_interval_ms,
        arg);
  } else if (strcasecmp(directive, kModPagespeedFetcherTimeoutMs) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_fetcher_time_out_ms, arg);
  } else if (strcasecmp(directive, kModPagespeedNumShards) == 0) {
    ret = ParseIntOption(cmd, &ApacheRewriteDriverFactory::set_num_shards, arg);
  } else if (strcasecmp(directive, kModPagespeedCssOutlineMinBytes) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_css_outline_min_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedJsOutlineMinBytes) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_js_outline_min_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedImgInlineMaxBytes) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_img_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedJsInlineMaxBytes) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_js_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedCssInlineMaxBytes) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_css_inline_max_bytes, arg);
  } else if (strcasecmp(directive, kModPagespeedLRUCacheKbPerProcess) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_kb_per_process, arg);
  } else if (strcasecmp(directive, kModPagespeedLRUCacheByteLimit) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_lru_cache_byte_limit, arg);
  } else if (strcasecmp(directive, kModPagespeedEnableFilters) == 0) {
    if (!factory->AddEnabledFilters(arg)) {
      ret = "Failed to enable some filters.";
    }
  } else if (strcasecmp(directive, kModPagespeedDisableFilters) == 0) {
    if (!factory->AddDisabledFilters(arg)) {
      ret = "Failed to disable some filters.";
    }
  } else if (strcasecmp(directive, kModPagespeedRewriteLevel) == 0) {
    RewriteOptions::RewriteLevel level = RewriteOptions::kPassThrough;
    if (RewriteOptions::ParseRewriteLevel(arg, &level)) {
      factory->SetRewriteLevel(level);
    } else {
      ret = "Failed to parse RewriteLevel.";
    }
  } else if (strcasecmp(directive, kModPagespeedSlurpDirectory) == 0) {
    factory->set_slurp_directory(arg);
  } else if (strcasecmp(directive, kModPagespeedSlurpReadOnly) == 0) {
    ret = ParseBoolOption(
        cmd, &ApacheRewriteDriverFactory::set_slurp_read_only, arg);
  } else if (strcasecmp(directive, kModPagespeedSlurpFlushLimit) == 0) {
    ret = ParseInt64Option(
        cmd, &ApacheRewriteDriverFactory::set_slurp_flush_limit, arg);
  } else if (strcasecmp(directive, kModPagespeedForceCaching) == 0) {
    ret = ParseBoolOption(
        cmd, &ApacheRewriteDriverFactory::set_force_caching, arg);
  } else if (strcasecmp(directive, kModPagespeedBeaconUrl) == 0) {
      factory->set_beacon_url(arg);
  } else if (strcasecmp(directive, kModPagespeedDomain) == 0) {
    factory->domain_lawyer()->AddDomain(arg, factory->message_handler());
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
#define APACHE_CONFIG_OPTION(name, help) \
  AP_INIT_TAKE1(name, reinterpret_cast<const char*(*)()>(ParseDirective), \
                NULL, RSRC_CONF, help)

static const command_rec mod_pagespeed_filter_cmds[] = {
  APACHE_CONFIG_OPTION(kModPagespeed, "Enable instaweb"),
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
  APACHE_CONFIG_OPTION(kModPagespeedRewriteLevel,
                       "Base level of rewriting (PassThrough, CoreFilters)"),
  APACHE_CONFIG_OPTION(kModPagespeedEnableFilters,
                       "Comma-separated list of enabled filters"),
  APACHE_CONFIG_OPTION(kModPagespeedDisableFilters,
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
  APACHE_CONFIG_OPTION(kModPagespeedCssOutlineMinBytes,
        "Number of bytes above which inline "
        "CSS resources will be outlined."),
  APACHE_CONFIG_OPTION(kModPagespeedJsOutlineMinBytes,
        "Number of bytes above which inline "
        "Javascript resources will be outlined."),
  APACHE_CONFIG_OPTION(kModPagespeedImgInlineMaxBytes,
        "Number of bytes below which images will be inlined."),
  APACHE_CONFIG_OPTION(kModPagespeedJsInlineMaxBytes,
        "Number of bytes below which javascript will be inlined."),
  APACHE_CONFIG_OPTION(kModPagespeedCssInlineMaxBytes,
        "Number of bytes below which stylesheets will be inlined."),
  APACHE_CONFIG_OPTION(kModPagespeedBeaconUrl, "URL for beacon callback"
                       " injected by add_instrumentation."),
  APACHE_CONFIG_OPTION(kModPagespeedDomain,
        "Authorize mod_pagespeed to rewrite resources in a domain."),
  {NULL}
};

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
  NULL,  // create per-directory config structure
  NULL,  // merge per-directory config structures
  net_instaweb::mod_pagespeed_create_server_config,
  NULL,  // merge per-server config structures
  net_instaweb::mod_pagespeed_filter_cmds,
  net_instaweb::mod_pagespeed_register_hooks,
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"
