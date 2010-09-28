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
#include "net/instaweb/apache/html_rewriter.h"
#include "net/instaweb/apache/html_rewriter_config.h"
#include "net/instaweb/apache/log_message_handler.h"
#include "net/instaweb/apache/pagespeed_server_context.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/instaweb_handler.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
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

using html_rewriter::HtmlRewriter;

extern "C" {
extern module AP_MODULE_DECLARE_DATA instaweb_module;

// Instaweb directive names -- these must match ../scripts/instaweb.conf
const char* kInstaweb = "Instaweb";
const char* kInstawebUrlPrefix = "InstawebUrlPrefix";
const char* kInstawebFetchProxy = "InstawebFetchProxy";
const char* kInstawebGeneratedFilePrefix = "InstawebGeneratedFilePrefix";
const char* kInstawebFileCachePath = "InstawebFileCachePath";
const char* kInstawebFileCacheSizeKb = "InstawebFileCacheSizeKb";
const char* kInstawebFileCacheCleanIntervalMs
    = "InstawebFileCacheCleanIntervalMs";
const char* kInstawebLRUCacheKbPerProcess = "InstawebLRUCacheKbPerProcess";
const char* kInstawebLRUCacheByteLimit = "InstawebLRUCacheByteLimit";
const char* kInstawebFetcherTimeoutMs = "InstawebFetcherTimeOutMs";
const char* kInstawebResourceTimeoutMs = "InstawebResourceTimeOutMs";
const char* kInstawebNumShards = "InstawebNumShards";
const char* kInstawebOutlineThreshold = "InstawebOutlineThreshold";
const char* kInstawebUseHttpCache = "InstawebUseHttpCache";
const char* kInstawebRewriters = "InstawebRewriters";
const char* kInstawebSlurpDirectory = "InstawebSlurpDirectory";
const char* kInstawebSlurpWrite = "InstawebSlurpWrite";
const char* kInstawebForceCaching = "InstawebForceCaching";
}  // extern "C"

namespace {

const char* instaweb_filter_name = "INSTAWEB_OUTPUT_FILTER";

enum RewriteOperation {REWRITE, FLUSH, FINISH};
enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

// We use the following structure to keep the pagespeed module context. The
// rewriter will put the rewritten content into the output string when flushed
// or finished. We call Flush when we see the FLUSH bucket, and call Finish when
// we see the EOS bucket.
class InstawebContext {
 public:
  static apr_status_t Cleanup(void* object) {
    InstawebContext* ic = static_cast<InstawebContext*>(object);
    delete ic;
    return APR_SUCCESS;
  }
  InstawebContext(apr_pool_t* pool) {
    apr_pool_cleanup_register(pool, this, Cleanup, apr_pool_cleanup_null);
  }
  std::string output;  // content after instaweb rewritten.
  scoped_ptr<HtmlRewriter> rewriter;
  apr_bucket_brigade* bucket_brigade;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstawebContext);
};

// Determine the resource type from a Content-Type string
bool is_html_content(const char* content_type) {
  if (content_type != NULL &&
      StartsWithASCII(content_type, "text/html", false)) {
    return true;
  }
  return false;
}

// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(ap_filter_t* filter,
                                apr_bucket_brigade* bb) {
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
  if ((strstr(request->unparsed_uri, "?instaweb=0") != NULL) ||
      (strstr(request->unparsed_uri, "&instaweb=0") != NULL)) {
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
  InstawebContext* context = static_cast<InstawebContext*>(filter->ctx);
  if (context == NULL) {
    LOG(DFATAL) << "Context is null";
    return NULL;
  }
  if (buf != NULL) {
      context->rewriter->Rewrite(buf, len);
  }
  if (operation == REWRITE) {
    return NULL;
  } else if (operation == FLUSH) {
    context->rewriter->Flush();
  } else if (operation == FINISH) {
    context->rewriter->Finish();
  }

  if (context->output.size() == 0) {
    return NULL;
  }
  // Use the rewritten content. Create in heap since output will
  // be emptied for reuse.
  apr_bucket* bucket = apr_bucket_heap_create(
      context->output.data(),
      context->output.size(),
      NULL,
      request->connection->bucket_alloc);
  context->output.clear();
  return bucket;
}

html_rewriter::ContentEncoding get_content_encoding(request_rec* request) {
  // Check if the content is gzipped. Steal from mod_deflate.
  const char* encoding = apr_table_get(
      request->headers_out, "Content-Encoding");
  if (encoding) {
    const char* err_enc = apr_table_get(request->err_headers_out,
                                        "Content-Encoding");
    if (err_enc) {
      // We don't properly handle stacked encodings now.
      return html_rewriter::OTHER;
    }
  } else {
    encoding = apr_table_get(request->err_headers_out, "Content-Encoding");
  }

  if (encoding) {
    if (strcasecmp(encoding, "gzip") == 0) {
      return html_rewriter::GZIP;
    } else {
      return  html_rewriter::OTHER;
    }
  } else {
    return  html_rewriter::NONE;
  }
}

apr_status_t instaweb_out_filter(ap_filter_t *filter, apr_bucket_brigade *bb) {
  // Check if pagespeed is enabled.
  html_rewriter::PageSpeedConfig* server_config =
      html_rewriter::mod_pagespeed_get_server_config(filter->r->server);
  if (!server_config->pagespeed_enable) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  // Do nothing if there is nothing, and stop passing to other filters.
  if (APR_BRIGADE_EMPTY(bb)) {
    return APR_SUCCESS;
  }

  // Check if pagespeed optimization applicable and get the resource type.
  if (!check_pagespeed_applicable(filter, bb)) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  request_rec* request = filter->r;
  InstawebContext* context = static_cast<InstawebContext*>(filter->ctx);

  LOG(INFO) << "Instaweb OutputFilter called for request "
            << request->unparsed_uri;

  // Initialize per-request context structure.  Note that instaweb_out_filter
  // may get called multiple times per HTTP request, and this occurs only
  // on the first call.
  if (context == NULL) {
    // Check if mod_instaweb has already rewritten the HTML.  If the server is
    // setup as both the original and the proxy server, mod_pagespeed filter may
    // be applied twice. To avoid this, skip the content if it is already
    // optimized by mod_pagespeed.
    if (apr_table_get(request->headers_out, "x-instaweb") != NULL) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                    "Already has x-instaweb");
      ap_remove_output_filter(filter);
      return ap_pass_brigade(filter->next, bb);
    }

    html_rewriter::ContentEncoding encoding = get_content_encoding(request);
    if (encoding == html_rewriter::GZIP) {
      // Unset the content encoding because the html_rewriter will decode the
      // content before parsing.
      apr_table_unset(request->headers_out, "Content-Encoding");
      apr_table_unset(request->err_headers_out, "Content-Encoding");
    } else if (encoding == html_rewriter::OTHER) {
      // We don't know the encoding, so we cannot rewrite the HTML.
      ap_remove_output_filter(filter);
      return ap_pass_brigade(filter->next, bb);
    }
    filter->ctx = context = new InstawebContext(request->pool);
    context->bucket_brigade = apr_brigade_create(
        request->pool,
        request->connection->bucket_alloc);
    std::string base_url;
    if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
      base_url = request->unparsed_uri;
    } else {
      base_url = ap_construct_url(request->pool, request->unparsed_uri,
                                  request);
    }
    LOG(INFO) << "unparsed=" << request->unparsed_uri
              << ", base_url=" << base_url;

    net_instaweb::SimpleMetaData request_headers, response_headers;
    net_instaweb::ApacheHeaderToMetaData(request->headers_in, 0,
                                         request->proto_num, &request_headers);
    LOG(INFO) << "Request headers:\n" << request_headers.ToString();

    // Hack for mod_proxy to figure out where it's proxying from
    LOG(INFO) << "request->filename=" << request->filename << ", uri="
              << request->unparsed_uri;
    if ((request->filename != NULL) &&
        (strncmp(request->filename, "proxy:", 6) == 0)) {
      base_url.assign(request->filename + 6, strlen(request->filename) - 6);
    }

    context->rewriter.reset(new HtmlRewriter(server_config->context,
                                             encoding,
                                             base_url,
                                             request->unparsed_uri,
                                             &context->output));
    apr_table_setn(request->headers_out, "x-instaweb", "1");
    apr_table_unset(request->headers_out, "Content-Length");
    apr_table_unset(request->headers_out, "Content-MD5");
    apr_table_unset(request->headers_out, "Content-Encoding");

    // Note that downstream output filters may further mutate the response
    // headers, and this will not show those mutations.
    net_instaweb::ApacheHeaderToMetaData(request->headers_out, request->status,
                                         request->proto_num, &response_headers);
    LOG(INFO) << "Instaweb Response headers:\n" << response_headers.ToString();
  }

  apr_bucket* new_bucket = NULL;
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
        APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, new_bucket);
      }
    } else if (APR_BUCKET_IS_EOS(bucket)) {
      new_bucket = rewrite_html(filter, FINISH, NULL, 0);
      if (new_bucket != NULL) {
        APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, new_bucket);
      }
      // Insert the EOS bucket to the new brigade.
      APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, bucket);
      // OK, we have seen the EOS. Time to pass it along down the chain.
      return ap_pass_brigade(filter->next, context->bucket_brigade);
    } else if (APR_BUCKET_IS_FLUSH(bucket)) {
      new_bucket = rewrite_html(filter, FLUSH, NULL, 0);
      if (new_bucket != NULL) {
        APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, new_bucket);
      }
      APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, bucket);
      // OK, Time to flush, pass it along down the chain.
      apr_status_t ret_code =
          ap_pass_brigade(filter->next, context->bucket_brigade);
      if (ret_code != APR_SUCCESS) {
        return ret_code;
      }
    } else {
      // TODO(lsong): remove this log.
      ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                    "Unknown meta data");
      APR_BRIGADE_INSERT_TAIL(context->bucket_brigade, bucket);
    }
  }

  apr_brigade_cleanup(bb);
  return APR_SUCCESS;
}

apr_status_t pagespeed_child_exit(void* data) {
  html_rewriter::PageSpeedConfig* config =
      static_cast<html_rewriter::PageSpeedConfig*>(data);
//  delete config->context;
  config->context = NULL;
  return APR_SUCCESS;
}

void pagespeed_child_init(apr_pool_t* pool, server_rec* server) {
  // Create PageSpeed context used by instaweb rewrite-driver.  This is
  // per-process, so we initialize all the server's context by iterating the
  // server lists in server->next.
  server_rec* next_server = server;
  while (next_server) {
    html_rewriter::PageSpeedConfig* config =
        html_rewriter::mod_pagespeed_get_server_config(next_server);
    if (html_rewriter::CreatePageSpeedServerContext(pool, config)) {
      // Free memory used in config before the pool is destroyed, because
      // some the components in config use sub-pool of the pool.

      // Also consider checking AP_MODULE_MAGIC_AT_LEAST(20051115, 15)
#if ((APR_MAJOR_VERSION > 1) ||                         \
     ((APR_MAJOR_VERSION == 1) && APR_PATCH_VERSION > 11))
      apr_pool_pre_cleanup_register(pool, config, pagespeed_child_exit);
#endif
    }
    next_server = next_server->next;
  }
}

int pagespeed_post_config(apr_pool_t* pool, apr_pool_t* plog, apr_pool_t* ptemp,
                          server_rec *server) {
  server_rec* next_server = server;
  while (next_server) {
    html_rewriter::PageSpeedConfig* config =
        html_rewriter::mod_pagespeed_get_server_config(next_server);
    if (config->pagespeed_enable) {
      if (config->rewrite_url_prefix == NULL ||
          config->generated_file_prefix == NULL ||
          config->file_cache_path == NULL) {
        LOG(ERROR) << "Page speed is enabled. "
                   << "The following directives must not be NULL";
        LOG(ERROR) << kInstawebUrlPrefix << "="
                   << config->rewrite_url_prefix;
        LOG(ERROR) << kInstawebFileCachePath << "="
                   << config->file_cache_path;
        LOG(ERROR) << kInstawebGeneratedFilePrefix << "="
                   << config->generated_file_prefix;
        return HTTP_INTERNAL_SERVER_ERROR;
      }
      if (config->file_cache_clean_interval_ms == 0) {
        // Default to 1 hour
        config->file_cache_clean_interval_ms = 60 * 60 * 1000;
      }
      if (config->file_cache_size_kb == 0) {
        // Default to 100M
        config->file_cache_size_kb = 100 * 1024;
      }
      LOG(INFO) << kInstawebUseHttpCache << " "
                << config->use_http_cache;
      LOG(INFO) << kInstawebRewriters << " "
                << config->rewriters;
    }
    next_server = next_server->next;
  }
  return OK;
}

// Here log transaction will wait for all the asynchronous resource fetchers to
// finish.
apr_status_t pagespeed_log_transaction(request_rec *request) {
  server_rec* server = request->server;
  html_rewriter::PageSpeedConfig* config =
      html_rewriter::mod_pagespeed_get_server_config(server);
  if (config == NULL || config->context == NULL ||
      config->context->rewrite_driver_factory() == NULL) {
    return DECLINED;
  }
#if 0
  html_rewriter::SerfUrlAsyncFetcher* url_async_fetcher =
      config->context->rewrite_driver_factory()->serf_url_async_fetcher();
  if (url_async_fetcher == NULL) {
    return DECLINED;
  }
  int64 max_ms = GetFetcherTimeOut(config->context);  // milliseconds.
  net_instaweb::GoogleMessageHandler handler;
  if (!url_async_fetcher->WaitForInProgressFetches(max_ms, &handler)) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "SerfFetch timeout request=%s", request->unparsed_uri);
  }
#endif
  return DECLINED;
}

// This function is a callback and it declares what
// other functions should be called for request
// processing and configuration requests. This
// callback function declares the Handlers for
// other events.
void mod_pagespeed_register_hooks(apr_pool_t *p) {
  // Enable logging using pagespeed style
  html_rewriter::InstallLogMessageHandler(p);

  // Use instaweb to handle generated resources.
  ap_hook_handler(mod_pagespeed::instaweb_handler, NULL, NULL, -1);
  ap_register_output_filter(instaweb_filter_name,
                            instaweb_out_filter,
                            NULL,
                            AP_FTYPE_RESOURCE);
  ap_hook_post_config(pagespeed_post_config, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(pagespeed_child_init, NULL, NULL, APR_HOOK_LAST);
  ap_hook_log_transaction(pagespeed_log_transaction, NULL, NULL, APR_HOOK_LAST);
}

void* mod_pagespeed_create_server_config(apr_pool_t* pool, server_rec* server) {
  html_rewriter::PageSpeedConfig* config =
      html_rewriter::mod_pagespeed_get_server_config(server);
  if (config == NULL) {
    config = static_cast<html_rewriter::PageSpeedConfig*>(
        apr_pcalloc(pool, sizeof(html_rewriter::PageSpeedConfig)));
    memset(config, 0, sizeof(html_rewriter::PageSpeedConfig));
  }
  return config;
}

ConfigSwitch get_config_switch(const char* arg) {
  if (strcasecmp(arg, "on") == 0) {
    return CONFIG_ON;
  } else if (strcasecmp(arg, "off") == 0) {
    return CONFIG_OFF;
  } else {
    return CONFIG_ERROR;
  }
}

}  // namespace

// Getters for mod_pagespeed configuration.
namespace html_rewriter {

PageSpeedConfig* mod_pagespeed_get_server_config(server_rec* server) {
  return static_cast<PageSpeedConfig*> ap_get_module_config(
      server->module_config, &instaweb_module);
}

PageSpeedServerContext* mod_pagespeed_get_config_server_context(
    server_rec* server) {
  PageSpeedConfig* config = mod_pagespeed_get_server_config(server);
  return config->context;
}

}  // namespace html_rewriter

extern "C" {
// Export our module so Apache is able to load us.
// See http://gcc.gnu.org/wiki/Visibility for more information.
#if defined(__linux)
#pragma GCC visibility push(default)
#endif

// TODO(lsong): Refactor the on/off options check. Use a function to repalce the
// repeated statements.
static const char* mod_pagespeed_config_one_string(cmd_parms* cmd, void* data,
                                                   const char* arg) {
  html_rewriter::PageSpeedConfig* config =
      html_rewriter::mod_pagespeed_get_server_config(cmd->server);
  const char* directive = (cmd->directive->directive);
  if (strcasecmp(directive, kInstaweb) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kInstaweb, " on|off", NULL);
    }
    config->pagespeed_enable = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kInstawebUrlPrefix) == 0) {
    config->rewrite_url_prefix = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebFetchProxy) == 0) {
    config->fetch_proxy = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebGeneratedFilePrefix) == 0) {
    config->generated_file_prefix = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebFileCachePath) == 0) {
    config->file_cache_path = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebFileCacheSizeKb) == 0) {
    config->file_cache_size_kb = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebFileCacheCleanIntervalMs) == 0) {
    config->file_cache_clean_interval_ms = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebFetcherTimeoutMs) == 0) {
    config->fetcher_timeout_ms = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebResourceTimeoutMs) == 0) {
    config->resource_timeout_ms = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebNumShards) == 0) {
    config->num_shards = static_cast<int>(apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebOutlineThreshold) == 0) {
    config->outline_threshold = static_cast<int>(apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebLRUCacheKbPerProcess) == 0) {
    config->lru_cache_kb_per_process =
        static_cast<int64>(apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebLRUCacheByteLimit) == 0) {
    config->lru_cache_byte_limit =
        static_cast<int64>(apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kInstawebUseHttpCache) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kInstawebUseHttpCache, " on|off", NULL);
    }
    config->use_http_cache = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kInstawebRewriters) == 0) {
    config->rewriters = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebSlurpDirectory) == 0) {
    config->slurp_directory = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kInstawebSlurpWrite) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kInstawebUseHttpCache, " on|off", NULL);
    }
    config->slurp_write = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kInstawebForceCaching) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kInstawebForceCaching, " on|off", NULL);
    }
    config->force_caching = (config_switch == CONFIG_ON);
  } else {
    return "Unknown directive.";
  }
  return NULL;
}

// TODO(lsong): Refactor this to make it simple if possible.
static const command_rec mod_pagespeed_filter_cmds[] = {
  AP_INIT_TAKE1(kInstaweb,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Enable instaweb"),
  AP_INIT_TAKE1(kInstawebUrlPrefix,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the url prefix"),
  AP_INIT_TAKE1(kInstawebFetchProxy,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the fetch proxy"),
  AP_INIT_TAKE1(kInstawebGeneratedFilePrefix,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set generated file's prefix"),
  AP_INIT_TAKE1(kInstawebFileCachePath,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the path for file cache"),
  AP_INIT_TAKE1(kInstawebFileCacheSizeKb,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the target size (in kilobytes) for file cache"),
  AP_INIT_TAKE1(kInstawebFileCacheCleanIntervalMs,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the interval (in ms) for cleaning the file cache"),
  AP_INIT_TAKE1(kInstawebFetcherTimeoutMs,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set internal fetcher timeout in milliseconds"),
  AP_INIT_TAKE1(kInstawebResourceTimeoutMs,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set resource fetcher timeout in milliseconds"),
  AP_INIT_TAKE1(kInstawebNumShards,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set number of shards"),
  AP_INIT_TAKE1(kInstawebLRUCacheKbPerProcess,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the total size, in KB, of the per-process "
                "in-memory LRU cache"),
  AP_INIT_TAKE1(kInstawebLRUCacheByteLimit,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the maximum byte size entry to store in the per-process "
                "in-memory LRU cache"),
  AP_INIT_TAKE1(kInstawebUseHttpCache,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to use http cache"),
  AP_INIT_TAKE1(kInstawebRewriters,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Comma-separated list of rewriting filters"),
  AP_INIT_TAKE1(kInstawebSlurpDirectory,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Directory from which to read slurped resources"),
  AP_INIT_TAKE1(kInstawebSlurpWrite,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Directory from which to read slurped resources"),
  AP_INIT_TAKE1(kInstawebForceCaching,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Ignore HTTP cache headers and TTLs"),
  {NULL}
};
// Declare and populate the module's data structure.  The
// name of this structure ('instaweb_module') is important - it
// must match the name of the module.  This structure is the
// only "glue" between the httpd core and the module.
module AP_MODULE_DECLARE_DATA instaweb_module = {
  // Only one callback function is provided.  Real
  // modules will need to declare callback functions for
  // server/directory configuration, configuration merging
  // and other tasks.
  STANDARD20_MODULE_STUFF,
  NULL,  // create per-directory config structure
  NULL,  // merge per-directory config structures
  mod_pagespeed_create_server_config,
  NULL,  // merge per-server config structures
  mod_pagespeed_filter_cmds,
  mod_pagespeed_register_hooks,      // callback for registering hooks
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"
