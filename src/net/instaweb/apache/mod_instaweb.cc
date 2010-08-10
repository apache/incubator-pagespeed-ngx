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

#include "base/string_util.h"
#include "net/instaweb/apache/html_rewriter.h"
#include "net/instaweb/apache/html_rewriter_config.h"
#include "net/instaweb/apache/html_parser_message_handler.h"
#include "net/instaweb/apache/pagespeed_server_context.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/instaweb_handler.h"
#include "net/instaweb/apache/mod_instaweb.h"
#include "mod_spdy/apache/log_message_handler.h"
#include "mod_spdy/apache/pool_util.h"
#include "third_party/apache/apr/src/include/apr_strings.h"
// The httpd header must be after the pagepseed_server_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "third_party/apache/httpd/src/include/httpd.h"
#include "third_party/apache/httpd/src/include/http_config.h"
#include "third_party/apache/httpd/src/include/http_core.h"
#include "third_party/apache/httpd/src/include/http_log.h"
#include "third_party/apache/httpd/src/include/http_protocol.h"

using html_rewriter::HtmlRewriter;

extern "C" {
extern module AP_MODULE_DECLARE_DATA pagespeed_module;

// Pagespeed directive names.
const char* kPagespeed = "Pagespeed";
const char* kPagespeedRewriteUrlPrefix = "PagespeedRewriteUrlPrefix";
const char* kPagespeedFetchProxy = "PagespeedFetchProxy";
const char* kPagespeedGeneratedFilePrefix = "PagespeedGeneratedFilePrefix";
const char* kPagespeedFileCachePath = "PagespeedFileCachePath";
const char* kPagespeedFetcherTimeoutMs = "PagespeedFetcherTimeOutMs";
const char* kPagespeedResourceTimeoutMs = "PagespeedResourceTimeOutMs";
const char* kPagespeedRewriterNumShards = "PagespeedRewriterNumShards";
const char* kPagespeedRewriterUseHttpCache = "PagespeedRewriterUseHttpCache";
const char* kPagespeedRewriterUseThreadsafeCache =
    "PagespeedRewriterUseThreadsafeCache";
const char* kPagespeedRewriterCombineCss = "PagespeedRewriterCombineCss";
const char* kPagespeedRewriterOutlineCss = "PagespeedRewriterOutlineCss";
const char* kPagespeedRewriterOutlineJavascript =
    "PagespeedRewriterOutlineJavascript";
const char* kPagespeedRewriterRewrieImages = "PagespeedRewriterRewriteImages";
const char* kPagespeedRewriterExtendCache = "PagespeedRewriterExtendCache";
const char* kPagespeedRewriterAddHead = "PagespeedRewriterAddHead";
const char* kPagespeedRewriterAddBaseTag = "PagespeedRewriterAddBaseTag";
const char* kPagespeedRewriterRemoveQuotes = "PagespeedRewriterRemoveQuotes";
const char* kPagespeedRewriterForceCaching = "PagespeedRewriterForceCaching";
const char* kPagespeedRewriterMoveCssToHead = "PagespeedRewriterMoveCssToHead";
const char* kPagespeedRewriterElideAttributes
    = "PagespeedRewriterElideAttributes";
const char* kPagespeedRewriterRemoveComments
    = "PagespeedRewriterRemoveComments";
const char* kPagespeedRewriterCollapseWhiteSpace
    = "PagespeedRewriterCollapseWhiteSpace";
}  // extern "C"

namespace {

const char* pagespeed_filter_name = "PAGESPEED";

enum RewriteOperation {REWRITE, FLUSH, FINISH};
enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

// We use the following structure to keep the pagespeed module context. The
// rewriter will put the rewritten content into the output string when flushed
// or finished. We call Flush when we see the FLUSH bucket, and call Finish when
// we see the EOS bucket.
struct PagespeedContext {
  std::string output;  // content after instaweb rewritten.
  HtmlRewriter* rewriter;
  apr_bucket_brigade* bucket_brigade;
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
                  "Content-Type=%s URI=%s%s",
                  request->content_type, request->hostname,
                  request->unparsed_uri);
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
  PagespeedContext* context = static_cast<PagespeedContext*>(filter->ctx);
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

apr_status_t pagespeed_out_filter(ap_filter_t *filter, apr_bucket_brigade *bb) {
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
  PagespeedContext* context = static_cast<PagespeedContext*>(filter->ctx);

  // Initialize pagespeed context structure.
  if (context == NULL) {
    // Check if mod_pagespeed has already rewritten the HTML.  If the server is
    // setup as both the original and the proxy server, mod_pagespeed filter may
    // be applied twice. To avoid this, skip the content if it is already
    // optimized by mod_pagespeed.
    if (apr_table_get(request->headers_out, "x-pagespeed") != NULL) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                    "Already has x-pagespeed");
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
    filter->ctx = context = new PagespeedContext;
    mod_spdy::PoolRegisterDelete(request->pool, context);
    context->bucket_brigade = apr_brigade_create(
        request->pool,
        request->connection->bucket_alloc);
    std::string base_url(ap_construct_url(request->pool,
                                          request->unparsed_uri,
                                          request));
    context->rewriter = new HtmlRewriter(server_config->context,
                                         encoding,
                                         base_url,
                                         request->unparsed_uri,
                                         &context->output);
    mod_spdy::PoolRegisterDelete(request->pool, context->rewriter);
    apr_table_setn(request->headers_out, "x-pagespeed", "1");
    apr_table_unset(request->headers_out, "Content-Length");
    apr_table_unset(request->headers_out, "Content-MD5");
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
      apr_pool_pre_cleanup_register(pool, config, pagespeed_child_exit);
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
        LOG(ERROR) << kPagespeedRewriteUrlPrefix << "="
                   << config->rewrite_url_prefix;
        LOG(ERROR) << kPagespeedFileCachePath << "="
                   << config->file_cache_path;
        LOG(ERROR) << kPagespeedGeneratedFilePrefix << "="
                   << config->generated_file_prefix;
        return HTTP_INTERNAL_SERVER_ERROR;
      }
      LOG(INFO) << kPagespeedRewriterUseHttpCache << " "
                << config->use_http_cache;
      LOG(INFO) << kPagespeedRewriterUseThreadsafeCache << " "
                << config->use_threadsafe_cache;
      LOG(INFO) << kPagespeedRewriterCombineCss << " "
                << config->combine_css;
      LOG(INFO) << kPagespeedRewriterOutlineCss << " "
                << config->outline_css;
      LOG(INFO) << kPagespeedRewriterOutlineJavascript << " "
                << config->outline_javascript;
      LOG(INFO) << kPagespeedRewriterRewrieImages << " "
                << config->rewrite_images;
      LOG(INFO) << kPagespeedRewriterExtendCache << " "
                << config->extend_cache;
      LOG(INFO) << kPagespeedRewriterAddHead << " "
                << config->add_head;
      LOG(INFO) << kPagespeedRewriterAddBaseTag << " "
                << config->add_base_tag;
      LOG(INFO) << kPagespeedRewriterRemoveQuotes << " "
                << config->remove_quotes;
      LOG(INFO) << kPagespeedRewriterForceCaching << " "
                << config->force_caching;
      LOG(INFO) << kPagespeedRewriterMoveCssToHead << " "
                << config->move_css_to_head;
      LOG(INFO) << kPagespeedRewriterElideAttributes << " "
                << config->elide_attributes;
      LOG(INFO) << kPagespeedRewriterRemoveComments << " "
                << config->remove_comments;
      LOG(INFO) << kPagespeedRewriterCollapseWhiteSpace << " "
                << config->collapse_whitespace;
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
  html_rewriter::SerfUrlAsyncFetcher* url_async_fetcher =
      config->context->rewrite_driver_factory()->serf_url_async_fetcher();
  if (url_async_fetcher == NULL) {
    return DECLINED;
  }
  int64 max_ms = GetFetcherTimeOut(config->context);  // milliseconds.
  html_rewriter::HtmlParserMessageHandler handler;
  if (!url_async_fetcher->WaitForInProgressFetches(max_ms, &handler)) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "SerfFetch timeout reuqest=%s", request->unparsed_uri);
  }
  return DECLINED;
}

// This function is a callback and it declares what
// other functions should be called for request
// processing and configuration requests. This
// callback function declares the Handlers for
// other events.
void mod_pagespeed_register_hooks(apr_pool_t *p) {
  // Enable logging using pagespeed style
  mod_spdy::InstallLogMessageHandler();

  // Use instaweb to handle generated resources.
  ap_hook_handler(mod_pagespeed::instaweb_handler, NULL, NULL, -1);
  ap_register_output_filter(pagespeed_filter_name,
                            pagespeed_out_filter,
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
      server->module_config, &pagespeed_module);
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
  if (strcasecmp(directive, kPagespeed) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeed, " on|off", NULL);
    }
    config->pagespeed_enable = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriteUrlPrefix) == 0) {
    config->rewrite_url_prefix = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kPagespeedFetchProxy) == 0) {
    config->fetch_proxy = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kPagespeedGeneratedFilePrefix) == 0) {
    config->generated_file_prefix = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kPagespeedFileCachePath) == 0) {
    config->file_cache_path = apr_pstrdup(cmd->pool, arg);
  } else if (strcasecmp(directive, kPagespeedFetcherTimeoutMs) == 0) {
    config->fetcher_timeout_ms = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kPagespeedResourceTimeoutMs) == 0) {
    config->resource_timeout_ms = static_cast<int64>(
        apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kPagespeedRewriterNumShards) == 0) {
    config->num_shards = static_cast<int>(apr_strtoi64(arg, NULL, 10));
  } else if (strcasecmp(directive, kPagespeedRewriterUseHttpCache) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterUseHttpCache,
                         " on|off", NULL);
    }
    config->use_http_cache = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterUseThreadsafeCache) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterUseThreadsafeCache,
                         " on|off", NULL);
    }
    config->use_threadsafe_cache = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterCombineCss) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterCombineCss,
                         " on|off", NULL);
    }
    config->combine_css = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterOutlineCss) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterOutlineCss,
                         " on|off", NULL);
    }
    config->outline_css = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterOutlineJavascript) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterOutlineJavascript,
                         " on|off", NULL);
    }
    config->outline_javascript = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterRewrieImages) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterRewrieImages,
                         " on|off", NULL);
    }
    config->rewrite_images = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterExtendCache) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterExtendCache,
                         " on|off", NULL);
    }
    config->extend_cache = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterAddHead) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterAddHead,
                         " on|off", NULL);
    }
    config->add_head = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterAddBaseTag) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterAddBaseTag,
                         " on|off", NULL);
    }
    config->add_base_tag = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterRemoveQuotes) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterRemoveQuotes,
                         " on|off", NULL);
    }
    config->remove_quotes = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterForceCaching) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, kPagespeedRewriterForceCaching,
                         " on|off", NULL);
    }
    config->force_caching = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterMoveCssToHead) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, directive,
                         " on|off", NULL);
    }
    config->move_css_to_head = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterElideAttributes) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, directive,
                         " on|off", NULL);
    }
    config->elide_attributes = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterRemoveComments) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, directive,
                         " on|off", NULL);
    }
    config->remove_comments = (config_switch == CONFIG_ON);
  } else if (strcasecmp(directive, kPagespeedRewriterCollapseWhiteSpace) == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, directive,
                         " on|off", NULL);
    }
    config->collapse_whitespace = (config_switch == CONFIG_ON);
  } else {
    return "Unknown directive.";
  }
  return NULL;
}

// TODO(lsong): Refactor this to make it simple if possible.
static const command_rec mod_pagespeed_filter_cmds[] = {
  AP_INIT_TAKE1(kPagespeed,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Enable pagespeed"),
  AP_INIT_TAKE1(kPagespeedRewriteUrlPrefix,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the url prefix"),
  AP_INIT_TAKE1(kPagespeedFetchProxy,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the fetch proxy"),
  AP_INIT_TAKE1(kPagespeedGeneratedFilePrefix,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set generated file's prefix"),
  AP_INIT_TAKE1(kPagespeedFileCachePath,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set the path for file cache"),
  AP_INIT_TAKE1(kPagespeedFetcherTimeoutMs,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set internal fetcher timeout in milliseconds"),
  AP_INIT_TAKE1(kPagespeedResourceTimeoutMs,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set resource fetcher timeout in milliseconds"),
  AP_INIT_TAKE1(kPagespeedRewriterNumShards,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set number of shards"),
  AP_INIT_TAKE1(kPagespeedRewriterUseHttpCache,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to use http cache"),
  AP_INIT_TAKE1(kPagespeedRewriterUseThreadsafeCache,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to use thread-safe cache"),
  AP_INIT_TAKE1(kPagespeedRewriterCombineCss,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to combine css"),
  AP_INIT_TAKE1(kPagespeedRewriterOutlineCss,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to outline css"),
  AP_INIT_TAKE1(kPagespeedRewriterOutlineJavascript,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to outline javascript"),
  AP_INIT_TAKE1(kPagespeedRewriterRewrieImages,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to rewrite images"),
  AP_INIT_TAKE1(kPagespeedRewriterExtendCache,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to extend cache"),
  AP_INIT_TAKE1(kPagespeedRewriterAddHead,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to add head"),
  AP_INIT_TAKE1(kPagespeedRewriterAddBaseTag,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to add base tag"),
  AP_INIT_TAKE1(kPagespeedRewriterRemoveQuotes,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to remove quotes"),
  AP_INIT_TAKE1(kPagespeedRewriterForceCaching,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to force caching"),
  AP_INIT_TAKE1(kPagespeedRewriterMoveCssToHead,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to move the css to head"),
  AP_INIT_TAKE1(kPagespeedRewriterElideAttributes,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to elide attributes"),
  AP_INIT_TAKE1(kPagespeedRewriterRemoveComments,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to remove comments"),
  AP_INIT_TAKE1(kPagespeedRewriterCollapseWhiteSpace,
                reinterpret_cast<const char*(*)()>(
                    mod_pagespeed_config_one_string),
                NULL, RSRC_CONF,
                "Set if to collapse whitespace"),
  {NULL}
};
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
  NULL,
  NULL,
  mod_pagespeed_create_server_config,
  NULL,
  mod_pagespeed_filter_cmds,
  mod_pagespeed_register_hooks,      // callback for registering hooks
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"
