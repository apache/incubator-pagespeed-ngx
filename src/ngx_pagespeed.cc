/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jefftk@google.com (Jeff Kaufman)

/*
 * Usage:
 *   server {
 *     pagespeed    on|off;
 *   }
 */

#include "ngx_pagespeed.h"

#include <vector>
#include <set>

#include "ngx_base_fetch.h"
#include "ngx_message_handler.h"
#include "ngx_request_context.h"
#include "ngx_rewrite_driver_factory.h"
#include "ngx_rewrite_options.h"
#include "ngx_server_context.h"
#include "ngx_thread_system.h"

#include "apr_time.h"

#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/furious_matcher.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "pthread_shared_mem.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/stack_buffer.h"

extern ngx_module_t ngx_pagespeed;

// Hacks for debugging.
#define DBG(r, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (r)->connection->log, 0, args)
#define PDBG(ctx, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (ctx)->pagespeed_connection->log, 0, args)
#define CDBG(cf, args...)                                     \
  ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, args)

// Unused flag, see
// http://lxr.evanmiller.org/http/source/http/ngx_http_request.h#L130
#define  NGX_HTTP_PAGESPEED_BUFFERED 0x08

const char* kInternalEtagName = "@psol-etag";

namespace ngx_psol {

StringPiece str_to_string_piece(ngx_str_t s) {
  return StringPiece(reinterpret_cast<char*>(s.data), s.len);
}

char* string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp) {
  // Need space for the final null.
  ngx_uint_t buffer_size = sp.size() + 1;
  char* s = static_cast<char*>(ngx_palloc(pool, buffer_size));
  if (s == NULL) {
    return NULL;
  }
  sp.copy(s, buffer_size /* max to copy */);
  s[buffer_size-1] = '\0';  // Null terminate it.
  return s;
}

ngx_int_t string_piece_to_buffer_chain(
    ngx_pool_t* pool, StringPiece sp, ngx_chain_t** link_ptr,
    bool send_last_buf) {
  // Below, *link_ptr will be NULL if we're starting the chain, and the head
  // chain link.
  *link_ptr = NULL;

  // If non-null, the current last link in the chain.
  ngx_chain_t* tail_link = NULL;

  // How far into sp we're currently working on.
  ngx_uint_t offset;

  // TODO(jefftk): look up the nginx buffer size properly.
  ngx_uint_t max_buffer_size = 8192;  // 8k
  for (offset = 0 ;
       offset < sp.size() ||
           // If we need to send the last buffer bit and there's no data, we
           // should send a single empty buffer.  Otherwise we shouldn't
           // generate empty buffers.
           (offset == 0 && sp.size() == 0);
       offset += max_buffer_size) {
    // Prepare a new nginx buffer to put our buffered writes into.
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(pool));
    if (b == NULL) {
      return NGX_ERROR;
    }

    if (sp.size() == 0) {
      CHECK(offset == 0);                                          // NOLINT
      b->pos = b->start = b->end = b->last = NULL;
      // The purpose of this buffer is just to pass along last_buf.
      b->sync = 1;
    } else {
      CHECK(sp.size() > offset);
      ngx_uint_t b_size = sp.size() - offset;
      if (b_size > max_buffer_size) {
        b_size = max_buffer_size;
      }

      b->start = b->pos = static_cast<u_char*>(ngx_palloc(pool, b_size));
      if (b->pos == NULL) {
        return NGX_ERROR;
      }

      // Copy our writes over.  We're copying from sp[offset] up to
      // sp[offset + b_size] into b which has size b_size.
      sp.copy(reinterpret_cast<char*>(b->pos), b_size, offset);
      b->last = b->end = b->pos + b_size;

      b->temporary = 1;  // Identify this buffer as in-memory and mutable.
    }

    // Prepare a chain link.
    ngx_chain_t* cl = static_cast<ngx_chain_t*>(ngx_alloc_chain_link(pool));
    if (cl == NULL) {
      return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    if (*link_ptr == NULL) {
      // This is the first link in the returned chain.
      *link_ptr = cl;
    } else {
      // Link us into the chain.
      CHECK(tail_link != NULL);
      tail_link->next = cl;
    }

    tail_link = cl;
  }


  CHECK(tail_link != NULL);
  if (send_last_buf) {
    tail_link->buf->last_buf = true;
  }

  return NGX_OK;
}

ngx_int_t copy_response_headers_to_ngx(
    ngx_http_request_t* r,
    const net_instaweb::ResponseHeaders& pagespeed_headers) {
  ngx_http_headers_out_t* headers_out = &r->headers_out;
  headers_out->status = pagespeed_headers.status_code();

  ngx_int_t i;
  for (i = 0 ; i < pagespeed_headers.NumAttributes() ; i++) {
    const GoogleString& name_gs = pagespeed_headers.Name(i);
    const GoogleString& value_gs = pagespeed_headers.Value(i);

    ngx_str_t name, value;

    // To prevent the gzip module from clearing weak etags, we output them
    // using a different name here. The etag header filter module runs behind
    // the gzip compressors header filter, and will rename it to 'ETag'
    if (net_instaweb::StringCaseEqual(name_gs, "etag")
        && net_instaweb::StringCaseStartsWith(value_gs, "W/")) {
      name.len = strlen(kInternalEtagName);
      name.data = reinterpret_cast<u_char*>(
          const_cast<char*>(kInternalEtagName));
    } else {
      name.len = name_gs.length();
      name.data = reinterpret_cast<u_char*>(const_cast<char*>(name_gs.data()));
    }
    value.len = value_gs.length();
    value.data = reinterpret_cast<u_char*>(const_cast<char*>(value_gs.data()));

    // TODO(jefftk): If we're setting a cache control header we'd like to
    // prevent any downstream code from changing it.  Specifically, if we're
    // serving a cache-extended resource the url will change if the resource
    // does and so we've given it a long lifetime.  If the site owner has done
    // something like set all css files to a 10-minute cache lifetime, that
    // shouldn't apply to our generated resources.  See Apache code in
    // net/instaweb/apache/header_util:AddResponseHeadersToRequest

    // Make copies of name and value to put into headers_out.

    u_char* value_s = ngx_pstrdup(r->pool, &value);
    if (value_s == NULL) {
      return NGX_ERROR;
    }

    if (STR_EQ_LITERAL(name, "Content-Type")) {
      // Unlike all the other headers, content_type is just a string.
      headers_out->content_type.data = value_s;
      headers_out->content_type.len = value.len;

      // We should not include the charset when determining content_type_len, so
      // scan for the ';' that marks the start of the charset part.
      for (ngx_uint_t i = 0; i < value.len; i++) {
        if (value_s[i] == ';')
          break;
        headers_out->content_type_len = i + 1;
      }

      // In ngx_http_test_content_type() nginx will allocate and calculate
      // content_type_lowcase if we leave it as null.
      headers_out->content_type_lowcase = NULL;
      continue;
    // TODO(oschaaf): are there any other headers we should not try to
    // copy here?
    } else if (STR_EQ_LITERAL(name, "Connection")) {
      continue;
    } else if (STR_EQ_LITERAL(name, "Vary")) {
      continue;
    } else if (STR_EQ_LITERAL(name, "Keep-Alive")) {
      continue;
    } else if (STR_EQ_LITERAL(name, "Transfer-Encoding")) {
      continue;
    } else if (STR_EQ_LITERAL(name, "Server")) {
      continue;
    }

    u_char* name_s = ngx_pstrdup(r->pool, &name);
    if (name_s == NULL) {
      return NGX_ERROR;
    }

    ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&headers_out->headers));
    if (header == NULL) {
      return NGX_ERROR;
    }

    header->hash = 1;  // Include this header in the output.
    header->key.len = name.len;
    header->key.data = name_s;
    header->value.len = value.len;
    header->value.data = value_s;

    // Populate the shortcuts to commonly used headers.
    if (STR_EQ_LITERAL(name, "Date")) {
      headers_out->date = header;
    } else if (STR_EQ_LITERAL(name, "Etag")) {
      headers_out->etag = header;
    } else if (STR_EQ_LITERAL(name, "Expires")) {
      headers_out->expires = header;
    } else if (STR_EQ_LITERAL(name, "Last-Modified")) {
      headers_out->last_modified = header;
    } else if (STR_EQ_LITERAL(name, "Location")) {
      headers_out->location = header;
    } else if (STR_EQ_LITERAL(name, "Server")) {
      headers_out->server = header;
    }
  }

  return NGX_OK;
}

namespace {

typedef struct {
  net_instaweb::NgxRewriteDriverFactory* driver_factory;
  net_instaweb::MessageHandler* handler;
} ps_main_conf_t;

typedef struct {
  // If pagespeed is configured in some server block but not this one our
  // per-request code will be invoked but server context will be null.  In those
  // cases we neet to short circuit, not changing anything.  Currently our
  // header filter, body filter, and content handler all do this, but if anyone
  // adds another way for nginx to give us a request to process we need to check
  // there as well.
  net_instaweb::NgxServerContext* server_context;
  net_instaweb::ProxyFetchFactory* proxy_fetch_factory;
  net_instaweb::NgxRewriteOptions* options;
  net_instaweb::MessageHandler* handler;
} ps_srv_conf_t;

typedef struct {
  net_instaweb::NgxRewriteOptions* options;
  net_instaweb::MessageHandler* handler;
} ps_loc_conf_t;

ngx_int_t ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

void* ps_create_srv_conf(ngx_conf_t* cf);

char* ps_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child);

char* ps_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);

void ps_release_request_context(void* data);

void ps_set_buffered(ngx_http_request_t* r, bool on);

GoogleString ps_determine_url(ngx_http_request_t* r);

ps_request_ctx_t* ps_get_request_context(ngx_http_request_t* r);

void ps_initialize_server_context(ps_srv_conf_t* cfg);

ngx_int_t ps_update(ps_request_ctx_t* ctx, ngx_event_t* ev);

void ps_connection_read_handler(ngx_event_t* ev);

ngx_int_t ps_create_connection(ps_request_ctx_t* ctx);

namespace CreateRequestContext {
enum Response {
  kOk,
  kError,
  kNotUnderstood,
  kStaticContent,
  kInvalidUrl,
  kPagespeedDisabled,
  kBeacon,
  kStatistics,
  kMessages,
  kPagespeedSubrequest,
  kNotHeadOrGet,
  kErrorResponse,
};
}  // namespace CreateRequestContext

CreateRequestContext::Response ps_create_request_context(
    ngx_http_request_t* r, bool is_resource_fetch);

void ps_send_to_pagespeed(ngx_http_request_t* r,
                          ps_request_ctx_t* ctx,
                          ps_srv_conf_t* cfg_s,
                          ngx_chain_t* in);

ngx_int_t ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

ngx_int_t ps_header_filter(ngx_http_request_t* r);

ngx_int_t ps_init(ngx_conf_t* cf);

char* ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

char* ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

void ps_ignore_sigpipe();

ngx_command_t ps_commands[] = {
  { ngx_string("pagespeed"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1|
    NGX_CONF_TAKE2|NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5,
    ps_srv_configure,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL },

  { ngx_string("pagespeed"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1|
    NGX_CONF_TAKE2|NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5,
    ps_loc_configure,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL },

  ngx_null_command
};

void ps_ignore_sigpipe() {
  struct sigaction act;
  ngx_memzero(&act, sizeof(act));
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGPIPE, &act, NULL);
}

namespace PsConfigure {
enum OptionLevel {
  kServer,
  kLocation,
};
}  // namespace PsConfigure

// These options are copied from mod_instaweb.cc, where
// APACHE_CONFIG_OPTIONX indicates that they can not be set at the
// directory/location level. They are not alphabetized on purpose,
// but rather left in the same order as in mod_instaweb.cc in case
// we end up needing te compare.
// TODO(oschaaf): this duplication is a short term solution.
const char* const global_only_options[] = {
  "BlockingRewriteKey",
  "CacheFlushFilename",
  "CacheFlushPollIntervalSec",
  "DangerPermitFetchFromUnknownHosts",
  "CriticalImagesBeaconEnabled",
  "ExperimentalFetchFromModSpdy",
  "FetcherTimeoutMs",
  "FetchHttps",
  "FetchWithGzip",
  "FileCacheCleanIntervalMs",
  "FileCacheInodeLimit",
  "FileCachePath",
  "FileCacheSizeKb",
  "ForceCaching",
  "ImageMaxRewritesAtOnce",
  "ImgMaxRewritesAtOnce",
  "InheritVHostConfig",
  "InstallCrashHandler",
  "LRUCacheByteLimit",
  "LRUCacheKbPerProcess",
  "MaxCacheableContentLength",
  "MemcachedServers",
  "MemcachedThreads",
  "MemcachedTimeoutUs",
  "MessageBufferSize",
  "NumRewriteThreads",
  "NumExpensiveRewriteThreads",
  "RateLimitBackgroundFetches",
  "ReportUnloadTime",
  "RespectXForwardedProto",
  "SharedMemoryLocks",
  "SlurpDirectory",
  "SlurpFlushLimit",
  "SlurpReadOnly",
  "SupportNoScriptEnabled",
  "StatisticsLoggingChartsCSS",
  "StatisticsLoggingChartsJS",
  "TestProxy",
  "TestProxySlurp",
  "TrackOriginalContentLength",
  "UsePerVHostStatistics",
  "XHeaderValue",
  "LoadFromFile",
  "LoadFromFileMatch",
  "LoadFromFileRule",
  "LoadFromFileRuleMatch",
  "UseNativeFetcher"
};

bool ps_is_global_only_option(const StringPiece& option_name) {
  ngx_uint_t i;
  ngx_uint_t size = sizeof(global_only_options) / sizeof(char*);
  for (i = 0; i < size; i++) {
    if (net_instaweb::StringCaseEqual(global_only_options[i], option_name)) {
      return true;
    }
  }
  return false;
}

#define NGX_PAGESPEED_MAX_ARGS 10
char* ps_configure(ngx_conf_t* cf,
                   net_instaweb::NgxRewriteOptions** options,
                   net_instaweb::MessageHandler* handler,
                   PsConfigure::OptionLevel option_level) {
  if (*options == NULL) {
    *options = new net_instaweb::NgxRewriteOptions();
  }
  // args[0] is always "pagespeed"; ignore it.
  ngx_uint_t n_args = cf->args->nelts - 1;

  // In ps_commands we only register 'pagespeed' as taking up to
  // five arguments, so this check should never fire.
  CHECK(n_args <= NGX_PAGESPEED_MAX_ARGS);
  StringPiece args[NGX_PAGESPEED_MAX_ARGS];

  ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
  ngx_uint_t i;
  for (i = 0 ; i < n_args ; i++) {
    args[i] = str_to_string_piece(value[i+1]);
  }

  if (net_instaweb::StringCaseEqual("UseNativeFetcher", args[0])) {
    if (option_level != PsConfigure::kServer) {
      return const_cast<char*>(
          "UseNativeFetcher can only be set in the http{} block.");
    }
  }
  if (option_level == PsConfigure::kLocation && n_args > 1) {
    if (ps_is_global_only_option(args[0])) {
      return string_piece_to_pool_string(cf->pool, net_instaweb::StrCat(
          "\"", args[0], "\" cannot be set at location scope"));
    }
  }

  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cf->cycle, ngx_pagespeed));
  const char* status = (*options)->ParseAndSetOptions(
      args, n_args, cf->pool, handler, cfg_m->driver_factory);

  // nginx expects us to return a string literal but doesn't mark it const.
  return const_cast<char*>(status);
}

char* ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));
  return ps_configure(cf, &cfg_s->options, cfg_s->handler,
                      PsConfigure::kServer);
}

char* ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(
          ngx_http_conf_get_module_loc_conf(cf, ngx_pagespeed));

  return ps_configure(cf, &cfg_l->options, cfg_l->handler,
                      PsConfigure::kLocation);
}

void ps_cleanup_loc_conf(void* data) {
  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(data);
  delete cfg_l->handler;
  cfg_l->handler = NULL;
  delete cfg_l->options;
  cfg_l->options = NULL;
}

bool factory_deleted = false;

void ps_cleanup_srv_conf(void* data) {
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(data);

  // destroy the factory on the first call, causing all worker threads
  // to be shut down when we destroy any proxy_fetch_factories. This
  // will prevent any queued callbacks to destroyed proxy fetch factories
  // from being executed

  if (!factory_deleted && cfg_s->server_context != NULL) {
    delete cfg_s->server_context->factory();
    factory_deleted = true;
  }
  if (cfg_s->proxy_fetch_factory != NULL) {
    delete cfg_s->proxy_fetch_factory;
    cfg_s->proxy_fetch_factory = NULL;
  }
  delete cfg_s->handler;
  cfg_s->handler = NULL;
  delete cfg_s->options;
  cfg_s->options = NULL;
}

void ps_cleanup_main_conf(void* data) {
  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(data);
  delete cfg_m->handler;
  cfg_m->handler = NULL;
  net_instaweb::NgxRewriteDriverFactory::Terminate();
  net_instaweb::NgxRewriteOptions::Terminate();

  // reset the factory deleted flag, so we will clean up properly next time,
  // in case of a configuration reload.
  // TODO(oschaaf): get rid of the factory_deleted flag
  factory_deleted = false;
}

template <typename ConfT> ConfT* ps_create_conf(ngx_conf_t* cf) {
  ConfT* cfg = static_cast<ConfT*>(ngx_pcalloc(cf->pool, sizeof(ConfT)));
  if (cfg == NULL) {
    return NULL;
  }
  cfg->handler = new net_instaweb::GoogleMessageHandler();
  return cfg;
}

void ps_set_conf_cleanup_handler(
    ngx_conf_t* cf, void (func)(void*), void* data) {                // NOLINT
  ngx_pool_cleanup_t* cleanup_m = ngx_pool_cleanup_add(cf->pool, 0);
  if (cleanup_m == NULL) {
     ngx_conf_log_error(
         NGX_LOG_ERR, cf, 0, "failed to register a cleanup handler");
  } else {
     cleanup_m->handler = func;
     cleanup_m->data = data;
  }
}

void* ps_create_main_conf(ngx_conf_t* cf) {
  ps_main_conf_t* cfg_m = ps_create_conf<ps_main_conf_t>(cf);
  if (cfg_m == NULL) {
    return NGX_CONF_ERROR;
  }
  CHECK(!factory_deleted);
  net_instaweb::NgxRewriteOptions::Initialize();
  net_instaweb::NgxRewriteDriverFactory::Initialize();

  cfg_m->driver_factory = new net_instaweb::NgxRewriteDriverFactory(
      new net_instaweb::NgxThreadSystem());
  ps_set_conf_cleanup_handler(cf, ps_cleanup_main_conf, cfg_m);
  return cfg_m;
}

void* ps_create_srv_conf(ngx_conf_t* cf) {
  ps_srv_conf_t* cfg_s = ps_create_conf<ps_srv_conf_t>(cf);
  if (cfg_s == NULL) {
    return NGX_CONF_ERROR;
  }
  ps_set_conf_cleanup_handler(cf, ps_cleanup_srv_conf, cfg_s);
  return cfg_s;
}

void* ps_create_loc_conf(ngx_conf_t* cf) {
  ps_loc_conf_t* cfg_l = ps_create_conf<ps_loc_conf_t>(cf);
  if (cfg_l == NULL) {
    return NGX_CONF_ERROR;
  }
  ps_set_conf_cleanup_handler(cf, ps_cleanup_loc_conf, cfg_l);
  return cfg_l;
}

// nginx has hierarchical configuration.  It maintains configurations at many
// levels.  At various points it needs to merge configurations from different
// levels, and then it calls this.  First it creates the configuration at the
// new level, parsing any pagespeed directives, then it merges in the
// configuration from the level above.  This function should merge the parent
// configuration into the child.  It's more complex than options->Merge() both
// because of the cases where the parent or child didn't have any pagespeed
// directives and because merging is order-dependent in the opposite way we'd
// like.
void ps_merge_options(net_instaweb::NgxRewriteOptions* parent_options,
                      net_instaweb::NgxRewriteOptions** child_options) {
  if (parent_options == NULL) {
    // Nothing to do.
  } else if (*child_options == NULL) {
    *child_options = parent_options->Clone();
  } else {  // Both non-null.
    // Unfortunately, merging configuration options is order dependent.  We'd
    // like to just do (*child_options)->Merge(*parent_options)
    // but then if we had:
    //    pagespeed RewriteLevel PassThrough
    //    server {
    //       pagespeed RewriteLevel CoreFilters
    //    }
    // it would always be stuck on PassThrough.
    net_instaweb::NgxRewriteOptions* child_specific_options = *child_options;
    *child_options = parent_options->Clone();
    (*child_options)->Merge(*child_specific_options);
    delete child_specific_options;
  }
}

// Called exactly once per server block to merge the main configuration with the
// configuration for this server.
char* ps_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child) {
  ps_srv_conf_t* parent_cfg_s =
      static_cast<ps_srv_conf_t*>(parent);
  ps_srv_conf_t* cfg_s =
      static_cast<ps_srv_conf_t*>(child);

  ps_merge_options(parent_cfg_s->options, &cfg_s->options);

  if (cfg_s->options == NULL) {
    return NGX_CONF_OK;  // No pagespeed options; don't do anything.
  }

  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_conf_get_module_main_conf(cf, ngx_pagespeed));
  cfg_m->driver_factory->set_main_conf(parent_cfg_s->options);
  cfg_s->server_context = cfg_m->driver_factory->MakeNgxServerContext();
  // The server context sets some options when we call global_options(). So
  // let it do that, then merge in options we got from the config file.
  // Once we do that we're done with cfg_s->options.
  cfg_s->server_context->global_options()->Merge(*cfg_s->options);
  delete cfg_s->options;
  cfg_s->options = NULL;

  if (cfg_s->server_context->global_options()->enabled()) {
    // Validate FileCachePath
    net_instaweb::GoogleMessageHandler handler;
    const char* file_cache_path =
        cfg_s->server_context->config()->file_cache_path().c_str();
    if (file_cache_path[0] == '\0') {
      return const_cast<char*>("FileCachePath must be set");
    } else if (!cfg_m->driver_factory->file_system()->IsDir(
        file_cache_path, &handler).is_true()) {
      return const_cast<char*>(
          "FileCachePath must be an nginx-writeable directory");
    }
  }

  return NGX_CONF_OK;
}

char* ps_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child) {
  ps_loc_conf_t* parent_cfg_l = static_cast<ps_loc_conf_t*>(parent);

  // The variant of the pagespeed directive that is acceptable in location
  // blocks is only acceptable in location blocks, so we should never be merging
  // in options from a server or main block.
  CHECK(parent_cfg_l->options == NULL);

  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(child);
  if (cfg_l->options == NULL) {
    // No directory specific options.
    return NGX_CONF_OK;
  }

  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));

  if (cfg_s->server_context == NULL) {
    // Pagespeed options cannot be defined only in location blocks.  There must
    // be at least a single "pagespeed off" in the main block or a server
    // block.
    return NGX_CONF_OK;
  }

  // If we get here we have parent options ("global options") from cfg_s, child
  // options ("directory specific options") from cfg_l, and no options from
  // parent_cfg_l.  Rebase the directory specific options on the global options.
  ps_merge_options(cfg_s->server_context->config(), &cfg_l->options);

  return NGX_CONF_OK;
}

// _ef_ is a shorthand for ETag Filter
ngx_http_output_header_filter_pt ngx_http_ef_next_header_filter;
ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

void ps_release_request_context(void* data) {
  ps_request_ctx_t* ctx = static_cast<ps_request_ctx_t*>(data);

  // proxy_fetch deleted itself if we called Done(), but if an error happened
  // before then we need to tell it to delete itself.
  //
  // If this is a resource fetch then proxy_fetch was never initialized.
  if (ctx->proxy_fetch != NULL) {
    ctx->proxy_fetch->Done(false /* failure */);
  }

  // In the normal flow BaseFetch doesn't delete itself in HandleDone() because
  // we still need to receive notification via pipe and call
  // CollectAccumulatedWrites.  If there's an error and we're cleaning up early
  // then HandleDone() hasn't been called yet and we need the base fetch to wait
  // for that and then delete itself.
  if (ctx->base_fetch != NULL) {
    ctx->base_fetch->Release();
    ctx->base_fetch = NULL;
  }

  if (ctx->inflater_ != NULL) {
    delete ctx->inflater_;
    ctx->inflater_ = NULL;
  }

  // Close the connection, delete the events attached with it, and free it to
  // Nginx's connection pool
  if (ctx->pagespeed_connection != NULL) {
    ngx_close_connection(ctx->pagespeed_connection);
    ctx->pipe_fd = -1;
  }

  if (ctx->pipe_fd != -1) {
    close(ctx->pipe_fd);
  }

  delete ctx;
}

// Tell nginx whether we have network activity we're waiting for so that it sets
// a write handler.  See src/http/ngx_http_request.c:2083.
void ps_set_buffered(ngx_http_request_t* r, bool on) {
  if (on) {
    r->buffered |= NGX_HTTP_PAGESPEED_BUFFERED;
  } else {
    r->buffered &= ~NGX_HTTP_PAGESPEED_BUFFERED;
  }
}

bool ps_is_https(ngx_http_request_t* r) {
  // Based on ngx_http_variable_scheme.
#if (NGX_HTTP_SSL)
  return r->connection->ssl;
#endif
  return false;
}

int ps_determine_port(ngx_http_request_t* r) {
  // Return -1 if the port isn't specified, the port number otherwise.
  //
  // If a Host header was provided, get the host from that.  Otherwise fall back
  // to the local port of the incoming connection.

  int port = -1;
  ngx_table_elt_t* host = r->headers_in.host;

  if (host != NULL) {
    // Host headers can look like:
    //
    //   www.example.com        // normal
    //   www.example.com:8080   // port specified
    //   127.0.0.1              // IPv4
    //   127.0.0.1:8080         // IPv4 with port
    //   [::1]                  // IPv6
    //   [::1]:8080             // IPv6 with port
    //
    // The IPv6 ones are the annoying ones, but the square brackets allow us to
    // disambiguate.  To find the port number, we can say:
    //
    //   1) Take the text after the final colon.
    //   2) If all of those characters are digits, that's your port number
    //
    // In the case of a plain IPv6 address with no port number, the text after
    // the final colon will include a ']', so we'll stop processing.

    StringPiece host_str = str_to_string_piece(host->value);
    size_t colon_index = host_str.rfind(":");
    if (colon_index == host_str.npos) {
      return -1;
    }
    // Strip everything up to and including the final colon.
    host_str.remove_prefix(colon_index + 1);

    bool ok = StringToInt(host_str, &port);
    if (!ok) {
      // Might be malformed port, or just IPv6 with no port specified.
      return -1;
    }

    return port;
  }

  // Based on ngx_http_variable_server_port.
#if (NGX_HAVE_INET6)
  if (r->connection->local_sockaddr->sa_family == AF_INET6) {
    port = ntohs(reinterpret_cast<struct sockaddr_in6*>(
        r->connection->local_sockaddr)->sin6_port);
  }
#endif
  if (port == -1 /* still need port */) {
    port = ntohs(reinterpret_cast<struct sockaddr_in*>(
        r->connection->local_sockaddr)->sin_port);
  }

  return port;
}

GoogleString ps_determine_url(ngx_http_request_t* r) {
  int port = ps_determine_port(r);
  GoogleString port_string;
  if ((ps_is_https(r) && (port == 443 || port == -1)) ||
      (!ps_is_https(r) && (port == 80 || port == -1))) {
    // No port specifier needed for requests on default ports.
    port_string = "";
  } else {
    port_string = net_instaweb::StrCat(
        ":", net_instaweb::IntegerToString(port));
  }

  StringPiece host = str_to_string_piece(r->headers_in.server);
  if (host.size() == 0) {
    // If host is unspecified, perhaps because of a pure HTTP 1.0 "GET /path",
    // fall back to server IP address.  Based on ngx_http_variable_server_addr.
    ngx_str_t  s;
    u_char addr[NGX_SOCKADDR_STRLEN];
    s.len = NGX_SOCKADDR_STRLEN;
    s.data = addr;
    ngx_int_t rc = ngx_connection_local_sockaddr(r->connection, &s, 0);
    if (rc != NGX_OK) {
      s.len = 0;
    }
    host =  str_to_string_piece(s);
  }

  return net_instaweb::StrCat(
      ps_is_https(r) ? "https://" : "http://", host, port_string,
      str_to_string_piece(r->unparsed_uri));
}

// Get the context for this request.  ps_create_request_context
// should already have been called to create it.
ps_request_ctx_t* ps_get_request_context(ngx_http_request_t* r) {
  return static_cast<ps_request_ctx_t*>(
      ngx_http_get_module_ctx(r, ngx_pagespeed));
}

// Returns:
//   NGX_OK: pagespeed is done, request complete
//   NGX_AGAIN: pagespeed still working, needs to be called again later
//   NGX_ERROR: error
ngx_int_t ps_update(ps_request_ctx_t* ctx, ngx_event_t* ev) {
  bool done;
  int rc;
  char chr;
  do {
    rc = read(ctx->pipe_fd, &chr, 1);
  } while (rc == -1 && errno == EINTR);  // Retry on EINTR.

  // read() should only ever return 0 (closed), 1 (data), or -1 (error).
  CHECK(rc == -1 || rc == 0 || rc == 1);

  if (rc == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      PDBG(ctx, "no data to read from pagespeed yet");
      return NGX_AGAIN;
    } else {
      perror("ps_connection_read_handler");
      return NGX_ERROR;
    }
  } else {
    // We're done iff we read 0 bytes because that means the pipe was closed.
    done = (rc == 0);
  }

  // Get output from pagespeed.
  if (ctx->is_resource_fetch && !ctx->sent_headers) {
    // For resource fetches, the first pipe-byte tells us headers are available
    // for fetching.
    rc = ctx->base_fetch->CollectHeaders(&ctx->r->headers_out);
    if (rc != NGX_OK) {
      PDBG(ctx, "problem with CollectHeaders");
      return rc;
    }

    ngx_http_send_header(ctx->r);
    ctx->sent_headers = true;
  } else {
    // For proxy fetches and subsequent resource fetch pipe-bytes, the response
    // body is available for (partial) fetching.
    ngx_chain_t* cl;
    rc = ctx->base_fetch->CollectAccumulatedWrites(&cl);
    if (rc != NGX_OK) {
      PDBG(ctx, "problem with CollectAccumulatedWrites");
      return rc;
    }

    PDBG(ctx, "pagespeed update: %p, done: %d", cl, done);

    if (cl == NULL) {
      rc = string_piece_to_buffer_chain(
          ctx->r->pool, "", &cl, false /* send_last_buf */);
      if (rc != NGX_OK) {
        PDBG(ctx, "problem with string_piece_to_buffer_chain");
        return rc;
      }
    }
    // Pass the optimized content along to later body filters.
    // From Weibin: This function should be called mutiple times. Store the
    // whole file in one chain buffers is too aggressive. It could consume
    // too much memory in busy servers.
    rc = ngx_http_next_body_filter(ctx->r, cl);
    if (rc == NGX_AGAIN && done) {
      ctx->write_pending = 1;
      return NGX_OK;
    }

    if (rc != NGX_OK) {
      return rc;
    }
  }

  return done ? NGX_OK : NGX_AGAIN;
}

void ps_writer(ngx_http_request_t* r) {
  ngx_connection_t* c = r->connection;
  ngx_event_t* wev = c->write;

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                 "http pagespeed writer handler: \"%V?%V\"",
                 &r->uri, &r->args);

  if (wev->timedout) {
    ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                  "client timed out");
    c->timedout = 1;

    ngx_http_finalize_request(r, NGX_HTTP_REQUEST_TIME_OUT);
    return;
  }

  int rc = ngx_http_next_body_filter(r, NULL);
  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                 "http pagespeed writer output filter: %d, \"%V?%V\"",
                 rc, &r->uri, &r->args);
  if (rc == NGX_AGAIN) {
    return;
  }

  r->write_event_handler = ngx_http_request_empty_handler;
  ngx_http_finalize_request(r, rc);
}

ngx_int_t ngx_http_set_pagespeed_write_handler(ngx_http_request_t *r) {
  r->http_state = NGX_HTTP_WRITING_REQUEST_STATE;

  r->read_event_handler = ngx_http_request_empty_handler;
  r->write_event_handler = ps_writer;

  ngx_event_t* wev = r->connection->write;
  ngx_http_core_loc_conf_t* clcf = static_cast<ngx_http_core_loc_conf_t*>(
      ngx_http_get_module_loc_conf(r, ngx_http_core_module));
  ngx_add_timer(wev, clcf->send_timeout);

  if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
    return NGX_ERROR;
  }

  return NGX_OK;
}

void ps_connection_read_handler(ngx_event_t* ev) {
  CHECK(ev != NULL);

  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  CHECK(c != NULL);

  ps_request_ctx_t* ctx =
      static_cast<ps_request_ctx_t*>(c->data);
  CHECK(ctx != NULL);

  int rc = ps_update(ctx, ev);
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "http pagespeed connection read handler rc: %d", rc);

  if (rc == NGX_AGAIN) {
    // Request needs more work by pagespeed.
    rc = ngx_handle_read_event(ev, 0);
    CHECK(rc == NGX_OK);
  } else if (rc == NGX_OK) {
    // Pagespeed is done.  Stop watching the pipe.  If we still have data to
    // write, set a write handler so we can get called back to make our write.
    ngx_del_event(ev, NGX_READ_EVENT, 0);
    ps_set_buffered(ctx->r, false);
    if (ctx->write_pending) {
      if (ngx_http_set_pagespeed_write_handler(ctx->r) != NGX_OK) {
        ngx_http_finalize_request(ctx->r, NGX_HTTP_INTERNAL_SERVER_ERROR);
      }
    } else {
      ngx_http_finalize_request(ctx->r, NGX_DONE);
    }
  } else if (rc == NGX_ERROR) {
    ngx_http_finalize_request(ctx->r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  } else {
    CHECK(false);
  }
}

ngx_int_t ps_create_connection(ps_request_ctx_t* ctx) {
  ngx_connection_t* c = ngx_get_connection(
      ctx->pipe_fd, ctx->r->connection->log);
  if (c == NULL) {
    return NGX_ERROR;
  }

  c->recv = ngx_recv;
  c->send = ngx_send;
  c->recv_chain = ngx_recv_chain;
  c->send_chain = ngx_send_chain;

  c->log_error = ctx->r->connection->log_error;

  c->read->log = c->log;
  c->write->log = c->log;

  ctx->pagespeed_connection = c;

  // Tell nginx to monitor this pipe and call us back when there's data.
  c->data = ctx;
  c->read->handler = ps_connection_read_handler;
  ngx_add_event(c->read, NGX_READ_EVENT, 0);

  return NGX_OK;
}

// Populate cfg_* with configuration information for this
// request.  Thin wrappers around ngx_http_get_module_*_conf and cast.
ps_main_conf_t* ps_get_main_config(ngx_http_request_t* r) {
  return static_cast<ps_main_conf_t*>(
      ngx_http_get_module_main_conf(r, ngx_pagespeed));
}
ps_srv_conf_t* ps_get_srv_config(ngx_http_request_t* r) {
  return static_cast<ps_srv_conf_t*>(
      ngx_http_get_module_srv_conf(r, ngx_pagespeed));
}
ps_loc_conf_t* ps_get_loc_config(ngx_http_request_t* r) {
  return static_cast<ps_loc_conf_t*>(
      ngx_http_get_module_loc_conf(r, ngx_pagespeed));
}

// Wrapper around GetQueryOptions()
net_instaweb::RewriteOptions* ps_determine_request_options(
    ngx_http_request_t* r,
    ps_request_ctx_t* ctx,
    ps_srv_conf_t* cfg_s,
    net_instaweb::GoogleUrl* url) {
  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  //
  // Sets option from request headers and url.
  net_instaweb::ServerContext::OptionsBoolPair query_options_success =
      cfg_s->server_context->GetQueryOptions(
          url, ctx->base_fetch->request_headers(), NULL);
  bool get_query_options_success = query_options_success.second;
  if (!get_query_options_success) {
    // Failed to parse query params or request headers.  Treat this as if there
    // were no query params given.
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "ps_create_request_context: "
                  "parsing headers or query params failed.");
    return NULL;
  }

  // Will be NULL if there aren't any options set with query params or in
  // headers.
  return query_options_success.first;
}

// Check whether this visitor is already in an experiment.  If they're not,
// classify them into one by setting a cookie.  Then set options appropriately
// for their experiment.
//
// See InstawebContext::SetFuriousStateAndCookie()
bool ps_set_furious_state_and_cookie(ngx_http_request_t* r,
                                     ps_request_ctx_t* ctx,
                                     net_instaweb::RewriteOptions* options,
                                     const StringPiece& host) {
  CHECK(options->running_furious());
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  bool need_cookie = cfg_s->server_context->furious_matcher()->
      ClassifyIntoExperiment(*ctx->base_fetch->request_headers(), options);
  if (need_cookie && host.length() > 0) {
    int64 time_now_us = apr_time_now();
    int64 expiration_time_ms = (time_now_us/1000 +
                                options->furious_cookie_duration_ms());

    // TODO(jefftk): refactor SetFuriousCookie to expose the value we want to
    // set on the cookie.
    int state = options->furious_id();
    GoogleString expires;
    net_instaweb::ConvertTimeToString(expiration_time_ms, &expires);
    GoogleString value = StringPrintf(
        "%s=%s; Expires=%s; Domain=.%s; Path=/",
        net_instaweb::furious::kFuriousCookie,
        net_instaweb::furious::FuriousStateToCookieString(state).c_str(),
        expires.c_str(), host.as_string().c_str());

    // Set the GFURIOUS cookie.
    ngx_table_elt_t* cookie = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&r->headers_out.headers));
    if (cookie == NULL) {
      return false;
    }
    cookie->hash = 1;  // Include this header in the response.

    ngx_str_set(&cookie->key, "Set-Cookie");
    // It's not safe to use value.c_str here because cookie header only keeps a
    // pointer to the string data.
    cookie->value.data = reinterpret_cast<u_char*>(
        string_piece_to_pool_string(r->pool, value));
    cookie->value.len = value.size();
  }
  return true;
}

// There are many sources of options:
//  - the request (query parameters and headers)
//  - location block
//  - global server options
//  - experiment framework
// Consider them all, returning appropriate options for this request, of which
// the caller takes ownership.  If the only applicable options are global,
// set options to NULL so we can use server_context->global_options().
bool ps_determine_options(ngx_http_request_t* r,
                          ps_request_ctx_t* ctx,
                          net_instaweb::RewriteOptions** options,
                          net_instaweb::GoogleUrl* url) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  ps_loc_conf_t* cfg_l = ps_get_loc_config(r);

  // Global options for this server.  Never null.
  net_instaweb::RewriteOptions* global_options =
      cfg_s->server_context->global_options();

  // Directory-specific options, usually null.  They've already been rebased off
  // of the global options as part of the configuration process.
  net_instaweb::RewriteOptions* directory_options = cfg_l->options;

  // Request-specific options, nearly always null.  If set they need to be
  // rebased on the directory options or the global options.
  net_instaweb::RewriteOptions* request_options =
      ps_determine_request_options(r, ctx, cfg_s, url);

  // Because the caller takes memory ownership of any options we return, the
  // only situation in which we can avoid allocating a new RewriteOptions is if
  // the global options are ok as are.
  if (directory_options == NULL && request_options == NULL &&
      !global_options->running_furious()) {
    return true;
  }

  // Start with directory options if we have them, otherwise request options.
  if (directory_options != NULL) {
    *options = directory_options->Clone();
  } else {
    *options = global_options->Clone();
  }

  // Modify our options in response to request options or experiment settings,
  // if we need to.  If there are request options then ignore the experiment
  // because we don't want experiments to be contaminated with unexpected
  // settings.
  if (request_options != NULL) {
    (*options)->Merge(*request_options);
    delete request_options;
  } else if ((*options)->running_furious()) {
    bool ok = ps_set_furious_state_and_cookie(r, ctx, *options, url->Host());
    if (!ok) {
      if (*options != NULL) {
        delete *options;
        *options = NULL;
      }
      return false;
    }
  }

  return true;
}

// Fix URL based on X-Forwarded-Proto.
// http://code.google.com/p/modpagespeed/issues/detail?id=546 For example, if
// Apache gives us the URL "http://www.example.com/" and there is a header:
// "X-Forwarded-Proto: https", then we update this base URL to
// "https://www.example.com/".  This only ever changes the protocol of the url.
//
// Returns true if it modified url, false otherwise.
bool ps_apply_x_forwarded_proto(ngx_http_request_t* r, GoogleString* url) {
  // First check for an X-Forwarded-Proto header.
  const ngx_str_t* x_forwarded_proto_header = NULL;

  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  ngx_list_part_t* part = &(r->headers_in.headers.part);
  ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }
    if (STR_CASE_EQ_LITERAL(header[i].key, "X-Forwarded-Proto")) {
      x_forwarded_proto_header = &header[i].value;
      break;
    }
  }

  if (x_forwarded_proto_header == NULL) {
    return false;  // No X-Forwarded-Proto header found.
  }

  StringPiece x_forwarded_proto
      = str_to_string_piece(*x_forwarded_proto_header);

  if (!STR_CASE_EQ_LITERAL(*x_forwarded_proto_header, "http") &&
      !STR_CASE_EQ_LITERAL(*x_forwarded_proto_header, "https")) {
    LOG(WARNING) << "Unsupported X-Forwarded-Proto: " << x_forwarded_proto
                 << " for URL " << url << " protocol not changed.";
    return false;
  }

  StringPiece url_sp(*url);
  StringPiece::size_type colon_pos = url_sp.find(":");

  if (colon_pos == StringPiece::npos) {
    return false;  // URL appears to have no protocol; give up.
  }

  // Replace URL protocol with that specified in X-Forwarded-Proto.
  *url = net_instaweb::StrCat(x_forwarded_proto, url_sp.substr(colon_pos));

  return true;
}

bool is_pagespeed_subrequest(ngx_http_request_t* r) {
  ngx_table_elt_t* user_agent_header = r->headers_in.user_agent;
  if (user_agent_header == NULL) {
    return false;
  }
  StringPiece user_agent = str_to_string_piece(user_agent_header->value);
  return (user_agent.find(kModPagespeedSubrequestUserAgent) != user_agent.npos);
}


// TODO(jud): Reuse the version in proxy_interface.cc.
bool UrlMightHavePropertyCacheEntry(const net_instaweb::GoogleUrl& url) {
  const net_instaweb::ContentType* type =
      net_instaweb::NameExtensionToContentType(url.LeafSansQuery());
  if (type == NULL) {
    return true;  // http://www.example.com/  -- no extension; could be HTML.
  }

  // Use a complete switch-statement rather than type()->IsHtmlLike()
  // so that every time we add a new content-type we make an explicit
  // decision about whether it should induce a pcache read.
  //
  // TODO(jmarantz): currently this returns false for ".txt".  Thus we will
  // do no optimizations relying on property-cache on HTML files ending with
  // ".txt".  We should determine whether this is the right thing or not.
  switch (type->type()) {
    case net_instaweb::ContentType::kHtml:
    case net_instaweb::ContentType::kXhtml:
    case net_instaweb::ContentType::kCeHtml:
      return true;
    case net_instaweb::ContentType::kJavascript:
    case net_instaweb::ContentType::kCss:
    case net_instaweb::ContentType::kText:
    case net_instaweb::ContentType::kXml:
    case net_instaweb::ContentType::kPng:
    case net_instaweb::ContentType::kGif:
    case net_instaweb::ContentType::kJpeg:
    case net_instaweb::ContentType::kSwf:
    case net_instaweb::ContentType::kWebp:
    case net_instaweb::ContentType::kIco:
    case net_instaweb::ContentType::kPdf:
    case net_instaweb::ContentType::kOther:
    case net_instaweb::ContentType::kJson:
    case net_instaweb::ContentType::kVideo:
    case net_instaweb::ContentType::kOctetStream:
      return false;
  }
  LOG(DFATAL) << "URL " << url.Spec() << ": unexpected type:" << type->type()
              << "; " << type->mime_type() << "; " << type->file_extension();
  return false;
}

// TODO(jud): Reuse ProxyInterface::InitiatePropertyCacheLookup.
net_instaweb::ProxyFetchPropertyCallbackCollector*
ps_initiate_property_cache_lookup(
    net_instaweb::ServerContext* server_context,
    bool is_resource_fetch,
    const net_instaweb::GoogleUrl& request_url,
    net_instaweb::RewriteOptions* options,
    net_instaweb::AsyncFetch* async_fetch,
    bool* added_page_property_callback) {
  net_instaweb::RequestContextPtr request_ctx = async_fetch->request_context();

  StringPiece user_agent = async_fetch->request_headers()->Lookup1(
      net_instaweb::HttpAttributes::kUserAgent);
  net_instaweb::UserAgentMatcher::DeviceType device_type =
      server_context->user_agent_matcher()->GetDeviceTypeForUA(user_agent);

  scoped_ptr<net_instaweb::ProxyFetchPropertyCallbackCollector>
      callback_collector(new net_instaweb::ProxyFetchPropertyCallbackCollector(
          server_context, request_url.Spec(), request_ctx, options,
          device_type));
  bool added_callback = false;
  net_instaweb::PropertyPageStarVector property_callbacks;

  net_instaweb::ProxyFetchPropertyCallback* client_callback = NULL;
  net_instaweb::ProxyFetchPropertyCallback* property_callback = NULL;
  net_instaweb::ProxyFetchPropertyCallback* fallback_property_callback = NULL;
  net_instaweb::PropertyCache* page_property_cache =
      server_context->page_property_cache();
  net_instaweb::PropertyCache* client_property_cache =
      server_context->client_property_cache();
  if (!is_resource_fetch &&
      server_context->page_property_cache()->enabled() &&
      UrlMightHavePropertyCacheEntry(request_url) &&
      async_fetch->request_headers()->method() ==
      net_instaweb::RequestHeaders::kGet) {
    if (options != NULL) {
      server_context->ComputeSignature(options);
    }
    net_instaweb::AbstractMutex* mutex =
        server_context->thread_system()->NewMutex();
    const StringPiece& device_type_suffix =
        net_instaweb::UserAgentMatcher::DeviceTypeSuffix(device_type);
    GoogleString page_key = server_context->GetPagePropertyCacheKey(
        request_url.Spec(), options, device_type_suffix);
    property_callback = new net_instaweb::ProxyFetchPropertyCallback(
        net_instaweb::ProxyFetchPropertyCallback::kPropertyCachePage,
        page_property_cache, page_key, device_type,
        callback_collector.get(), mutex);
    callback_collector->AddCallback(property_callback);
    added_callback = true;
    if (added_page_property_callback != NULL) {
      *added_page_property_callback = true;
    }
    // Trigger property cache lookup for the requests which contains query param
    // as cache key without query params. The result of this lookup will be used
    // if actual property page does not contains property value.
    if (options != NULL &&
        options->use_fallback_property_cache_values() &&
        request_url.has_query()) {
      GoogleString fallback_page_key =
          server_context->GetFallbackPagePropertyCacheKey(
              request_url.AllExceptQuery(), options, device_type_suffix);
      fallback_property_callback = new net_instaweb::ProxyFetchPropertyCallback(
          net_instaweb::ProxyFetchPropertyCallback::kPropertyCacheFallbackPage,
          page_property_cache, fallback_page_key, device_type,
          callback_collector.get(),
          server_context->thread_system()->NewMutex());
      callback_collector->AddCallback(fallback_property_callback);
    }
  }

  // Initiate client property cache lookup.
  if (async_fetch != NULL) {
    const char* client_id = async_fetch->request_headers()->Lookup1(
        net_instaweb::HttpAttributes::kXGooglePagespeedClientId);
    if (client_id != NULL) {
      if (client_property_cache->enabled()) {
        net_instaweb::AbstractMutex* mutex =
            server_context->thread_system()->NewMutex();
        client_callback = new net_instaweb::ProxyFetchPropertyCallback(
            net_instaweb::ProxyFetchPropertyCallback::kClientPropertyCachePage,
            client_property_cache, client_id,
            net_instaweb::UserAgentMatcher::kEndOfDeviceType,
            callback_collector.get(), mutex);
        callback_collector->AddCallback(client_callback);
        added_callback = true;
      }
    }
  }

  // All callbacks need to be registered before Reads to avoid race.
  if (property_callback != NULL) {
    page_property_cache->Read(property_callback);
  }
  if (fallback_property_callback != NULL) {
    page_property_cache->Read(fallback_property_callback);
  }
  if (client_callback != NULL) {
    client_property_cache->Read(client_callback);
  }

  if (!added_callback) {
    callback_collector.reset(NULL);
  }
  return callback_collector.release();
}

// Set us up for processing a request.
CreateRequestContext::Response ps_create_request_context(
    ngx_http_request_t* r, bool is_resource_fetch) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);

  if (!cfg_s->server_context->global_options()->enabled()) {
    // Not enabled for this server block.
    return CreateRequestContext::kPagespeedDisabled;
  }

  if (r->err_status != 0) {
    return CreateRequestContext::kErrorResponse;
  }

  GoogleString url_string = ps_determine_url(r);

  net_instaweb::GoogleUrl url(url_string);

  if (!url.is_valid()) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "invalid url");

    // Let nginx deal with the error however it wants; we will see a NULL ctx in
    // the body filter or content handler and do nothing.
    return CreateRequestContext::kInvalidUrl;
  }

  if (is_pagespeed_subrequest(r)) {
    return CreateRequestContext::kPagespeedSubrequest;
  }

  if (url.PathSansLeaf() ==
      net_instaweb::NgxRewriteDriverFactory::kStaticAssetPrefix) {
    return CreateRequestContext::kStaticContent;
  }
  if (url.PathSansQuery() == "/ngx_pagespeed_statistics"
      || url.PathSansQuery() == "/ngx_pagespeed_global_statistics" ) {
    return CreateRequestContext::kStatistics;
  }
  if (url.PathSansQuery() == "/ngx_pagespeed_message") {
    return CreateRequestContext::kMessages;
  }

  net_instaweb::RewriteOptions* global_options =
      cfg_s->server_context->global_options();

  const GoogleString* beacon_url;
  if (ps_is_https(r)) {
    beacon_url = &(global_options->beacon_url().https);
  } else {
    beacon_url = &(global_options->beacon_url().http);
  }

  if (url.PathSansQuery() == StringPiece(*beacon_url)) {
    return CreateRequestContext::kBeacon;
  }

  if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
    return CreateRequestContext::kNotHeadOrGet;
  }

  if (is_resource_fetch && !cfg_s->server_context->IsPagespeedResource(url)) {
    DBG(r, "Passing on content handling for non-pagespeed resource '%s'",
        url_string.c_str());
    return CreateRequestContext::kNotUnderstood;
  }

  int file_descriptors[2];
  int rc = pipe(file_descriptors);
  if (rc != 0) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "pipe() failed");
    return CreateRequestContext::kError;
  }

  if (ngx_nonblocking(file_descriptors[0]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                  ngx_nonblocking_n " pipe[0] failed");
    return CreateRequestContext::kError;
  }

  if (ngx_nonblocking(file_descriptors[1]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                  ngx_nonblocking_n " pipe[1] failed");
    return CreateRequestContext::kError;
  }

  ps_request_ctx_t* ctx = new ps_request_ctx_t();

  ctx->r = r;
  ctx->pipe_fd = file_descriptors[0];
  ctx->is_resource_fetch = is_resource_fetch;
  ctx->write_pending = false;
  ctx->pagespeed_connection = NULL;

  rc = ps_create_connection(ctx);
  if (rc != NGX_OK) {
    close(file_descriptors[1]);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ps_create_request_context: "
                  "no pagespeed connection.");
    ps_release_request_context(ctx);
    return CreateRequestContext::kError;
  }

  // Handles its own deletion.  We need to call Release() when we're done with
  // it, and call Done() on the associated parent (Proxy or Resource) fetch.  If
  // we fail before creating the associated fetch then we need to call Done() on
  // the BaseFetch ourselves.
  ctx->base_fetch = new net_instaweb::NgxBaseFetch(
      r, file_descriptors[1],
      cfg_s->server_context,
      net_instaweb::RequestContextPtr(new net_instaweb::NgxRequestContext(
          cfg_s->server_context->thread_system()->NewMutex(),
          cfg_s->server_context->timer(), r)));

  // If null, that means use global options.
  net_instaweb::RewriteOptions* custom_options = NULL;
  bool ok = ps_determine_options(r, ctx, &custom_options, &url);
  if (!ok) {
    ctx->base_fetch->Done(false);  // Not passed to Proxy/ResourceFetch yet.
    ps_release_request_context(ctx);
    return CreateRequestContext::kError;
  }

  // ps_determine_options modified url, removing any ModPagespeedFoo=Bar query
  // parameters.  Keep url_string in sync with url.
  url.Spec().CopyToString(&url_string);

  net_instaweb::RewriteOptions* options;
  if (custom_options == NULL) {
    options = cfg_s->server_context->global_options();
  } else {
    options = custom_options;
  }

  if (!options->enabled()) {
    // Disabled via query params or request headers.

    ctx->base_fetch->Done(false);  // Not passed to Proxy/ResourceFetch yet.
    ps_release_request_context(ctx);
    return CreateRequestContext::kPagespeedDisabled;
  }

  if (options->respect_x_forwarded_proto()) {
    bool modified_url = ps_apply_x_forwarded_proto(r, &url_string);
    if (modified_url) {
      url.Reset(url_string);
      CHECK(url.is_valid()) << "The output of ps_apply_x_forwarded_proto should"
                            << " always be a valid url because it only changes"
                            << " the scheme between http and https.";
    }
  }

  bool page_callback_added = false;
  scoped_ptr<net_instaweb::ProxyFetchPropertyCallbackCollector>
      property_callback(ps_initiate_property_cache_lookup(
          cfg_s->server_context,
          is_resource_fetch, url, options, ctx->base_fetch,
          &page_callback_added));

  if (is_resource_fetch) {
    // TODO(jefftk): Set using_spdy appropriately.  See
    // ProxyInterface::ProxyRequestCallback
    net_instaweb::ResourceFetch::Start(
        url, custom_options /* null if there aren't custom options */,
        false /* using_spdy */, cfg_s->server_context, ctx->base_fetch);
  } else {
    // If we don't have custom options we can use NewRewriteDriver which reuses
    // rewrite drivers and so is faster because there's no wait to construct
    // them.  Otherwise we have to build a new one every time.

    if (custom_options == NULL) {
      ctx->driver = cfg_s->server_context->NewRewriteDriver(
          ctx->base_fetch->request_context());
    } else {
      // NewCustomRewriteDriver takes ownership of custom_options.
      ctx->driver = cfg_s->server_context->NewCustomRewriteDriver(
          custom_options, ctx->base_fetch->request_context());
    }

    // TODO(jefftk): FlushEarlyFlow would go here.

    // Will call StartParse etc.  The rewrite driver will take care of deleting
    // itself if necessary.
    ctx->proxy_fetch = cfg_s->proxy_fetch_factory->CreateNewProxyFetch(
        url_string, ctx->base_fetch, ctx->driver,
        property_callback.release(),
        NULL /* original_content_fetch */);
  }



  // Set up a cleanup handler on the request.
  ngx_http_cleanup_t* cleanup = ngx_http_cleanup_add(r, 0);
  if (cleanup == NULL) {
    ps_release_request_context(ctx);
    return CreateRequestContext::kError;
  }
  cleanup->handler = ps_release_request_context;
  cleanup->data = ctx;
  ngx_http_set_ctx(r, ctx, ngx_pagespeed);

  return CreateRequestContext::kOk;
}

// Send each buffer in the chain to the proxy_fetch for optimization.
// Eventually it will make it's way, optimized, to base_fetch.
void ps_send_to_pagespeed(ngx_http_request_t* r,
                          ps_request_ctx_t* ctx,
                          ps_srv_conf_t* cfg_s,
                          ngx_chain_t* in) {
  ngx_chain_t* cur;
  int last_buf = 0;
  for (cur = in; cur != NULL; cur = cur->next) {
    last_buf = cur->buf->last_buf;

    // Buffers are not really the last buffer until they've been through
    // pagespeed.
    cur->buf->last_buf = 0;

    CHECK(ctx->proxy_fetch != NULL);
    if (ctx->inflater_ == NULL) {
      ctx->proxy_fetch->Write(
          StringPiece(reinterpret_cast<char*>(cur->buf->pos),
                      cur->buf->last - cur->buf->pos), cfg_s->handler);
    } else {
      char buf[net_instaweb::kStackBufferSize];
      ctx->inflater_->SetInput(reinterpret_cast<char*>(cur->buf->pos),
                               cur->buf->last - cur->buf->pos);
      while (ctx->inflater_->HasUnconsumedInput()) {
        int num_inflated_bytes = ctx->inflater_->InflateBytes(
            buf, net_instaweb::kStackBufferSize);
        if (num_inflated_bytes < 0) {
          cfg_s->handler->Message(net_instaweb::kWarning,
                                  "Corrupted inflation");
        } else if (num_inflated_bytes > 0) {
          ctx->proxy_fetch->Write(StringPiece(buf, num_inflated_bytes),
                                  cfg_s->handler);
        }
      }
    }
    // We're done with buffers as we pass them through, so mark them as sent as
    // we go.
    cur->buf->pos = cur->buf->last;
  }

  if (last_buf) {
    ctx->proxy_fetch->Done(true /* success */);
    ctx->proxy_fetch = NULL;  // ProxyFetch deletes itself on Done().
  } else {
    // TODO(jefftk): Decide whether Flush() is warranted here.
    ctx->proxy_fetch->Flush(cfg_s->handler);
  }
}

ngx_int_t ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return ngx_http_next_body_filter(r, in);
  }

  if (r != r->main) {
    // Don't handle subrequests.
    return ngx_http_next_body_filter(r, in);
  }
  // Don't need to check for a cache flush; already did in ps_header_filter.

  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx == NULL) {
    // ctx is null iff we've decided to pass through this request unchanged.
    return ngx_http_next_body_filter(r, in);
  }

  // We don't want to handle requests with errors, but we should be dealing with
  // that in the header filter and not initializing ctx.
  CHECK(r->err_status == 0);                                         // NOLINT

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed filter \"%V\"", &r->uri);

  if (!ctx->data_received) {
    // This is the first set of buffers we've got for this request.
    ctx->data_received = true;
    // Call this here and not in the header filter because we want to see the
    // headers after any other filters are finished modifying them.  At this
    // point they are final.
    // TODO(jefftk): is this thread safe?
    ctx->base_fetch->PopulateResponseHeaders();
  }

  if (in != NULL) {
    // Send all input data to the proxy fetch.
    ps_send_to_pagespeed(r, ctx, cfg_s, in);
  }

  ps_set_buffered(r, true);
  return NGX_AGAIN;
}

#ifndef ngx_http_clear_etag
// The ngx_http_clear_etag(r) macro was added in 1.3.3.  Backport it if it's not
// present.
#define ngx_http_clear_etag(r)       \
  if (r->headers_out.etag) {         \
    r->headers_out.etag->hash = 0;   \
    r->headers_out.etag = NULL;      \
  }
#endif

// Based on ngx_http_add_cache_control.
ngx_int_t ps_set_cache_control(ngx_http_request_t* r, char* cache_control) {
  if (r->headers_out.cache_control.elts == NULL) {
    ngx_int_t rc = ngx_array_init(&r->headers_out.cache_control, r->pool,
                                  1, sizeof(ngx_table_elt_t *));
    if (rc != NGX_OK) {
      return NGX_ERROR;
    }
  }
  ngx_table_elt_t** cache_control_headers = static_cast<ngx_table_elt_t**>(
      ngx_array_push(&r->headers_out.cache_control));
  if (cache_control_headers == NULL) {
    return NGX_ERROR;
  }
  cache_control_headers[0] = static_cast<ngx_table_elt_t*>(
      ngx_list_push(&r->headers_out.headers));
  if (cache_control_headers[0] == NULL) {
    return NGX_ERROR;
  }
  cache_control_headers[0]->hash = 1;
  ngx_str_set(&cache_control_headers[0]->key, "Cache-Control");
  cache_control_headers[0]->value.len = strlen(cache_control);
  cache_control_headers[0]->value.data =
      reinterpret_cast<u_char*>(cache_control);

  return NGX_OK;
}

void ps_strip_html_headers(ngx_http_request_t* r) {
  // We're modifying content, so switch to 'Transfer-Encoding: chunked' and
  // calculate on the fly.
  ngx_http_clear_content_length(r);

  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  ngx_list_part_t* part = &(r->headers_out.headers.part);
  ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }

    // We also need to strip:
    //   Accept-Ranges
    //    - won't work because our html changes
    //   Vary: Accept-Encoding
    //    - our gzip filter will add this later
    if (STR_CASE_EQ_LITERAL(header[i].key, "Accept-Ranges") ||
        (STR_CASE_EQ_LITERAL(header[i].key, "Vary") &&
         STR_CASE_EQ_LITERAL(header[i].value, "Accept-Encoding"))) {
      // Response headers with hash of 0 are excluded from the response.
      header[i].hash = 0;
    }
  }
}

// Returns true, if the the response headers indicate there are multiple
// content encodings.
bool ps_has_stacked_content_encoding(ngx_http_request_t* r) {
  ngx_uint_t i;
  ngx_list_part_t* part = &(r->headers_out.headers.part);
  ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);
  int field_count = 0;

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }

    // Inspect Content-Encoding headers, checking all value fields
    // If an origin returns gzip,foo, that is what we will get here.
    if (STR_CASE_EQ_LITERAL(header[i].key, "Content-Encoding")) {
      if (header[i].value.data != NULL && header[i].value.len > 0) {
        char* p = reinterpret_cast<char*>(header[i].value.data);
        ngx_uint_t j;
        for (j = 0; j < header[i].value.len; j++) {
          if (p[j] == ',' || j == header[i].value.len - 1) {
            field_count++;
          }
        }
        if (field_count > 1) {
          return true;
        }
      }
    }
  }

  return false;
}

ngx_int_t ps_etag_header_filter(ngx_http_request_t* r) {
  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  ngx_list_part_t* part = &(r->headers_out.headers.part);
  ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);
  u_char* etag = reinterpret_cast<u_char*>(
      const_cast<char*>(kInternalEtagName));

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }

    if (header[i].key.len == strlen(kInternalEtagName) &&
        !ngx_strncasecmp(header[i].key.data, etag, header[i].key.len)) {
      header[i].key.data = reinterpret_cast<u_char*>(const_cast<char*>("ETag"));
      header[i].key.len = 4;
      r->headers_out.etag = header;
      break;
    }
  }

  return ngx_http_ef_next_header_filter(r);
}

ngx_int_t ps_header_filter(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return ngx_http_next_header_filter(r);
  }

  if (r != r->main) {
    // Don't handle subrequests.
    return ngx_http_next_header_filter(r);
  }
  // Poll for cache flush on every request (polls are rate-limited).
  cfg_s->server_context->FlushCacheIfNecessary();

  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx != NULL) {
    // ctx will already exist iff this is a pagespeed resource.  Do nothing.
    CHECK(ctx->is_resource_fetch);
    return ngx_http_next_header_filter(r);
  }

  if (r->err_status != 0) {
    return ngx_http_next_header_filter(r);
  }

  // We don't know what this request is, but we only want to send html through
  // to pagespeed.  Check the content type header and find out.
  const net_instaweb::ContentType* content_type =
      net_instaweb::MimeTypeToContentType(
          str_to_string_piece(r->headers_out.content_type));
  if (content_type == NULL || !content_type->IsHtmlLike()) {
    // Unknown or otherwise non-html content type: skip it.
    return ngx_http_next_header_filter(r);
  }

  switch (ps_create_request_context(
      r, false /* not a resource fetch */)) {
    case CreateRequestContext::kError:
      // TODO(oschaaf): don't finalize, nginx will do that for us.
      // can we put a check in place that we cleaned up
      // properly after ourselves somewhere?
      return NGX_ERROR;
    case CreateRequestContext::kNotUnderstood:
      // This should only happen when ctx->is_resource_fetch is true,
      // in which case we can not get here.
      CHECK(false);
      return NGX_ERROR;
    case CreateRequestContext::kBeacon:
    case CreateRequestContext::kStaticContent:
    case CreateRequestContext::kStatistics:
    case CreateRequestContext::kMessages:
    case CreateRequestContext::kPagespeedSubrequest:
    case CreateRequestContext::kPagespeedDisabled:
    case CreateRequestContext::kInvalidUrl:
    case CreateRequestContext::kNotHeadOrGet:
    case CreateRequestContext::kErrorResponse:
      return ngx_http_next_header_filter(r);
    case CreateRequestContext::kOk:
      break;
  }
  ctx = ps_get_request_context(r);
  CHECK(ctx->driver != NULL);  // Not a resource fetch, so driver is defined.

  if (r->headers_out.content_encoding &&
      r->headers_out.content_encoding->value.len) {
    // headers_out.content_encoding will be set to the exact last
    // Content-Encoding response header value that nginx receives. To
    // check if there were multiple (aka stacked) encodings in the
    // response headers, we must iterate them all.
    if (!ps_has_stacked_content_encoding(r)) {
      StringPiece content_encoding =
          str_to_string_piece(r->headers_out.content_encoding->value);
      net_instaweb::GzipInflater::InflateType inflate_type;
      bool is_encoded = false;
      if (net_instaweb::StringCaseEqual(content_encoding, "deflate")) {
        is_encoded = true;
        inflate_type = net_instaweb::GzipInflater::kDeflate;
      } else if (net_instaweb::StringCaseEqual(content_encoding, "gzip")) {
        is_encoded = true;
        inflate_type = net_instaweb::GzipInflater::kGzip;
      }

      if (is_encoded) {
        r->headers_out.content_encoding->hash = 0;
        r->headers_out.content_encoding = NULL;
        ctx->inflater_ = new net_instaweb::GzipInflater(inflate_type);
        ctx->inflater_->Init();
      }
    }
  }

  const net_instaweb::RewriteOptions* options = ctx->driver->options();

  ps_strip_html_headers(r);

  if (options->modify_caching_headers()) {
    // Don't cache html.  See mod_instaweb:instaweb_fix_headers_filter.
    ps_set_cache_control(r, const_cast<char*>("max-age=0, no-cache"));

    // Pagespeed html doesn't need etags: it should never be cached.
    ngx_http_clear_etag(r);

    // An html page may change without the underlying file changing, because of
    // how resources are included.  Pagespeed adds cache control headers for
    // resources instead of using the last modified header.
    ngx_http_clear_last_modified(r);
  }

  r->filter_need_in_memory = 1;

  // Set the "X-Page-Speed: VERSION" header.
  ngx_table_elt_t* x_pagespeed = static_cast<ngx_table_elt_t*>(
      ngx_list_push(&r->headers_out.headers));
  if (x_pagespeed == NULL) {
    return NGX_ERROR;
  }
  // Tell ngx_http_header_filter_module to include this header in the response.
  x_pagespeed->hash = 1;

  ngx_str_set(&x_pagespeed->key, kPageSpeedHeader);
  // It's safe to use c_str here because once we're handling requests the
  // rewrite options are frozen and won't change out from under us.
  x_pagespeed->value.data = reinterpret_cast<u_char*>(const_cast<char*>(
      options->x_header_value().c_str()));
  x_pagespeed->value.len = options->x_header_value().size();

  return ngx_http_next_header_filter(r);
}

// TODO(oschaaf): make ps_static_handler use ps_write_handler_response? for now,
// minimize the diff
ngx_int_t ps_static_handler(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);

  StringPiece request_uri_path = str_to_string_piece(r->uri);

  // Strip out the common prefix url before sending to
  // StaticJavascriptManager.
  StringPiece file_name = request_uri_path.substr(
      strlen(net_instaweb::NgxRewriteDriverFactory::kStaticAssetPrefix));
  StringPiece file_contents;
  StringPiece cache_header;
  net_instaweb::ContentType content_type;
  bool found = cfg_s->server_context->static_asset_manager()->GetAsset(
      file_name, &file_contents, &content_type, &cache_header);
  if (!found) {
    return NGX_DECLINED;
  }

  // Set and send headers.
  r->headers_out.status = NGX_HTTP_OK;

  // Content length
  r->headers_out.content_length_n = file_contents.size();

  // Content type
  StringPiece content_type_sp(content_type.mime_type());
  r->headers_out.content_type_len = content_type_sp.length();
  r->headers_out.content_type.len = content_type_sp.length();
  r->headers_out.content_type.data = reinterpret_cast<u_char*>(
      string_piece_to_pool_string(r->pool, content_type_sp));
  if (r->headers_out.content_type.data == NULL) {
    return NGX_ERROR;
  }
  // ngx_http_test_content_type() will recalculate this if we null it
  r->headers_out.content_type_lowcase = NULL;

  // Cache control
  char* cache_control_s = string_piece_to_pool_string(r->pool, cache_header);
  if (cache_control_s == NULL) {
    return NGX_ERROR;
  }
  ps_set_cache_control(r, cache_control_s);

  if (net_instaweb::FindIgnoreCase(cache_header, "private") ==
      static_cast<int>(StringPiece::npos)) {
    ngx_table_elt_t* etag = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&r->headers_out.headers));
    if (etag == NULL) {
      return NGX_ERROR;
    }

    etag->hash = 1;  // Include this header in the output.
    etag->key.len = 4;
    etag->key.data = reinterpret_cast<u_char*>(const_cast<char*>("ETag"));
    etag->value.len = 5;
    etag->value.data = reinterpret_cast<u_char*>(const_cast<char*>("W/\"0\""));
    r->headers_out.etag = etag;
  }

  ngx_http_send_header(r);

  // Send the body.
  ngx_chain_t* out;
  ngx_int_t rc = string_piece_to_buffer_chain(
      r->pool, file_contents, &out, true /* send_last_buf */);
  if (rc == NGX_ERROR) {
    return NGX_ERROR;
  }
  CHECK(rc == NGX_OK);

  return ngx_http_output_filter(r, out);
}

ngx_int_t send_out_headers_and_body(
    ngx_http_request_t* r,
    const net_instaweb::ResponseHeaders& response_headers,
    const GoogleString& output) {
  ngx_int_t rc = copy_response_headers_to_ngx(r, response_headers);

  if (rc != NGX_OK) {
    return NGX_ERROR;
  }

  rc = ngx_http_send_header(r);

  if (rc != NGX_OK) {
    return NGX_ERROR;
  }

  // Send the body.
  ngx_chain_t* out;
  rc = string_piece_to_buffer_chain(
      r->pool, output, &out, true /* send_last_buf */);
  if (rc == NGX_ERROR) {
    return NGX_ERROR;
  }
  CHECK(rc == NGX_OK);

  return ngx_http_output_filter(r, out);
}

// Write response headers and send out headers and output, including the option
// for a custom Content-Type.
void ps_write_handler_response(const StringPiece& output,
                            ngx_http_request_t* r,
                            net_instaweb::ContentType content_type,
                            const StringPiece& cache_control,
                            net_instaweb::Timer* timer) {
  net_instaweb::ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(net_instaweb::HttpStatus::kOK);
  response_headers.set_major_version(1);
  response_headers.set_minor_version(1);

  response_headers.Add(net_instaweb::HttpAttributes::kContentType,
                       content_type.mime_type());

  int64 now_ms = timer->NowMs();
  response_headers.SetDate(now_ms);
  response_headers.SetLastModified(now_ms);
  response_headers.Add(net_instaweb::HttpAttributes::kCacheControl,
                       cache_control);
  send_out_headers_and_body(r, response_headers, output.as_string());
}

// Writes text wrapped in a <pre> block
void ps_write_pre(StringPiece str, net_instaweb::Writer* writer,
              net_instaweb::MessageHandler* handler) {
  writer->Write("<pre>\n", handler);
  writer->Write(str, handler);
  writer->Write("</pre>\n", handler);
}

void ps_write_handler_response(const StringPiece& output,
                            ngx_http_request_t* r,
                            net_instaweb::ContentType content_type,
                            net_instaweb::Timer* timer) {
  ps_write_handler_response(output, r, net_instaweb::kContentTypeHtml,
                         net_instaweb::HttpAttributes::kNoCache, timer);
}

void ps_write_handler_response(const StringPiece& output, ngx_http_request_t* r,
                            net_instaweb::Timer* timer) {
  ps_write_handler_response(output, r, net_instaweb::kContentTypeHtml, timer);
}

// TODO(oschaaf): port SPDY specific functionality, shmcache stats
// TODO(oschaaf): refactor this with the apache code to share this code
ngx_int_t ps_statistics_handler(
    ngx_http_request_t* r,
    net_instaweb::NgxServerContext* server_context) {

  StringPiece request_uri_path = str_to_string_piece(r->uri);
  bool general_stats_request = net_instaweb::StringCaseStartsWith(
      request_uri_path, "/ngx_pagespeed_statistics");
  bool global_stats_request =
      net_instaweb::StringCaseStartsWith(
          request_uri_path, "/ngx_pagespeed_global_statistics");
  net_instaweb::NgxRewriteDriverFactory* factory =
      static_cast<net_instaweb::NgxRewriteDriverFactory*>(
          server_context->factory());
  net_instaweb::MessageHandler* message_handler = factory->message_handler();

  int64 start_time, end_time, granularity_ms;
  std::set<GoogleString> var_titles;
  std::set<GoogleString> hist_titles;
  if (general_stats_request && !factory->use_per_vhost_statistics()) {
    global_stats_request = true;
  }

  // Choose the correct statistics.
  net_instaweb::Statistics* statistics = global_stats_request ?
      factory->statistics() : server_context->statistics();

  net_instaweb::QueryParams params;
  StringPiece query_string = StringPiece(
      reinterpret_cast<char*>(r->args.data), r->args.len);
  params.Parse(query_string);

  // Parse various mode query params.
  bool print_normal_config = params.Has("config");

  // JSON statistics handling is done only if we have a console logger.
  bool json = false;
  if (statistics->console_logger() != NULL) {
    // Default values for start_time, end_time, and granularity_ms in case the
    // query does not include these parameters.
    start_time = 0;
    end_time = server_context->timer()->NowMs();
    // Granularity is the difference in ms between data points. If it is not
    // specified by the query, the default value is 3000 ms, the same as the
    // default logging granularity.
    granularity_ms = 3000;
    for (int i = 0; i < params.size(); ++i) {
      const GoogleString value =
          (params.value(i) == NULL) ? "" : *params.value(i);
      const char* name = params.name(i);
      if (strcmp(name, "json") == 0) {
        json = true;
      } else if (strcmp(name, "start_time") == 0) {
        net_instaweb::StringToInt64(value, &start_time);
      } else if (strcmp(name, "end_time") == 0) {
        net_instaweb::StringToInt64(value, &end_time);
      } else if (strcmp(name, "var_titles") == 0) {
        std::vector<StringPiece> variable_names;
        net_instaweb::SplitStringPieceToVector(
            value, ",", &variable_names, true);
        for (size_t i = 0; i < variable_names.size(); ++i) {
          var_titles.insert(variable_names[i].as_string());
        }
      } else if (strcmp(name, "hist_titles") == 0) {
        std::vector<StringPiece> histogram_names;
        net_instaweb::SplitStringPieceToVector(
            value, ",", &histogram_names, true);
        for (size_t i = 0; i < histogram_names.size(); ++i) {
          // TODO(morlovich): Cleanup & publicize UrlToFileNameEncoder::Unescape
          // and use it here, instead of this GlobalReplaceSubstring hack.
          GoogleString name = histogram_names[i].as_string();
          net_instaweb::GlobalReplaceSubstring("%20", " ", &(name));
          hist_titles.insert(name);
        }
      } else if (strcmp(name, "granularity") == 0) {
        net_instaweb::StringToInt64(value, &granularity_ms);
      }
    }
  }
  GoogleString output;
  net_instaweb::StringWriter writer(&output);
  if (json) {
    statistics->console_logger()->DumpJSON(var_titles, hist_titles,
                                           start_time, end_time,
                                           granularity_ms, &writer,
                                           message_handler);
  } else {
    // Generate some navigational links to the right to help
    // our users get to other modes.
    writer.Write(
                "<div style='float:right'>View "
                        "<a href='?config'>Configuration</a>, "
                        "<a href='?'>Statistics</a> "
                        "(<a href='?memcached'>with memcached Stats</a>). "
                "</div>",
                message_handler);

    // Only print stats or configuration, not both.
    if (!print_normal_config) {
      writer.Write(global_stats_request ?
                   "Global Statistics" : "VHost-Specific Statistics",
                   message_handler);

      // TODO(oschaaf): for when refactoring this with the apache code,
      // this note is a reminder that this is different in nginx:
      // we prepend the host identifier here
      if (!global_stats_request) {
        writer.Write(
            net_instaweb::StrCat("[",
                                 server_context->hostname_identifier(), "]"),
            message_handler);
      }

      // Write <pre></pre> for Dump to keep good format.
      writer.Write("<pre>", message_handler);
      statistics->Dump(&writer, message_handler);
      writer.Write("</pre>", message_handler);
      statistics->RenderHistograms(&writer, message_handler);

      if (params.Has("memcached")) {
        GoogleString memcached_stats;
        factory->PrintMemCacheStats(&memcached_stats);
        if (!memcached_stats.empty()) {
          ps_write_pre(memcached_stats, &writer, message_handler);
        }
      }
    }

    if (print_normal_config) {
      writer.Write("Configuration:<br>", message_handler);
      ps_write_pre(server_context->config()->OptionsToString(),
               &writer, message_handler);
    }
  }

  if (json) {
    ps_write_handler_response(output, r, net_instaweb::kContentTypeJson,
                           factory->timer());
  } else {
    ps_write_handler_response(output, r, factory->timer());
  }

  return NGX_OK;
}

ngx_int_t ps_messages_handler(
    ngx_http_request_t* r,
    net_instaweb::NgxServerContext* server_context) {
  GoogleString output;
  net_instaweb::StringWriter writer(&output);
  net_instaweb::NgxRewriteDriverFactory* factory =
      server_context->ngx_rewrite_driver_factory();
  net_instaweb::NgxMessageHandler* message_handler =
      factory->ngx_message_handler();
  // Write <pre></pre> for Dump to keep good format.
  writer.Write("<pre>", message_handler);
  if (!message_handler->Dump(&writer)) {
    writer.Write("Writing to ngx_pagespeed_message failed. \n"
                 "Please check if it's enabled in pagespeed.conf.\n",
                 message_handler);
  }
  writer.Write("</pre>", message_handler);
  ps_write_handler_response(output, r, factory->timer());
  return NGX_OK;
}

void ps_beacon_handler_helper(ngx_http_request_t* r,
                              StringPiece beacon_data) {
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "ps_beacon_handler_helper: beacon[%d] %*s",
                beacon_data.size(),  beacon_data.size(),
                beacon_data.data());

  StringPiece user_agent;
  if (r->headers_in.user_agent != NULL) {
    user_agent = str_to_string_piece(r->headers_in.user_agent->value);
  }

  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  CHECK(cfg_s != NULL);

  cfg_s->server_context->HandleBeacon(
      beacon_data,
      user_agent,
      net_instaweb::RequestContextPtr(new net_instaweb::NgxRequestContext(
          cfg_s->server_context->thread_system()->NewMutex(),
          cfg_s->server_context->timer(), r)));

  ps_set_cache_control(r, const_cast<char*>("max-age=0, no-cache"));

  // TODO(jefftk): figure out how to insert Content-Length:0 as a response
  // header so wget doesn't hang.
}


// Load the request body into out.  ngx_http_read_client_request_body must
// already have been called.  Return false on failure, true on success.
bool ps_request_body_to_string_piece(
    ngx_http_request_t* r, StringPiece* out) {
  if (r->request_body == NULL || r->request_body->bufs == NULL) {
    ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                  "ps_request_body_to_string_piece: "
                  "empty request body.");
    return false;
  }

  if (r->request_body->temp_file) {
    // For now raise an error instead of figuring out how to read temporary
    // files.
    ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                  "ps_request_body_to_string_piece: "
                  "request body in temporary file unsupported."
                  "Increase client_body_buffer_size.");
    return false;
  } else if (r->request_body->bufs->next == NULL) {
    // There's just one buffer, so we can simply return a StringPiece pointing
    // to this buffer.
    ngx_buf_t* buffer = r->request_body->bufs->buf;
    CHECK(!buffer->in_file);
    int len = buffer->last - buffer->pos;
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                  "ngx_pagespeed beacon: single buffer of %d", len);
    *out = StringPiece(reinterpret_cast<char*>(buffer->pos), len);
    return true;
  } else {
    // There are multiple buffers, so we need to allocate memory for a string to
    // hold the whole result.  This should only happen when the POST is sent
    // with "Transfer-Encoding: Chunked".

    // First determine how much data there is.
    int len = 0;
    int buffers = 0;

    ngx_chain_t* chain_link;
    for (chain_link = r->request_body->bufs;
         chain_link != NULL;
         chain_link = chain_link->next) {
      len += chain_link->buf->last - chain_link->buf->pos;
      buffers++;
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                  "ngx_pagespeed beacon: %d buffers totalling %d", len);

    // Allocate a string to store the combined result.
    u_char* s = static_cast<u_char*>(ngx_palloc(r->pool, len));
    if (s == NULL) {
      ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                    "ps_request_body_to_string_piece: "
                    "failed to allocate memory");
      return false;
    }

    // Copy the data into the combined string.
    u_char* current_position = s;
    int i;
    for (chain_link = r->request_body->bufs, i = 0;
         chain_link != NULL;
         chain_link = chain_link->next, i++) {
      ngx_buf_t* buffer = chain_link->buf;
      CHECK(!buffer->in_file);
      current_position = ngx_copy(current_position, buffer->pos,
                                  buffer->last - buffer->pos);
    }
    CHECK_EQ(current_position, s + len);
    *out = StringPiece(reinterpret_cast<char*>(s), len);
    return true;
  }
}

// Called after nginx reads the request body from the client.  For another
// example processing request buffers, see ngx_http_form_input_module.c
void ps_beacon_body_handler(ngx_http_request_t* r) {
  StringPiece request_body;
  bool ok = ps_request_body_to_string_piece(r, &request_body);
  if (ok) {
    ps_beacon_handler_helper(r, request_body);
    ngx_http_finalize_request(r, NGX_HTTP_NO_CONTENT);
  } else {
    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  }
}

ngx_int_t ps_beacon_handler(ngx_http_request_t* r) {
  if (r->method == NGX_HTTP_POST) {
    // Use post body. Handler functions are called before the request body has
    // been read from the client, so we need to ask nginx to read it from the
    // client and then call us back.  Control flow continues in
    // ps_beacon_body_handler unless there's an error reading the request body.
    //
    // See: http://forum.nginx.org/read.php?2,31312,31312
    ngx_int_t rc = ngx_http_read_client_request_body(r, ps_beacon_body_handler);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
      return rc;
    }
    return NGX_DONE;
  } else {
    // Use query params.
    StringPiece beacon_data;
    StringPiece unparsed_uri = str_to_string_piece(r->unparsed_uri);
    stringpiece_ssize_type question_mark_index = unparsed_uri.find("?");
    if (question_mark_index == StringPiece::npos) {
      beacon_data = "";
    } else {
      beacon_data = unparsed_uri.substr(
          question_mark_index+1, unparsed_uri.size() - (question_mark_index+1));
    }
    ps_beacon_handler_helper(r, beacon_data);
    return NGX_HTTP_NO_CONTENT;
  }
}

// Handle requests for resources like example.css.pagespeed.ce.LyfcM6Wulf.css
// and for static content like /ngx_pagespeed_static/js_defer.q1EBmcgYOC.js
ngx_int_t ps_content_handler(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return NGX_DECLINED;
  }

  // Poll for cache flush on every request (polls are rate-limited).
  cfg_s->server_context->FlushCacheIfNecessary();

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed handler \"%V\"", &r->uri);

  switch (ps_create_request_context(
      r, true /* is a resource fetch */)) {
    case CreateRequestContext::kError:
      return NGX_ERROR;
    case CreateRequestContext::kNotUnderstood:
    case CreateRequestContext::kPagespeedDisabled:
    case CreateRequestContext::kInvalidUrl:
    case CreateRequestContext::kPagespeedSubrequest:
    case CreateRequestContext::kNotHeadOrGet:
    case CreateRequestContext::kErrorResponse:
      return NGX_DECLINED;
    case CreateRequestContext::kBeacon:
      return ps_beacon_handler(r);
    case CreateRequestContext::kStaticContent:
      return ps_static_handler(r);
    case CreateRequestContext::kStatistics:
      return ps_statistics_handler(r, cfg_s->server_context);
    case CreateRequestContext::kMessages:
      return ps_messages_handler(r, cfg_s->server_context);
    case CreateRequestContext::kOk:
      break;
  }

  ps_request_ctx_t* ctx = ps_get_request_context(r);
  CHECK(ctx != NULL);

  // Tell nginx we're still working on this one.
  r->count++;
  return NGX_DONE;
}

ngx_int_t ps_phase_handler(ngx_http_request_t *r,
      ngx_http_phase_handler_t *ph) {
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "pagespeed phase: %ui", r->phase_handler);

  r->write_event_handler = ngx_http_request_empty_handler;

  ngx_int_t rc = ps_content_handler(r);
  // Warning: this requires ps_content_handler to always return NGX_DECLINED
  // directly if it's not going to handle the request. It is not ok for
  // ps_content_handler to asynchronously determine whether to handle the
  // request, returning NGX_DONE here.
  if (rc == NGX_DECLINED) {
    r->write_event_handler = ngx_http_core_run_phases;
    r->phase_handler++;
    return NGX_AGAIN;
  }

  ngx_http_finalize_request(r, rc);
  return NGX_OK;
}

// preaccess_handler should be at generic phase before try_files
ngx_int_t ps_preaccess_handler(ngx_http_request_t *r) {
  ngx_http_core_main_conf_t *cmcf;
  ngx_http_phase_handler_t *ph;
  ngx_uint_t i;

  cmcf = static_cast<ngx_http_core_main_conf_t *>(
                    ngx_http_get_module_main_conf(r, ngx_http_core_module));

  ph = cmcf->phase_engine.handlers;

  i = r->phase_handler;
  // move handlers before try_files && content phase
  while (ph[i + 1].checker != ngx_http_core_try_files_phase
      && ph[i + 1].checker != ngx_http_core_content_phase) {
    ph[i] = ph[i + 1];
    ph[i].next--;
    i++;
  }

  // insert ps phase handler
  ph[i].checker = ps_phase_handler;
  ph[i].handler = NULL;
  ph[i].next = i + 1;

  // next preaccess handler
  r->phase_handler--;
  return NGX_DECLINED;
}

ngx_int_t ps_etag_filter_init(ngx_conf_t* cf) {
  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_conf_get_module_main_conf(cf, ngx_pagespeed));
  if (cfg_m->driver_factory != NULL) {
    ngx_http_ef_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ps_etag_header_filter;
  }
  return NGX_OK;
}

ngx_int_t ps_init(ngx_conf_t* cf) {
  // Only put register pagespeed code to run if there was a "pagespeed"
  // configuration option set in the config file.  With "pagespeed off" we
  // consider every request and choose not to do anything, while with no
  // "pagespeed" directives we won't have any effect after nginx is done loading
  // its configuration.

  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_conf_get_module_main_conf(cf, ngx_pagespeed));

  // The driver factory is on the main config and is non-NULL iff there is a
  // pagespeed configuration option in the main config or a server block.  Note
  // that if any server block has pagespeed 'on' then our header filter, body
  // filter, and content handler will run in every server block.  This is ok,
  // because they will notice that the server context is NULL and do nothing.
  if (cfg_m->driver_factory != NULL) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ps_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ps_body_filter;

    ngx_http_core_main_conf_t* cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));

    ngx_http_handler_pt* h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers));
    if (h == NULL) {
      return NGX_ERROR;
    }
    *h = ps_preaccess_handler;
  }

  return NGX_OK;
}

ngx_http_module_t ps_etag_filter_module = {
  NULL,  // preconfiguration
  ps_etag_filter_init,  // postconfiguration
  NULL,
  NULL,  // initialize main configuration
  NULL,
  NULL,
  NULL,
  NULL
};

ngx_http_module_t ps_module = {
  NULL,  // preconfiguration
  ps_init,  // postconfiguration

  ps_create_main_conf,
  NULL,  // initialize main configuration

  ps_create_srv_conf,
  ps_merge_srv_conf,

  ps_create_loc_conf,
  ps_merge_loc_conf
};

// called after configuration is complete, but before nginx starts forking
ngx_int_t ps_init_module(ngx_cycle_t* cycle) {
  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_pagespeed));

  ngx_http_core_main_conf_t* cmcf = static_cast<ngx_http_core_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module));
  ngx_http_core_srv_conf_t** cscfp = static_cast<ngx_http_core_srv_conf_t**>(
      cmcf->servers.elts);
  ngx_uint_t s;

  bool have_server_context = false;
  net_instaweb::Statistics* statistics = NULL;
  // Iterate over all configured server{} blocks, and find out if we have
  // an enabled ServerContext.
  for (s = 0; s < cmcf->servers.nelts; s++) {
    ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
        cscfp[s]->ctx->srv_conf[ngx_pagespeed.ctx_index]);
    if (cfg_s->server_context != NULL) {
      have_server_context = true;

      net_instaweb::NgxRewriteOptions* config = cfg_s->server_context->config();
      // Lazily create shared-memory statistics if enabled in any
      // config, even when ngx_pagespeed is totally disabled.  This
      // allows statistics to work if ngx_pagespeed gets turned on via
      // .htaccess or query param.
      if ((statistics == NULL) && config->statistics_enabled()) {
        statistics = cfg_m->driver_factory->MakeGlobalSharedMemStatistics(
            config->statistics_logging_enabled(),
            config->statistics_logging_interval_ms(),
            config->statistics_logging_file());
      }

      // The hostname identifier is used by the shared memory statistics
      // to allocate a segment, and should be unique name per server
      GoogleString hostname_identifier = net_instaweb::StrCat(
          "Host[", base::IntToString(static_cast<int>(s)), "]");
      cfg_s->server_context->set_hostname_identifier(hostname_identifier);

      // If config has statistics on and we have per-vhost statistics on
      // as well, then set it up.
      if (config->statistics_enabled()
          && cfg_m->driver_factory->use_per_vhost_statistics()) {
        cfg_s->server_context->CreateLocalStatistics(statistics);
      }
    }
  }

  if (have_server_context) {
    // TODO(oschaaf): this ignores sigpipe messages from memcached.
    // however, it would be better to not have those signals generated
    // in the first place, as suppressing them this way may interfere
    // with other modules that actually are interested in these signals
    ps_ignore_sigpipe();

    // If no shared-mem statistics are enabled, then init using the default
    // NullStatistics.
    if (statistics == NULL) {
      statistics = cfg_m->driver_factory->statistics();
      net_instaweb::NgxRewriteDriverFactory::InitStats(statistics);
    }

    ngx_http_core_loc_conf_t* clcf = static_cast<ngx_http_core_loc_conf_t*>(
        ngx_http_conf_get_module_loc_conf((*cscfp), ngx_http_core_module));

    cfg_m->driver_factory->set_resolver(clcf->resolver);
    cfg_m->driver_factory->set_resolver_timeout(clcf->resolver_timeout);

    if (!cfg_m->driver_factory->CheckResolver()) {
      cfg_m->handler->Message(
          net_instaweb::kError,
          "UseNativeFetcher is on, please configure a resolver.");
      return NGX_ERROR;
    }

    cfg_m->driver_factory->RootInit(cycle->log);
  } else {
    delete cfg_m->driver_factory;
    cfg_m->driver_factory = NULL;
  }
  return NGX_OK;
}

// Called when nginx forks worker processes.  No threads should be started
// before this.
ngx_int_t ps_init_child_process(ngx_cycle_t* cycle) {
  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_pagespeed));
  if (cfg_m->driver_factory == NULL) {
    return NGX_OK;
  }

  // ChildInit() will initialise all ServerContexts, which we need to
  // create ProxyFetchFactories below
  cfg_m->driver_factory->ChildInit(cycle->log);

  ngx_http_core_main_conf_t* cmcf = static_cast<ngx_http_core_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module));
  ngx_http_core_srv_conf_t** cscfp = static_cast<ngx_http_core_srv_conf_t**>(
      cmcf->servers.elts);
  ngx_uint_t s;

  // Iterate over all configured server{} blocks, and find our context in it,
  // so we can create and set a ProxyFetchFactory for it.
  for (s = 0; s < cmcf->servers.nelts; s++) {
    ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
        cscfp[s]->ctx->srv_conf[ngx_pagespeed.ctx_index]);
    // Some server{} blocks may not have a ServerContext in that case we must
    // not instantiate a ProxyFetchFactory.
    if (cfg_s->server_context != NULL) {
      cfg_s->proxy_fetch_factory =
          new net_instaweb::ProxyFetchFactory(cfg_s->server_context);
    }
  }

  if (!cfg_m->driver_factory->InitNgxUrlAsyncFetcher()) {
    return NGX_ERROR;
  }
  cfg_m->driver_factory->StartThreads();

  return NGX_OK;
}

}  // namespace

}  // namespace ngx_psol

ngx_module_t ngx_pagespeed_etag_filter = {
  NGX_MODULE_V1,
  &ngx_psol::ps_etag_filter_module,
  NULL,
  NGX_HTTP_MODULE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};

ngx_module_t ngx_pagespeed = {
  NGX_MODULE_V1,
  &ngx_psol::ps_module,
  ngx_psol::ps_commands,
  NGX_HTTP_MODULE,
  NULL,
  ngx_psol::ps_init_module,
  ngx_psol::ps_init_child_process,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};
