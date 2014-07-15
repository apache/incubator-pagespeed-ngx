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
#include "ngx_caching_headers.h"
#include "ngx_list_iterator.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_driver_factory.h"
#include "ngx_rewrite_options.h"
#include "ngx_server_context.h"

#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/in_place_resource_recorder.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_request_context.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/system/public/system_thread_system.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/statistics_logger.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/stack_buffer.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"

extern ngx_module_t ngx_pagespeed;

// Hacks for debugging.
#define DBG(r, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (r)->connection->log, 0, args)
#define PDBG(ctx, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (ctx)->r->connection->log, 0, args)
#define CDBG(cf, args...)                                     \
  ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, args)

// Unused flag, see
// http://lxr.evanmiller.org/http/source/http/ngx_http_request.h#L130
#define  NGX_HTTP_PAGESPEED_BUFFERED 0x08

// Needed for SystemRewriteDriverFactory to use shared memory.
#define PAGESPEED_SUPPORT_POSIX_SHARED_MEM

namespace net_instaweb {

const char* kInternalEtagName = "@psol-etag";
// The process context takes care of proactively initialising
// a few libraries for us, some of which are not thread-safe
// when they are initialized lazily.
ProcessContext* process_context = new ProcessContext();
bool process_context_cleanup_hooked = false;

StringPiece str_to_string_piece(ngx_str_t s) {
  return StringPiece(reinterpret_cast<char*>(s.data), s.len);
}

char* string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp) {
  // Need space for the final null.
  ngx_uint_t buffer_size = sp.size() + 1;
  char* s = static_cast<char*>(ngx_palloc(pool, buffer_size));
  if (s == NULL) {
    LOG(ERROR) << "string_piece_to_pool_string: ngx_palloc() returned NULL";
    DCHECK(false);
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

  // Other modules seem to default to ngx_pagesize.
  ngx_uint_t max_buffer_size = ngx_pagesize;
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

// modified from NgxBaseFetch::CopyHeadersFromTable()
namespace {

// Based on ngx_http_add_cache_control.
ngx_int_t ps_set_cache_control(ngx_http_request_t* r, char* cache_control) {
  // First strip existing cache-control headers.
  ngx_table_elt_t* header;
  NgxListIterator it(&(r->headers_out.headers.part));
  while ((header = it.Next()) != NULL) {
    if (STR_CASE_EQ_LITERAL(header->key, "Cache-Control")) {
      // Response headers with hash of 0 are excluded from the response.
      header->hash = 0;
    }
  }

  // Now add our new cache control header.
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

template<class Headers>
void copy_headers_from_table(const ngx_list_t &from, Headers* to) {
  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  const ngx_list_part_t* part = &from.part;
  const ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }
    // Make sure we don't copy over headers that are unset.
    if (header[i].hash == 0) {
      continue;
    }
    StringPiece key = str_to_string_piece(header[i].key);
    StringPiece value = str_to_string_piece(header[i].value);

    to->Add(key, value);
  }
}
}  // namespace

void copy_response_headers_from_ngx(const ngx_http_request_t* r,
                                    ResponseHeaders* headers) {
  headers->set_major_version(r->http_version / 1000);
  headers->set_minor_version(r->http_version % 1000);
  copy_headers_from_table(r->headers_out.headers, headers);

  headers->set_status_code(r->headers_out.status);

  // Manually copy over the content type because it's not included in
  // request_->headers_out.headers.
  headers->Add(HttpAttributes::kContentType,
               str_to_string_piece(r->headers_out.content_type));

  // When we don't have a date header, set one with the current time.
  if (headers->Lookup1(HttpAttributes::kDate) == NULL) {
    headers->SetDate(ngx_current_msec);
  }

  // TODO(oschaaf): ComputeCaching should be called in setupforhtml()?
  headers->ComputeCaching();
}

void copy_request_headers_from_ngx(const ngx_http_request_t* r,
                                   RequestHeaders* headers) {
  // TODO(chaizhenhua): only allow RewriteDriver::kPassThroughRequestAttributes?
  headers->set_major_version(r->http_version / 1000);
  headers->set_minor_version(r->http_version % 1000);
  copy_headers_from_table(r->headers_in.headers, headers);
}

// PSOL produces caching headers that need some changes before we can send them
// out.  Make those changes and populate r->headers_out from pagespeed_headers.
ngx_int_t copy_response_headers_to_ngx(
    ngx_http_request_t* r,
    const ResponseHeaders& pagespeed_headers,
    PreserveCachingHeaders preserve_caching_headers) {
  ngx_http_headers_out_t* headers_out = &r->headers_out;
  headers_out->status = pagespeed_headers.status_code();

  ngx_int_t i;
  for (i = 0 ; i < pagespeed_headers.NumAttributes() ; i++) {
    const GoogleString& name_gs = pagespeed_headers.Name(i);
    const GoogleString& value_gs = pagespeed_headers.Value(i);

    if (preserve_caching_headers == kPreserveAllCachingHeaders) {
      if (StringCaseEqual(name_gs, "ETag") ||
          StringCaseEqual(name_gs, "Expires") ||
          StringCaseEqual(name_gs, "Date") ||
          StringCaseEqual(name_gs, "Last-Modified") ||
          StringCaseEqual(name_gs, "Cache-Control")) {
        continue;
      }
    } else if (preserve_caching_headers == kPreserveOnlyCacheControl) {
      // Retain the original Cache-Control header, but send the recomputed
      // values for all other cache-related headers.
      if (StringCaseEqual(name_gs, "Cache-Control")) {
        continue;
      }
    } // else we don't preserve any headers

    ngx_str_t name, value;

    // To prevent the gzip module from clearing weak etags, we output them
    // using a different name here. The etag header filter module runs behind
    // the gzip compressors header filter, and will rename it to 'ETag'
    if (StringCaseEqual(name_gs, "etag")
        && StringCaseStartsWith(value_gs, "W/")) {
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

    if (STR_EQ_LITERAL(name, "Cache-Control")) {
      ps_set_cache_control(r, const_cast<char*>(value_gs.c_str()));
      continue;
    } else if (STR_EQ_LITERAL(name, "Content-Type")) {
      // Unlike all the other headers, content_type is just a string.
      headers_out->content_type.data = value_s;
      headers_out->content_type.len = value.len;

      // We should not include the charset when determining content_type_len, so
      // scan for the ';' that marks the start of the charset part.
      for (ngx_uint_t i = 0; i < value.len; i++) {
        if (value_s[i] == ';') {
          break;
        }
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
    } else if (STR_EQ_LITERAL(name, "Content-Length")) {
      int64 len;
      CHECK(pagespeed_headers.FindContentLength(&len));
      headers_out->content_length_n = len;
      headers_out->content_length = header;
    }
  }

  return NGX_OK;
}

namespace {

typedef struct {
  NgxRewriteDriverFactory* driver_factory;
  MessageHandler* handler;
} ps_main_conf_t;

typedef struct {
  // If pagespeed is configured in some server block but not this one our
  // per-request code will be invoked but server context will be null.  In those
  // cases we neet to short circuit, not changing anything.  Currently our
  // header filter, body filter, and content handler all do this, but if anyone
  // adds another way for nginx to give us a request to process we need to check
  // there as well.
  NgxServerContext* server_context;
  ProxyFetchFactory* proxy_fetch_factory;
  // Only used while parsing config.  After we merge cfg_s and cfg_m you most
  // likely want cfg_s->server_context->config() as options here will be NULL.
  NgxRewriteOptions* options;
  MessageHandler* handler;
} ps_srv_conf_t;

typedef struct {
  NgxRewriteOptions* options;
  MessageHandler* handler;
} ps_loc_conf_t;

namespace RequestRouting {
enum Response {
  kError,
  kNotUnderstood,
  kStaticContent,
  kInvalidUrl,
  kPagespeedDisabled,
  kBeacon,
  kStatistics,
  kGlobalStatistics,
  kConsole,
  kMessages,
  kAdmin,
  kGlobalAdmin,
  kPagespeedSubrequest,
  kNotHeadOrGet,
  kErrorResponse,
  kResource,
};
}  // namespace RequestRouting

char* ps_main_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
char* ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
char* ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

// TODO(jud): Verify that all the offsets should be NGX_HTTP_SRV_CONF_OFFSET and
// not NGX_HTTP_LOC_CONF_OFFSET or NGX_HTTP_MAIN_CONF_OFFSET.
ngx_command_t ps_commands[] = {
  { ngx_string("pagespeed"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1|
    NGX_CONF_TAKE2|NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5,
    ps_main_configure,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL },
  { ngx_string("pagespeed"),
    NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1|
    NGX_CONF_TAKE2|NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5,
    ps_srv_configure,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL },

  { ngx_string("pagespeed"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1|
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

char* ps_init_dir(const StringPiece& directive,
                  const StringPiece& path,
                  ngx_conf_t* cf) {
  if (path.size() == 0 || path[0] != '/') {
    return string_piece_to_pool_string(
        cf->pool, net_instaweb::StrCat(directive, " ", path,
                                       " must start with a slash"));
  }

  net_instaweb::StdioFileSystem file_system;
  net_instaweb::NullMessageHandler message_handler;
  GoogleString gs_path;
  path.CopyToString(&gs_path);
  if (!file_system.IsDir(gs_path.c_str(), &message_handler).is_true()) {
    if (!file_system.RecursivelyMakeDir(path, &message_handler)) {
      return string_piece_to_pool_string(
          cf->pool, net_instaweb::StrCat(
              directive, " path ", path,
              " does not exist and could not be created."));
    }
    // Directory created, but may not be readable by the worker processes.
  }

  if (geteuid() != 0) {
    return NULL;  // We're not root, so we're staying whoever we are.
  }

  // chown if owner differs from nginx worker user.
  ngx_core_conf_t* ccf = reinterpret_cast<ngx_core_conf_t*>(
      ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module));
  CHECK(ccf != NULL);
  struct stat gs_stat;
  if (stat(gs_path.c_str(), &gs_stat) != 0) {
    return string_piece_to_pool_string(
        cf->pool, net_instaweb::StrCat(
            directive, " ", path, " stat() failed"));
  }
  if (gs_stat.st_uid != ccf->user) {
    if (chown(gs_path.c_str(), ccf->user, ccf->group) != 0) {
      return string_piece_to_pool_string(
          cf->pool, net_instaweb::StrCat(
              directive, " ", path, " unable to set permissions"));
    }
  }

  return NULL;
}

ngx_int_t ps_dollar(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
  v->valid = 1;
  v->no_cacheable = 0;
  v->not_found = 0;
  v->data = reinterpret_cast<u_char*>(const_cast<char*>("$"));
  v->len = 1;
  return NGX_OK;
}

char* ps_configure(ngx_conf_t* cf,
                   NgxRewriteOptions** options,
                   MessageHandler* handler,
                   net_instaweb::RewriteOptions::OptionScope option_scope) {
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

  // Some options require the worker process to be able to read and write to
  // a specific directory.  Generally the master process is root while the
  // worker is nobody, so we need to change permissions and create the directory
  // if necessary.
  if (n_args == 2 &&
      (net_instaweb::StringCaseEqual("LogDir", args[0]) ||
       net_instaweb::StringCaseEqual("FileCachePath", args[0]))) {
    char* error_message = ps_init_dir(args[0], args[1], cf);
    if (error_message != NULL) {
      return error_message;
    }
    // The directory has been prepared, but we haven't actually parsed the
    // directive yet.  That happens below in ParseAndSetOptions().
  }

  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cf->cycle, ngx_pagespeed));
  if (*options == NULL) {
    *options = new NgxRewriteOptions(
        cfg_m->driver_factory->thread_system());
  }

  bool process_script_variables = dynamic_cast<NgxRewriteDriverFactory*>(
      cfg_m->driver_factory)->process_script_variables();

  if (process_script_variables) {
    // To be able to use '$', we map '$ps_dollar' to '$' via a script variable.
    ngx_str_t name = ngx_string("ps_dollar");
    ngx_http_variable_t* var = ngx_http_add_variable(
        cf, &name, NGX_HTTP_VAR_CHANGEABLE);

    if (var == NULL) {
      return const_cast<char*>(
          "Failed to add global configuration variable for '$ps_dollar'");
    }
    var->get_handler = ps_dollar;
  }

  const char* status = (*options)->ParseAndSetOptions(
      args, n_args, cf->pool, handler, cfg_m->driver_factory, option_scope, cf,
      process_script_variables);

  // nginx expects us to return a string literal but doesn't mark it const.
  return const_cast<char*>(status);
}

char* ps_main_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));
  return ps_configure(cf, &cfg_s->options, cfg_s->handler,
                      net_instaweb::RewriteOptions::kProcessScopeStrict);
}

char* ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));
  return ps_configure(cf, &cfg_s->options, cfg_s->handler,
                      net_instaweb::RewriteOptions::kServerScope);
}

char* ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(
      ngx_http_conf_get_module_loc_conf(cf, ngx_pagespeed));

  return ps_configure(
      cf, &cfg_l->options, cfg_l->handler,
      net_instaweb::RewriteOptions::kDirectoryScope);
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
  NgxRewriteDriverFactory::Terminate();
  NgxRewriteOptions::Terminate();

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
  cfg->handler = new GoogleMessageHandler();
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

void terminate_process_context() {
  delete process_context;
  process_context = NULL;
}

void* ps_create_main_conf(ngx_conf_t* cf) {
  if (!process_context_cleanup_hooked) {
    atexit(terminate_process_context);
    process_context_cleanup_hooked = true;
  }
  ps_main_conf_t* cfg_m = ps_create_conf<ps_main_conf_t>(cf);
  if (cfg_m == NULL) {
    return NGX_CONF_ERROR;
  }
  CHECK(!factory_deleted);
  NgxRewriteOptions::Initialize();
  NgxRewriteDriverFactory::Initialize();

  cfg_m->driver_factory = new NgxRewriteDriverFactory(
      *process_context,
      new SystemThreadSystem(),
      "" /* hostname, not used */,
      -1 /* port, not used */);
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
void ps_merge_options(NgxRewriteOptions* parent_options,
                      NgxRewriteOptions** child_options) {
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
    NgxRewriteOptions* child_specific_options = *child_options;
    *child_options = parent_options->Clone();
    (*child_options)->Merge(*child_specific_options);

    if (child_specific_options->clear_inherited_scripts()) {
      // We don't want to inherit any inherited script lines from the parent
      // options here, so we just stick to the child specific ones.
      child_specific_options->CopyScriptLinesTo(*child_options);
    } else {
      // We append the child specific script lines to the parent's script lines
      // so we preserve the order in which they will be executed at request time
      child_specific_options->AppendScriptLinesTo(*child_options);
    }
    delete child_specific_options;
  }
}

namespace {

int times_ps_merge_srv_conf_called = 0;

}  // namespace

// Called exactly once per server block to merge the main configuration with the
// configuration for this server.
char* ps_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child) {
  times_ps_merge_srv_conf_called += 1;

  ps_srv_conf_t* parent_cfg_s = static_cast<ps_srv_conf_t*>(parent);
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(child);

  ps_merge_options(parent_cfg_s->options, &cfg_s->options);

  if (cfg_s->options == NULL) {
    return NGX_CONF_OK;  // No pagespeed options; don't do anything.
  }

  // ServerContext needs a hostname and port, but I don't see how to get this
  // and it ignores that a server can have multiple names and ports.  Because
  // the server context only needs them to make a unique identifier and to make
  // debugging easier, substitute our own unique identifier.
  // TODO(jefftk): either figure out how to get a hostname and port for this
  // server block or change ServerContext not to ask for them.
  int dummy_port = -times_ps_merge_srv_conf_called;

  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_conf_get_module_main_conf(cf, ngx_pagespeed));
  cfg_m->driver_factory->set_main_conf(parent_cfg_s->options);
  cfg_s->server_context = cfg_m->driver_factory->MakeNgxServerContext(
      "dummy_hostname", dummy_port);
  // The server context sets some options when we call global_options(). So
  // let it do that, then merge in options we got from the config file.
  // Once we do that we're done with cfg_s->options.
  cfg_s->server_context->global_options()->Merge(*cfg_s->options);
  NgxRewriteOptions* ngx_options = dynamic_cast<NgxRewriteOptions*>(
      cfg_s->server_context->global_options());
  cfg_s->options->CopyScriptLinesTo(ngx_options);
  delete cfg_s->options;
  cfg_s->options = NULL;

  if (cfg_s->server_context->global_options()->enabled()) {
    // Validate FileCachePath
    GoogleMessageHandler handler;
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
  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(child);
  if (cfg_l->options == NULL) {
    // No directory specific options.
    return NGX_CONF_OK;
  }

  // While you can't put a "location" block inside a "location" block you can
  // put an "if" block inside a "location" block, which is implemented by making
  // a pretend "location" block.  In this case we may have pagespeed options
  // from the parent "location" block as well as from the current locationish
  // "if" block.
  ps_loc_conf_t* parent_cfg_l = static_cast<ps_loc_conf_t*>(parent);
  if (parent_cfg_l->options != NULL) {
    // Rebase our options off of the ones defined in the parent location block.
    ps_merge_options(parent_cfg_l->options, &cfg_l->options);
    return NGX_CONF_OK;
  }

  // Pagespeed options are defined in this location block, and it either has no
  // parent (typical case) or is an if block whose parent location block defines
  // no pagespeed options.  Base our options off of those in the server block.

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
}  // namespace

StringPiece ps_determine_host(ngx_http_request_t* r) {
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
    host = str_to_string_piece(s);
  }
  return host;
}

namespace {

GoogleString ps_determine_url(ngx_http_request_t* r) {
  int port = ps_determine_port(r);
  GoogleString port_string;
  if ((ps_is_https(r) && (port == 443 || port == -1)) ||
      (!ps_is_https(r) && (port == 80 || port == -1))) {
    // No port specifier needed for requests on default ports.
    port_string = "";
  } else {
    port_string = StrCat(":", IntegerToString(port));
  }

  StringPiece host = ps_determine_host(r);

  return StrCat(ps_is_https(r) ? "https://" : "http://",
                host, port_string, str_to_string_piece(r->unparsed_uri));
}

// Get the context for this request.  ps_connection_read_handler should already
// have been called to create it.
ps_request_ctx_t* ps_get_request_context(ngx_http_request_t* r) {
  return static_cast<ps_request_ctx_t*>(
      ngx_http_get_module_ctx(r, ngx_pagespeed));
}

void ps_release_base_fetch(ps_request_ctx_t* ctx);

// we are still at pagespeed phase
ngx_int_t ps_decline_request(ngx_http_request_t* r) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);
  CHECK(ctx != NULL);

  // re init ctx
  ctx->fetch_done = false;
  ctx->write_pending = false;

  ps_release_base_fetch(ctx);
  ps_set_buffered(r, false);

  r->count++;
  r->phase_handler++;
  r->write_event_handler = ngx_http_core_run_phases;
  ngx_http_core_run_phases(r);
  ngx_http_run_posted_requests(r->connection);
  return NGX_DONE;
}

ngx_int_t ps_async_wait_response(ngx_http_request_t* r) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);
  CHECK(ctx != NULL);

  r->count++;
  r->write_event_handler = ngx_http_request_empty_handler;
  ps_set_buffered(r, true);
  // We don't need to add a timer here, as it will be set by nginx.
  return NGX_DONE;
}

namespace {

ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

ngx_int_t ps_base_fetch_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx == NULL || ctx->base_fetch == NULL) {
    return ngx_http_next_body_filter(r, in);
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed write filter \"%V\"", &r->uri);

  // send response body
  if (in || ctx->write_pending) {
    ngx_int_t rc = ngx_http_next_body_filter(r, in);
    ctx->write_pending = (rc == NGX_AGAIN);
    if (rc == NGX_OK && !ctx->fetch_done) {
      return NGX_AGAIN;
    }
    return rc;
  }

  return ctx->fetch_done ? NGX_OK : NGX_AGAIN;
}

ngx_int_t ps_base_fetch_handler(ngx_http_request_t* r) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);
  ngx_int_t rc;
  ngx_chain_t* cl = NULL;

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "ps fetch handler: %V", &r->uri);

  if (!r->header_sent) {
    if (ctx->preserve_caching_headers != kDontPreserveHeaders) {
      ngx_table_elt_t* header;
      NgxListIterator it(&(r->headers_out.headers.part));
      while ((header = it.Next()) != NULL) {
        // We need to remember a few headers when ModifyCachingHeaders is off,
        // so we can send them unmodified in copy_response_headers_to_ngx().
        // This just sets the hash to 0 for all other headers. That way, we
        // avoid  some relatively complicated code to reconstruct these headers.
        if (!(STR_CASE_EQ_LITERAL(header->key, "Cache-Control") ||
              (ctx->preserve_caching_headers == kPreserveAllCachingHeaders &&
               (STR_CASE_EQ_LITERAL(header->key, "Etag") ||
                STR_CASE_EQ_LITERAL(header->key, "Date") ||
                STR_CASE_EQ_LITERAL(header->key, "Last-Modified") ||
                STR_CASE_EQ_LITERAL(header->key, "Expires"))))) {
          header->hash = 0;
          if (STR_CASE_EQ_LITERAL(header->key, "Location")) {
            // There's a possible issue with the location header, where setting
            // the hash to 0 is not enough. See:
            // https://github.com/nginx/nginx/blob/master/src/http/ngx_http_header_filter_module.c#L314
            r->headers_out.location = NULL;
          }
        }
      }
    } else {
      ngx_http_clean_header(r);
    }
    // collect response headers from pagespeed
    rc = ctx->base_fetch->CollectHeaders(&r->headers_out);
    if (rc == NGX_ERROR) {
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // send response headers
    rc = ngx_http_next_header_filter(r);

    // standard nginx send header check see ngx_http_send_response
    if (rc == NGX_ERROR || rc > NGX_OK) {
      return ngx_http_filter_finalize_request(r, NULL, rc);
    }

    // for in_place_check_header_filter
    if (rc < NGX_OK && rc != NGX_AGAIN) {
      CHECK(rc == NGX_DONE);
      return rc;
    }

    ctx->write_pending = (rc == NGX_AGAIN);

    ps_set_buffered(r, true);
  }

  // collect response body from pagespeed
  // Pass the optimized content along to later body filters.
  // From Weibin: This function should be called mutiple times. Store the
  // whole file in one chain buffers is too aggressive. It could consume
  // too much memory in busy servers.

  rc = ctx->base_fetch->CollectAccumulatedWrites(&cl);
  PDBG(ctx, "CollectAccumulatedWrites, %d", rc);

  if (rc == NGX_ERROR) {
    ps_set_buffered(r, false);
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  if (rc == NGX_AGAIN && cl == NULL) {
    // there is no body buffer to send now.
    return NGX_AGAIN;
  }

  if (rc == NGX_OK) {
    ps_set_buffered(r, false);
    ctx->fetch_done = true;
  }

  return ps_base_fetch_filter(r, cl);
}

void ps_base_fetch_filter_init() {
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ps_base_fetch_filter;
}

}  // namespace

void ps_connection_read_handler(ngx_event_t* ev) {
  CHECK(ev != NULL);
  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  CHECK(c != NULL);

  int rc;

  // Request has been finalized, do nothing just clear the pipe.
  if (c->error) {
    do {
      char chr[256];
      rc = read(c->fd, chr, 256);
    } while (rc > 0 || (rc == -1 && errno == EINTR));  // Retry on EINTR.

    if (rc == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    // Write peer close or error occur.
    ngx_close_connection(c);
    return;
  }

  ps_request_ctx_t* ctx = static_cast<ps_request_ctx_t*>(c->data);
  CHECK(ctx != NULL);
  ngx_http_request_t* r = ctx->r;
  CHECK(r != NULL);

  // Clear the pipe.
  do {
    char chr[256];
    rc = read(c->fd, chr, 256);
  } while (rc > 0 || (rc == -1 && errno == EINTR));  // Retry on EINTR.

  if (rc == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ctx->pagespeed_connection = NULL;
    ngx_close_connection(c);
    return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  }

  // AGAIN or rc == 0.
  if (rc == 0) {
    // Close the pipe here to avoid SIGPIPE
    // Done will be check in RequestCollection.
    ctx->pagespeed_connection = NULL;
    ngx_close_connection(c);
  }

  if (ctx->fetch_done) {
    return;
  }

  ngx_http_finalize_request(r, ps_base_fetch_handler(r));
}

ngx_int_t ps_create_connection(
    ps_request_ctx_t* ctx, NgxServerContext* server_context, int pipe_fd) {
  // We have to use the server_context's log (which is the server context's
  // ngx_http_core_loc_conf_t->error_log) and not the request's log because
  // this connection can outlast the request by a little while.
  ngx_log_t* server_context_log = server_context->ngx_message_handler()->log();
  if (server_context_log == NULL) {
    ngx_log_debug0(NGX_LOG_INFO, ctx->r->connection->log, 0,
                   "ps_create_connection failed to get server context log");
    return NGX_ERROR;
  }

  ngx_connection_t* c = ngx_get_connection(pipe_fd, server_context_log);
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

// Populate cfg_* with configuration information for this request.
// Thin wrappers around ngx_http_get_module_*_conf and cast.
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
RewriteOptions* ps_determine_request_options(
    ngx_http_request_t* r,
    RequestHeaders* request_headers,
    ResponseHeaders* response_headers,
    ps_srv_conf_t* cfg_s,
    GoogleUrl* url) {
  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  //
  // Sets option from request headers and url.
  RewriteQuery rewrite_query;
  if (!cfg_s->server_context->GetQueryOptions(
          url, request_headers, response_headers, &rewrite_query)) {
    // Failed to parse query params or request headers.  Treat this as if there
    // were no query params given.
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "ps_route rerquest: parsing headers or query params failed.");
    return NULL;
  }

  // Will be NULL if there aren't any options set with query params or in
  // headers.
  return rewrite_query.ReleaseOptions();
}

// Check whether this visitor is already in an experiment.  If they're not,
// classify them into one by setting a cookie.  Then set options appropriately
// for their experiment.
//
// See InstawebContext::SetExperimentStateAndCookie()
bool ps_set_experiment_state_and_cookie(ngx_http_request_t* r,
                                        RequestHeaders* request_headers,
                                        RewriteOptions* options,
                                        const StringPiece& host) {
  CHECK(options->running_experiment());
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  bool need_cookie = cfg_s->server_context->experiment_matcher()->
      ClassifyIntoExperiment(*request_headers, options);
  if (need_cookie && host.length() > 0) {
    PosixTimer timer;
    int64 time_now_ms = timer.NowMs();
    int64 expiration_time_ms = (time_now_ms +
                                options->experiment_cookie_duration_ms());

    // TODO(jefftk): refactor SetExperimentCookie to expose the value we want to
    // set on the cookie.
    int state = options->experiment_id();
    GoogleString expires;
    ConvertTimeToString(expiration_time_ms, &expires);
    GoogleString value = StringPrintf(
        "%s=%s; Expires=%s; Domain=.%s; Path=/",
        experiment::kExperimentCookie,
        experiment::ExperimentStateToCookieString(state).c_str(),
        expires.c_str(), host.as_string().c_str());

    // Set the PagespeedExperiment cookie.
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
                          RequestHeaders* request_headers,
                          ResponseHeaders* response_headers,
                          RewriteOptions** options,
                          GoogleUrl* url,
                          bool html_rewrite) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  ps_loc_conf_t* cfg_l = ps_get_loc_config(r);

  // Global options for this server.  Never null.
  RewriteOptions* global_options = cfg_s->server_context->global_options();

  // Directory-specific options, usually null.  They've already been rebased off
  // of the global options as part of the configuration process.
  RewriteOptions* directory_options = cfg_l->options;

  // Request-specific options, nearly always null.  If set they need to be
  // rebased on the directory options or the global options.
  RewriteOptions* request_options = ps_determine_request_options(
      r, request_headers, response_headers, cfg_s, url);
  bool have_request_options = request_options != NULL;

  // Because the caller takes ownership of any options we return, the only
  // situation in which we can avoid allocating a new RewriteOptions is if the
  // global options are ok as they are and we don't have script variables we
  // need to evaluate at this point.
  NgxRewriteOptions* ngx_global_options =
      dynamic_cast<NgxRewriteOptions*>(global_options);
  if (!have_request_options && directory_options == NULL &&
      !global_options->running_experiment() &&
      ngx_global_options->script_lines().size() == 0) {
    return true;
  }

  // Start with directory options if we have them, otherwise request options.
  if (directory_options != NULL) {
    *options = directory_options->Clone();
  } else {
    *options = global_options->Clone();
  }

  NgxRewriteDriverFactory* ngx_factory = dynamic_cast<NgxRewriteDriverFactory*>(
    cfg_s->server_context->factory());
  NgxRewriteOptions* ngx_options = dynamic_cast<NgxRewriteOptions*>(*options);

  // ExecuteScriptVariables() sets 'pagespeed off' on ngx_options when execution
  // fails and then returns false. When that happens we return, as we don't want
  // to allow enabling pagespeed by request and execute without the intended
  // configuration.
  if (!ngx_options->ExecuteScriptVariables(r, cfg_s->handler, ngx_factory)) {
    return false;
  }

  // Modify our options in response to request options if specified.
  if (have_request_options) {
    (*options)->Merge(*request_options);
    delete request_options;
    request_options = NULL;
  }

  // If we're running an experiment and processing html then modify our options
  // in response to the experiment.  Except we generally don't want experiments
  // to be contaminated with unexpected settings, so ignore experiments if we
  // have request-specific options.  Unless EnrollExperiment is on, probably set
  // by a query parameter, in which case we want to go ahead and apply the
  // experimental settings even if it means bad data, because we're just seeing
  // what it looks like.
  if ((*options)->running_experiment() &&
      html_rewrite &&
      (!have_request_options ||
       (*options)->enroll_experiment())) {
    bool ok = ps_set_experiment_state_and_cookie(
        r, request_headers, *options, url->Host());
    if (!ok) {
      delete *options;
      *options = NULL;
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

  ngx_table_elt_t* header;
  NgxListIterator it(&(r->headers_in.headers.part));
  while ((header = it.Next()) != NULL) {
    if (STR_CASE_EQ_LITERAL(header->key, "X-Forwarded-Proto")) {
      x_forwarded_proto_header = &header->value;
      break;
    }
  }

  if (x_forwarded_proto_header == NULL) {
    return false;  // No X-Forwarded-Proto header found.
  }

  StringPiece x_forwarded_proto =
      str_to_string_piece(*x_forwarded_proto_header);
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
  *url = StrCat(x_forwarded_proto, url_sp.substr(colon_pos));

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

// TODO(chaizhenhua): merge into NgxBaseFetch::Release()
void ps_release_base_fetch(ps_request_ctx_t* ctx) {
  // In the normal flow BaseFetch doesn't delete itself in HandleDone() because
  // we still need to receive notification via pipe and call
  // CollectAccumulatedWrites.  If there's an error and we're cleaning up early
  // then HandleDone() hasn't been called yet and we need the base fetch to wait
  // for that and then delete itself.
  if (ctx->base_fetch != NULL) {
    ctx->base_fetch->Release();
    ctx->base_fetch = NULL;
  }

  if (ctx->pagespeed_connection != NULL) {
    // Tell pagespeed connection ctx has been released.
    ctx->pagespeed_connection->error = 1;
    ctx->pagespeed_connection = NULL;
  }
}

// TODO(chaizhenhua): merge into NgxBaseFetch ctor
ngx_int_t ps_create_base_fetch(ps_request_ctx_t* ctx) {
  ngx_http_request_t* r = ctx->r;
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  int file_descriptors[2];

  int rc = pipe(file_descriptors);
  if (rc != 0) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "pipe() failed");
    return NGX_ERROR;
  }

  if (ngx_nonblocking(file_descriptors[0]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                  ngx_nonblocking_n " pipe[0] failed");
    close(file_descriptors[0]);
    close(file_descriptors[1]);
    return NGX_ERROR;
  }

  if (ngx_nonblocking(file_descriptors[1]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                  ngx_nonblocking_n " pipe[1] failed");
    close(file_descriptors[0]);
    close(file_descriptors[1]);
    return NGX_ERROR;
  }

  rc = ps_create_connection(ctx, cfg_s->server_context, file_descriptors[0]);
  if (rc != NGX_OK) {
    close(file_descriptors[0]);
    close(file_descriptors[1]);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ps_route_request: no pagespeed connection.");
    return NGX_ERROR;
  }

  // Handles its own deletion.  We need to call Release() when we're done with
  // it, and call Done() on the associated parent (Proxy or Resource) fetch. If
  // we fail before creating the associated fetch then we need to call Done() on
  // the BaseFetch ourselves.
  ctx->base_fetch = new NgxBaseFetch(
      r, file_descriptors[1], cfg_s->server_context,
      RequestContextPtr(cfg_s->server_context->NewRequestContext(r)),
      ctx->preserve_caching_headers);

  return NGX_OK;
}

void ps_release_request_context(void* data) {
  ps_request_ctx_t* ctx = static_cast<ps_request_ctx_t*>(data);

  // proxy_fetch deleted itself if we called Done(), but if an error happened
  // before then we need to tell it to delete itself.
  //
  // If this is a resource fetch then proxy_fetch was never initialized.
  if (ctx->proxy_fetch != NULL) {
    ctx->proxy_fetch->Done(false /* failure */);
    ctx->proxy_fetch = NULL;
  }

  if (ctx->inflater_ != NULL) {
    delete ctx->inflater_;
    ctx->inflater_ = NULL;
  }

  if (ctx->driver != NULL) {
    ctx->driver->Cleanup();
    ctx->driver = NULL;
  }

  if (ctx->recorder != NULL) {
    ctx->recorder->Fail();
    ctx->recorder->DoneAndSetHeaders(NULL);  // Deletes recorder.
    ctx->recorder = NULL;
  }

  ps_release_base_fetch(ctx);
  delete ctx;
}

// Set us up for processing a request.  Creates a request context and determines
// which handler should deal with the request.
RequestRouting::Response ps_route_request(ngx_http_request_t* r,
                                          bool is_resource_fetch) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);

  if (!cfg_s->server_context->global_options()->enabled()) {
    // Not enabled for this server block.
    return RequestRouting::kPagespeedDisabled;
  }

  if (r->err_status != 0) {
    return RequestRouting::kErrorResponse;
  }

  GoogleString url_string = ps_determine_url(r);
  GoogleUrl url(url_string);

  if (!url.IsWebValid()) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "invalid url");

    // Let nginx deal with the error however it wants; we will see a NULL ctx in
    // the body filter or content handler and do nothing.
    return RequestRouting::kInvalidUrl;
  }

  if (is_pagespeed_subrequest(r)) {
    return RequestRouting::kPagespeedSubrequest;
  } else if (
      url.PathSansLeaf() == dynamic_cast<NgxRewriteDriverFactory*>(
          cfg_s->server_context->factory())->static_asset_prefix()) {
    return RequestRouting::kStaticContent;
  }

  const NgxRewriteOptions* global_options = cfg_s->server_context->config();

  StringPiece path = url.PathSansQuery();
  if (StringCaseEqual(path, global_options->statistics_path())) {
    return RequestRouting::kStatistics;
  } else if (StringCaseEqual(path, global_options->global_statistics_path())) {
    return RequestRouting::kGlobalStatistics;
  } else if (StringCaseEqual(path, global_options->console_path())) {
    return RequestRouting::kConsole;
  } else if (StringCaseEqual(path, global_options->messages_path())) {
    return RequestRouting::kMessages;
  } else if (// The admin handlers get everything under a path (/path/*) while
             // all the other handlers only get exact matches (/path).  So match
             // all paths starting with the handler path.
             !global_options->admin_path().empty() &&
             StringCaseStartsWith(path, global_options->admin_path())) {
    return RequestRouting::kAdmin;
  } else if (!global_options->global_admin_path().empty() &&
             StringCaseStartsWith(path, global_options->global_admin_path())) {
    return RequestRouting::kGlobalAdmin;
  }

  const GoogleString* beacon_url;
  if (ps_is_https(r)) {
    beacon_url = &(global_options->beacon_url().https);
  } else {
    beacon_url = &(global_options->beacon_url().http);
  }

  if (url.PathSansQuery() == StringPiece(*beacon_url)) {
    return RequestRouting::kBeacon;
  }

  return RequestRouting::kResource;
}

ngx_int_t ps_resource_handler(ngx_http_request_t* r,
                              bool html_rewrite,
                              RequestRouting::Response response_category) {
  if (r != r->main) {
    return NGX_DECLINED;
  }

  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  ps_request_ctx_t* ctx = ps_get_request_context(r);

  CHECK(!(html_rewrite && (ctx == NULL || ctx->html_rewrite == false)));

  if (!html_rewrite &&
      r->method != NGX_HTTP_GET &&
      r->method != NGX_HTTP_HEAD) {
    return NGX_DECLINED;
  }

  GoogleString url_string = ps_determine_url(r);
  GoogleUrl url(url_string);

  CHECK(url.IsWebValid());

  scoped_ptr<RequestHeaders> request_headers(new RequestHeaders);
  scoped_ptr<ResponseHeaders> response_headers(new ResponseHeaders);

  copy_request_headers_from_ngx(r, request_headers.get());
  copy_response_headers_from_ngx(r, response_headers.get());

  RewriteOptions* options = NULL;

  if (!ps_determine_options(r, request_headers.get(), response_headers.get(),
                            &options, &url, html_rewrite)) {
    return NGX_ERROR;
  }

  // Take ownership of custom_options.
  scoped_ptr<RewriteOptions> custom_options(options);
  if (options == NULL) {
    options = cfg_s->server_context->global_options();
  }

  if (!options->enabled()) {
    // Disabled via query params or request headers.
    return NGX_DECLINED;
  }

  // ps_determine_options modified url, removing any ModPagespeedFoo=Bar query
  // parameters.  Keep url_string in sync with url.
  url.Spec().CopyToString(&url_string);

  if (cfg_s->server_context->global_options()->respect_x_forwarded_proto()) {
    bool modified_url = ps_apply_x_forwarded_proto(r, &url_string);
    if (modified_url) {
      url.Reset(url_string);
      CHECK(url.IsWebValid()) << "The output of ps_apply_x_forwarded_proto"
                              << " should always be a valid url because it only"
                              << " changes the scheme between http and https.";
    }
  }

  bool pagespeed_resource =
      !html_rewrite && cfg_s->server_context->IsPagespeedResource(url);
  bool is_an_admin_handler =
      response_category == RequestRouting::kStatistics ||
      response_category == RequestRouting::kGlobalStatistics ||
      response_category == RequestRouting::kConsole ||
      response_category == RequestRouting::kAdmin ||
      response_category == RequestRouting::kGlobalAdmin;

  if (html_rewrite) {
    ps_release_base_fetch(ctx);
  } else {
    // create request ctx
    CHECK(ctx == NULL);
    ctx = new ps_request_ctx_t();

    ctx->r = r;
    ctx->write_pending = false;
    ctx->html_rewrite = false;
    ctx->in_place = false;
    ctx->pagespeed_connection = NULL;
    ctx->preserve_caching_headers = kDontPreserveHeaders;

    // See build_context_for_request() in mod_instaweb.cc
    // TODO(jefftk): Is this the right place to be modifying caching headers for
    // html fetches?  Or should that be done later, in the headers flow for
    // filter mode, rather than here in resource fetch mode?
    if (!options->modify_caching_headers()) {
      ctx->preserve_caching_headers = kPreserveAllCachingHeaders;
    } else if (!options->IsDownstreamCacheIntegrationEnabled()) {
      // Downstream cache integration is not enabled. Disable original
      // Cache-Control headers.
      ctx->preserve_caching_headers = kDontPreserveHeaders;
    } else if (!pagespeed_resource && !is_an_admin_handler) {
      ctx->preserve_caching_headers = kPreserveOnlyCacheControl;
      // Downstream cache integration is enabled. If a rebeaconing key has been
      // configured and there is a ShouldBeacon header with the correct key,
      // disable original Cache-Control headers so that the instrumented page is
      // served out with no-cache.
      StringPiece should_beacon(request_headers->Lookup1(kPsaShouldBeacon));
      if (options->MatchesDownstreamCacheRebeaconingKey(should_beacon)) {
        ctx->preserve_caching_headers = kDontPreserveHeaders;
      }
    }

    ctx->recorder = NULL;
    ctx->url_string = url_string;

    // Set up a cleanup handler on the request.
    ngx_http_cleanup_t* cleanup = ngx_http_cleanup_add(r, 0);
    if (cleanup == NULL) {
      ps_release_request_context(ctx);
      return NGX_ERROR;
    }
    cleanup->handler = ps_release_request_context;
    cleanup->data = ctx;
    ngx_http_set_ctx(r, ctx, ngx_pagespeed);
  }

  if (ps_create_base_fetch(ctx)!= NGX_OK) {
    // Do not need to release request context.
    // http_pool_cleanup will call ps_release_request_context
    return NGX_ERROR;
  }

  ctx->base_fetch->SetRequestHeadersTakingOwnership(request_headers.release());

  bool page_callback_added = false;
  scoped_ptr<ProxyFetchPropertyCallbackCollector>
      property_callback(
          ProxyFetchFactory::InitiatePropertyCacheLookup(
              !html_rewrite /* is_resource_fetch */,
              url,
              cfg_s->server_context,
              options,
              ctx->base_fetch,
              false /* requires_blink_cohort (no longer unused) */,
              &page_callback_added));

  if (pagespeed_resource) {
    // TODO(jefftk): Set using_spdy appropriately.  See
    // ProxyInterface::ProxyRequestCallback
    ResourceFetch::Start(
        url,
        custom_options.release() /* null if there aren't custom options */,
        false /* using_spdy */, cfg_s->server_context, ctx->base_fetch);
    return ps_async_wait_response(r);
  } else if (is_an_admin_handler) {
    QueryParams query_params;
    query_params.Parse(url.Query());

    PosixTimer timer;
    int64 now_ms = timer.NowMs();
    ctx->base_fetch->response_headers()->SetDateAndCaching(
        now_ms, 0 /* max-age */, ", no-cache");

    if (response_category == RequestRouting::kStatistics ||
        response_category == RequestRouting::kGlobalStatistics) {
      cfg_s->server_context->StatisticsPage(
          response_category == RequestRouting::kGlobalStatistics,
          query_params,
          cfg_s->server_context->config(),
          ctx->base_fetch);
    } else if (response_category == RequestRouting::kConsole) {
      cfg_s->server_context->ConsoleHandler(
          *cfg_s->server_context->config(),
          SystemServerContext::kStatistics,
          query_params,
          ctx->base_fetch);
    } else if (response_category == RequestRouting::kAdmin ||
               response_category == RequestRouting::kGlobalAdmin) {
      cfg_s->server_context->AdminPage(
          response_category == RequestRouting::kGlobalAdmin,
          url,
          query_params,
          custom_options == NULL ? cfg_s->server_context->config()
                                 : custom_options.get(),
          ctx->base_fetch);
    } else {
      CHECK(false);
    }

    return ps_async_wait_response(r);
  }

  if (html_rewrite) {
    // Do not store driver in request_context, it's not safe.
    RewriteDriver* driver;

    // If we don't have custom options we can use NewRewriteDriver which reuses
    // rewrite drivers and so is faster because there's no wait to construct
    // them.  Otherwise we have to build a new one every time.

    if (custom_options.get() == NULL) {
      driver = cfg_s->server_context->NewRewriteDriver(
         ctx->base_fetch->request_context());
    } else {
      // NewCustomRewriteDriver takes ownership of custom_options.
      driver = cfg_s->server_context->NewCustomRewriteDriver(
      custom_options.release(), ctx->base_fetch->request_context());
    }

    StringPiece user_agent = ctx->base_fetch->request_headers()->Lookup1(
        HttpAttributes::kUserAgent);
    if (!user_agent.empty()) {
      driver->SetUserAgent(user_agent);
    }
    driver->SetRequestHeaders(*ctx->base_fetch->request_headers());

    // TODO(jefftk): FlushEarlyFlow would go here.

    // Will call StartParse etc.  The rewrite driver will take care of deleting
    // itself if necessary.
    ctx->proxy_fetch = cfg_s->proxy_fetch_factory->CreateNewProxyFetch(
        url_string, ctx->base_fetch, driver,
        property_callback.release(),
        NULL /* original_content_fetch */);
    return NGX_OK;
  }

  if (options->in_place_rewriting_enabled() &&
      options->enabled() &&
      options->IsAllowed(url.Spec())) {
    // Do not store driver in request_context, it's not safe.
    RewriteDriver* driver;
    if (custom_options.get() == NULL) {
      driver = cfg_s->server_context->NewRewriteDriver(
          ctx->base_fetch->request_context());
    } else {
      // NewCustomRewriteDriver takes ownership of custom_options.
      driver = cfg_s->server_context->NewCustomRewriteDriver(
          custom_options.release(), ctx->base_fetch->request_context());
    }

    StringPiece user_agent = ctx->base_fetch->request_headers()->Lookup1(
        HttpAttributes::kUserAgent);
    if (!user_agent.empty()) {
      driver->SetUserAgent(user_agent);
    }
    driver->SetRequestHeaders(*ctx->base_fetch->request_headers());

    ctx->driver = driver;

    cfg_s->server_context->message_handler()->Message(
        kInfo, "Trying to serve rewritten resource in-place: %s",
        url_string.c_str());

    ctx->in_place = true;
    ctx->base_fetch->set_handle_error(false);
    ctx->driver->FetchInPlaceResource(
        url, false /* proxy_mode */, ctx->base_fetch);

    return ps_async_wait_response(r);
  }

  // NOTE: We are using the below debug message as is for some of our system
  // tests. So, be careful about test breakages caused by changing or
  // removing this line.
  DBG(r, "Passing on content handling for non-pagespeed resource '%s'",
      url_string.c_str());

  ctx->base_fetch->Done(false);
  ps_release_base_fetch(ctx);
  // set html_rewrite flag.
  ctx->html_rewrite = true;
  return NGX_DECLINED;
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
      char buf[kStackBufferSize];
      ctx->inflater_->SetInput(reinterpret_cast<char*>(cur->buf->pos),
                               cur->buf->last - cur->buf->pos);
      while (ctx->inflater_->HasUnconsumedInput()) {
        int num_inflated_bytes = ctx->inflater_->InflateBytes(
            buf, kStackBufferSize);
        if (num_inflated_bytes < 0) {
          cfg_s->handler->Message(kWarning,
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

#ifndef ngx_http_clear_etag
// The ngx_http_clear_etag(r) macro was added in 1.3.3.  Backport it if it's not
// present.
#define ngx_http_clear_etag(r)       \
  if (r->headers_out.etag) {         \
    r->headers_out.etag->hash = 0;   \
    r->headers_out.etag = NULL;      \
  }
#endif

void ps_strip_html_headers(ngx_http_request_t* r) {
  // We're modifying content, so switch to 'Transfer-Encoding: chunked' and
  // calculate on the fly.
  ngx_http_clear_content_length(r);

  ngx_table_elt_t* header;
  NgxListIterator it(&(r->headers_out.headers.part));
  while ((header = it.Next()) != NULL) {
    // We also need to strip:
    //   Accept-Ranges
    //    - won't work because our html changes
    //   Vary: Accept-Encoding
    //    - our gzip filter will add this later
    if (STR_CASE_EQ_LITERAL(header->key, "Accept-Ranges") ||
        (STR_CASE_EQ_LITERAL(header->key, "Vary") &&
         STR_CASE_EQ_LITERAL(header->value, "Accept-Encoding"))) {
      // Response headers with hash of 0 are excluded from the response.
      header->hash = 0;
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
  u_char* etag = reinterpret_cast<u_char*>(
      const_cast<char*>(kInternalEtagName));
  ngx_table_elt_t* header;
  NgxListIterator it(&(r->headers_out.headers.part));
  while ((header = it.Next()) != NULL) {
    if (header->key.len == strlen(kInternalEtagName) &&
        !ngx_strncasecmp(header->key.data, etag, header->key.len)) {
      header->key.data = reinterpret_cast<u_char*>(const_cast<char*>("ETag"));
      header->key.len = 4;
      r->headers_out.etag = header;
      break;
    }
  }
  return ngx_http_ef_next_header_filter(r);
}

namespace html_rewrite {
ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

ngx_int_t ps_html_rewrite_header_filter(ngx_http_request_t* r) {
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

  if (ctx == NULL || ctx->html_rewrite == false) {
    return ngx_http_next_header_filter(r);
  }

  if (r->err_status != 0) {
    ctx->html_rewrite = false;
    return ngx_http_next_header_filter(r);
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed html rewrite header filter \"%V\"", &r->uri);

  // We don't know what this request is, but we only want to send html through
  // to pagespeed.  Check the content type header and find out.
  const ContentType* content_type =
      MimeTypeToContentType(
          str_to_string_piece(r->headers_out.content_type));
  if (content_type == NULL || !content_type->IsHtmlLike()) {
    // Unknown or otherwise non-html content type: skip it.
    ctx->html_rewrite = false;
    return ngx_http_next_header_filter(r);
  }

  ngx_int_t rc = ps_resource_handler(r, true /* html rewrite */,
                                     RequestRouting::kResource);
  if (rc != NGX_OK) {
    ctx->html_rewrite = false;
    return ngx_http_next_header_filter(r);
  }

  if (r->headers_out.content_encoding &&
      r->headers_out.content_encoding->value.len) {
    // headers_out.content_encoding will be set to the exact last
    // Content-Encoding response header value that nginx receives. To
    // check if there were multiple (aka stacked) encodings in the
    // response headers, we must iterate them all.
    if (!ps_has_stacked_content_encoding(r)) {
      StringPiece content_encoding =
          str_to_string_piece(r->headers_out.content_encoding->value);
      GzipInflater::InflateType inflate_type;
      bool is_encoded = false;
      if (StringCaseEqual(content_encoding, "deflate")) {
        is_encoded = true;
        inflate_type = GzipInflater::kDeflate;
      } else if (StringCaseEqual(content_encoding, "gzip")) {
        is_encoded = true;
        inflate_type = GzipInflater::kGzip;
      }

      if (is_encoded) {
        r->headers_out.content_encoding->hash = 0;
        r->headers_out.content_encoding = NULL;
        ctx->inflater_ = new GzipInflater(inflate_type);
        ctx->inflater_->Init();
      }
    }
  }

  ps_strip_html_headers(r);

  // TODO(jefftk): is this thread safe?
  copy_response_headers_from_ngx(r, ctx->base_fetch->response_headers());

  ps_set_buffered(r, true);
  r->filter_need_in_memory = 1;
  return NGX_AGAIN;
}

ngx_int_t ps_html_rewrite_body_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return ngx_http_next_body_filter(r, in);
  }

  if (r != r->main) {
    // Don't handle subrequests.
    return ngx_http_next_body_filter(r, in);
  }
  // Don't need to check for a cache flush; already did in
  // ps_html_rewrite_header_filter.

  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx == NULL || ctx->html_rewrite == false) {
    // ctx is null iff we've decided to pass through this request unchanged.
    return ngx_http_next_body_filter(r, in);
  }

  // We don't want to handle requests with errors, but we should be dealing with
  // that in the header filter and not initializing ctx.
  CHECK(r->err_status == 0);                                         // NOLINT

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed html rewrite body filter \"%V\"", &r->uri);


  if (in != NULL) {
    // Send all input data to the proxy fetch.
    ps_send_to_pagespeed(r, ctx, cfg_s, in);
  }

  return ngx_http_next_body_filter(r, NULL);
}

void ps_html_rewrite_filter_init() {
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ps_html_rewrite_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ps_html_rewrite_body_filter;
}

}  // namespace html_rewrite

using html_rewrite::ps_html_rewrite_filter_init;

namespace in_place {

ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

ngx_int_t ps_in_place_check_header_filter(ngx_http_request_t* r) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx == NULL) {
    return ngx_http_next_header_filter(r);
  }

  if (ctx->recorder != NULL) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "ps in place check header filter recording: %V", &r->uri);

    CHECK(!ctx->in_place);

    // We didn't find this resource in cache originally, so we're recording it
    // as it passes us by.  At this point the headers from things that run
    // before us are set but not things that run after us, which means here is
    // where we need to check whether there's a "Content-Encoding: gzip".  If we
    // waited to do this in ps_in_place_body_filter we wouldn't be able to tell
    // the difference between response headers that have "C-E: gz" because we're
    // proxying for an upstream that gzipped the content and response headers
    // that have it because the gzip filter (which runs after us) is going to
    // produce gzipped output.
    //
    // The recorder will do this checking, so pass it the headers.
    ResponseHeaders response_headers;
    copy_response_headers_from_ngx(r, &response_headers);
    ctx->recorder->ConsiderResponseHeaders(
        InPlaceResourceRecorder::kPreliminaryHeaders, &response_headers);
    return ngx_http_next_header_filter(r);
  }

  if (!ctx->in_place) {
    return ngx_http_next_header_filter(r);
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "ps in place check header filter initial: %V", &r->uri);

  int status_code = r->headers_out.status;
  bool status_ok = (status_code != 0) && (status_code < 400);

  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  NgxServerContext* server_context = cfg_s->server_context;
  MessageHandler* message_handler = cfg_s->handler;
  GoogleString url = ps_determine_url(r);
  // The URL we use for cache key is a bit different since it may
  // have PageSpeed query params removed.
  GoogleString cache_url = ctx->url_string;

  // continue process
  if (status_ok) {
    ctx->in_place = false;

    server_context->rewrite_stats()->ipro_served()->Add(1);
    message_handler->Message(
        kInfo, "Serving rewritten resource in-place: %s",
        url.c_str());

    return ngx_http_next_header_filter(r);
  }

  if (status_code == CacheUrlAsyncFetcher::kNotInCacheStatus &&
      !r->header_only) {
    server_context->rewrite_stats()->ipro_not_in_cache()->Add(1);
    server_context->message_handler()->Message(
        kInfo,
        "Could not rewrite resource in-place "
        "because URL is not in cache: %s",
        cache_url.c_str());
    const SystemRewriteOptions* options = SystemRewriteOptions::DynamicCast(
        ctx->driver->options());
    RequestHeaders request_headers;
    copy_request_headers_from_ngx(r, &request_headers);
    // This URL was not found in cache (neither the input resource nor
    // a ResourceNotCacheable entry) so we need to get it into cache
    // (or at least a note that it cannot be cached stored there).
    // We do that using an Apache output filter.
    ctx->recorder = new InPlaceResourceRecorder(
        RequestContextPtr(cfg_s->server_context->NewRequestContext(r)),
        cache_url,
        ctx->driver->CacheFragment(),
        request_headers.GetProperties(),
        options->respect_vary(),
        options->ipro_max_response_bytes(),
        options->ipro_max_concurrent_recordings(),
        options->implicit_cache_ttl_ms(),
        server_context->http_cache(),
        server_context->statistics(),
        message_handler);
    // set in memory flag for in place_body_filter
    r->filter_need_in_memory = 1;

    // We don't have the response headers at all yet because we haven't yet gone
    // to the backend.
  } else {
    server_context->rewrite_stats()->ipro_not_rewritable()->Add(1);
    message_handler->Message(kInfo,
           "Could not rewrite resource in-place: %s", url.c_str());
  }

  ctx->driver->Cleanup();
  ctx->driver = NULL;
  // enable html_rewrite
  ctx->html_rewrite = true;
  ctx->in_place = false;

  return ps_decline_request(r);
}

ngx_int_t ps_in_place_body_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);
  if (ctx == NULL || ctx->recorder == NULL) {
    return ngx_http_next_body_filter(r, in);
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "ps in place body filter: %V", &r->uri);

  InPlaceResourceRecorder* recorder = ctx->recorder;
  for (ngx_chain_t* cl = in; cl; cl = cl->next) {
    if (ngx_buf_size(cl->buf)) {
       CHECK(ngx_buf_in_memory(cl->buf));
       StringPiece contents(reinterpret_cast<char*>(cl->buf->pos),
                            ngx_buf_size(cl->buf));
       recorder->Write(contents, recorder->handler());
    }

    if (cl->buf->flush) {
      recorder->Flush(recorder->handler());
    }

    if (cl->buf->last_buf || recorder->failed()) {
      ResponseHeaders response_headers;
      copy_response_headers_from_ngx(r, &response_headers);
      ctx->recorder->DoneAndSetHeaders(&response_headers);
      ctx->recorder = NULL;
      break;
    }
  }

  return ngx_http_next_body_filter(r, in);
}

void ps_in_place_filter_init() {
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ps_in_place_check_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ps_in_place_body_filter;
}

}  // namespace in_place

using in_place::ps_in_place_filter_init;

ngx_int_t send_out_headers_and_body(
    ngx_http_request_t* r,
    const ResponseHeaders& response_headers,
    const GoogleString& output) {
  ngx_int_t rc = copy_response_headers_to_ngx(
      r, response_headers, kDontPreserveHeaders);

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

ngx_int_t ps_simple_handler(ngx_http_request_t* r,
                            NgxServerContext* server_context,
                            RequestRouting::Response response_category) {
  NgxRewriteDriverFactory* factory =
      static_cast<NgxRewriteDriverFactory*>(
          server_context->factory());
  NgxMessageHandler* message_handler = factory->ngx_message_handler();
  StringPiece request_uri_path = str_to_string_piece(r->uri);

  GoogleString url_string = ps_determine_url(r);
  GoogleUrl url(url_string);
  QueryParams query_params;
  if (url.IsWebValid()) {
    query_params.Parse(url.Query());
  }

  GoogleString output;
  StringWriter writer(&output);
  HttpStatus::Code status = HttpStatus::kOK;
  ContentType content_type = kContentTypeHtml;
  StringPiece cache_control = HttpAttributes::kNoCache;
  const char* error_message = NULL;

  switch (response_category) {
    case RequestRouting::kStaticContent: {
      StringPiece file_contents;
      if (!server_context->static_asset_manager()->GetAsset(
              request_uri_path.substr(factory->static_asset_prefix().length()),
              &file_contents, &content_type, &cache_control)) {
        return NGX_DECLINED;
      }
      file_contents.CopyToString(&output);
      break;
    }
    case RequestRouting::kMessages: {
      GoogleString log;
      StringWriter log_writer(&log);
      if (!message_handler->Dump(&log_writer)) {
        writer.Write("Writing to ngx_pagespeed_message failed. \n"
                     "Please check if it's enabled in pagespeed.conf.\n",
                     message_handler);
      } else {
        HtmlKeywords::WritePre(log, &writer, message_handler);
      }
      break;
    }
    default:
      ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                    "ps_simple_handler: unknown RequestRouting.");
      return NGX_ERROR;
  }

  if (error_message != NULL) {
    status = HttpStatus::kNotFound;
    content_type = kContentTypeHtml;
    output = error_message;
  }

  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(status);
  response_headers.set_major_version(1);
  response_headers.set_minor_version(1);

  response_headers.Add(HttpAttributes::kContentType, content_type.mime_type());
  // http://msdn.microsoft.com/en-us/library/ie/gg622941(v=vs.85).aspx
  // Script and styleSheet elements will reject responses with
  // incorrect MIME types if the server sends the response header
  // "X-Content-Type-Options: nosniff". This is a security feature
  // that helps prevent attacks based on MIME-type confusion.
  response_headers.Add("X-Content-Type-Options", "nosniff");

  int64 now_ms = factory->timer()->NowMs();
  response_headers.SetDate(now_ms);
  response_headers.SetLastModified(now_ms);
  response_headers.Add(HttpAttributes::kCacheControl, cache_control);

  char* cache_control_s = string_piece_to_pool_string(r->pool, cache_control);
  if (cache_control_s != NULL) {
    if (FindIgnoreCase(cache_control, "private") == StringPiece::npos) {
      response_headers.Add(HttpAttributes::kEtag, "W/\"0\"");
    }
  }

  send_out_headers_and_body(r, response_headers, output);
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
      RequestContextPtr(cfg_s->server_context->NewRequestContext(r)));

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

// Parses out query params from the request.
void ps_query_params_handler(ngx_http_request_t* r, StringPiece* data) {
  StringPiece unparsed_uri = str_to_string_piece(r->unparsed_uri);
  stringpiece_ssize_type question_mark_index = unparsed_uri.find("?");
  if (question_mark_index == StringPiece::npos) {
    *data = "";
  } else {
    *data = unparsed_uri.substr(
        question_mark_index+1, unparsed_uri.size() - (question_mark_index+1));
  }
}

// Called after nginx reads the request body from the client.  For another
// example processing request buffers, see ngx_http_form_input_module.c
void ps_beacon_body_handler(ngx_http_request_t* r) {
  // Even if the beacon is a POST, the originating url should be in the query
  // params, not the POST body.
  StringPiece query_param_beacon_data;
  ps_query_params_handler(r, &query_param_beacon_data);

  StringPiece request_body;
  bool ok = ps_request_body_to_string_piece(r, &request_body);
  GoogleString beacon_data = StrCat(
      query_param_beacon_data, "&", request_body);
  if (ok) {
    ps_beacon_handler_helper(r, beacon_data.c_str());
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
    StringPiece query_param_beacon_data;
    ps_query_params_handler(r, &query_param_beacon_data);
    ps_beacon_handler_helper(r, query_param_beacon_data);
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

  RequestRouting::Response response_category =
      ps_route_request(r, true /* is a resource fetch */);
  switch (response_category) {
    case RequestRouting::kError:
      return NGX_ERROR;
    case RequestRouting::kNotUnderstood:
    case RequestRouting::kPagespeedDisabled:
    case RequestRouting::kInvalidUrl:
    case RequestRouting::kPagespeedSubrequest:
    case RequestRouting::kNotHeadOrGet:
    case RequestRouting::kErrorResponse:
      return NGX_DECLINED;
    case RequestRouting::kBeacon:
      return ps_beacon_handler(r);
    case RequestRouting::kStaticContent:
    case RequestRouting::kMessages:
      return ps_simple_handler(r, cfg_s->server_context, response_category);
    case RequestRouting::kStatistics:
    case RequestRouting::kGlobalStatistics:
    case RequestRouting::kConsole:
    case RequestRouting::kAdmin:
    case RequestRouting::kGlobalAdmin:
    case RequestRouting::kResource:
      return ps_resource_handler(
          r, false /* html rewrite */, response_category);
  }

  CHECK(0);
  return NGX_ERROR;
}

ngx_int_t ps_phase_handler(ngx_http_request_t* r,
      ngx_http_phase_handler_t* ph) {
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

namespace fix_headers {
ngx_http_output_header_filter_pt ngx_http_next_header_filter;

ngx_int_t ps_html_rewrite_fix_headers_filter(ngx_http_request_t* r) {
  ps_request_ctx_t* ctx = ps_get_request_context(r);
  if (r != r->main || ctx == NULL || !ctx->html_rewrite
      || ctx->preserve_caching_headers == kPreserveAllCachingHeaders) {
    return ngx_http_next_header_filter(r);
  }
  if (ctx->preserve_caching_headers == kDontPreserveHeaders) {
    // Don't cache html.  See mod_instaweb:instaweb_fix_headers_filter.
    NgxCachingHeaders caching_headers(r);
    ps_set_cache_control(r, string_piece_to_pool_string(
        r->pool, caching_headers.GenerateDisabledCacheControl()));
  }

  // Pagespeed html doesn't need etags: it should never be cached.
  ngx_http_clear_etag(r);

  // An html page may change without the underlying file changing, because of
  // how resources are included.  Pagespeed adds cache control headers for
  // resources instead of using the last modified header.
  ngx_http_clear_last_modified(r);

  // Clear expires
  if (r->headers_out.expires) {
    r->headers_out.expires->hash = 0;
    r->headers_out.expires = NULL;
  }

  return ngx_http_next_header_filter(r);
}

void ps_html_rewrite_fix_headers_filter_init() {
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ps_html_rewrite_fix_headers_filter;
}

}  // namespace fix_headers

using fix_headers::ps_html_rewrite_fix_headers_filter_init;


// preaccess_handler should be at generic phase before try_files
ngx_int_t ps_preaccess_handler(ngx_http_request_t* r) {
  ngx_http_core_main_conf_t* cmcf;
  ngx_http_phase_handler_t* ph;
  ngx_uint_t i;

  cmcf = static_cast<ngx_http_core_main_conf_t*>(
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
    // The filter init order is important.
    ps_in_place_filter_init();

    ps_html_rewrite_fix_headers_filter_init();
    ps_base_fetch_filter_init();
    ps_html_rewrite_filter_init();

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

  std::vector<SystemServerContext*> server_contexts;
  // Iterate over all configured server{} blocks to collect the server contexts.
  for (s = 0; s < cmcf->servers.nelts; s++) {
    ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
        cscfp[s]->ctx->srv_conf[ngx_pagespeed.ctx_index]);
    if (cfg_s->server_context != NULL) {
      server_contexts.push_back(cfg_s->server_context);
    }
  }

  GoogleString error_message;
  int error_index = -1;
  Statistics* global_statistics = NULL;
  cfg_m->driver_factory->PostConfig(
      server_contexts, &error_message, &error_index, &global_statistics);
  if (error_index != -1) {
    server_contexts[error_index]->message_handler()->Message(
        kError, "ngx_pagespeed is enabled. %s", error_message.c_str());
    return NGX_ERROR;
  }

  if (!server_contexts.empty()) {
    // TODO(oschaaf): this ignores sigpipe messages from memcached.
    // however, it would be better to not have those signals generated
    // in the first place, as suppressing them this way may interfere
    // with other modules that actually are interested in these signals
    ps_ignore_sigpipe();

    // If no shared-mem statistics are enabled, then init using the default
    // NullStatistics.
    if (global_statistics == NULL) {
      NgxRewriteDriverFactory::InitStats(cfg_m->driver_factory->statistics());
    }

    ngx_http_core_loc_conf_t* clcf = static_cast<ngx_http_core_loc_conf_t*>(
        ngx_http_conf_get_module_loc_conf((*cscfp), ngx_http_core_module));

    cfg_m->driver_factory->set_resolver(clcf->resolver);
    cfg_m->driver_factory->set_resolver_timeout(clcf->resolver_timeout);

    if (!cfg_m->driver_factory->CheckResolver()) {
      cfg_m->handler->Message(
          kError,
          "UseNativeFetcher is on, please configure a resolver.");
      return NGX_ERROR;
    }

    cfg_m->driver_factory->LoggingInit(cycle->log);
    cfg_m->driver_factory->RootInit();
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

  SystemRewriteDriverFactory::InitApr();

  // ChildInit() will initialise all ServerContexts, which we need to
  // create ProxyFetchFactories below
  cfg_m->driver_factory->LoggingInit(cycle->log);
  cfg_m->driver_factory->ChildInit();

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
      cfg_s->proxy_fetch_factory = new ProxyFetchFactory(cfg_s->server_context);
      ngx_http_core_loc_conf_t* clcf = static_cast<ngx_http_core_loc_conf_t*>(
          cscfp[s]->ctx->loc_conf[ngx_http_core_module.ctx_index]);
      cfg_m->driver_factory->SetServerContextMessageHandler(
          cfg_s->server_context, clcf->error_log);
    }
  }

  if (!cfg_m->driver_factory->InitNgxUrlAsyncFetchers()) {
    return NGX_ERROR;
  }
  cfg_m->driver_factory->StartThreads();

  return NGX_OK;
}

}  // namespace

}  // namespace net_instaweb

ngx_module_t ngx_pagespeed_etag_filter = {
  NGX_MODULE_V1,
  &net_instaweb::ps_etag_filter_module,
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
  &net_instaweb::ps_module,
  net_instaweb::ps_commands,
  NGX_HTTP_MODULE,
  NULL,
  net_instaweb::ps_init_module,
  net_instaweb::ps_init_child_process,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};
