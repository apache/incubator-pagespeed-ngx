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

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include <unistd.h>

#include "ngx_rewrite_driver_factory.h"
#include "ngx_server_context.h"
#include "ngx_rewrite_options.h"
#include "ngx_base_fetch.h"

#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/rewriter/public/furious_matcher.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/automatic/public/resource_fetch.h"

extern ngx_module_t ngx_pagespeed;

// Hacks for debugging.
#define DBG(r, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (r)->connection->log, 0, args)
#define PDBG(ctx, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (ctx)->pagespeed_connection->log, 0, args)
#define CDBG(cf, args...)                                     \
  ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, args)

namespace ngx_psol {

StringPiece
str_to_string_piece(ngx_str_t s) {
  return StringPiece(reinterpret_cast<char*>(s.data), s.len);
}

char*
string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp) {
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

ngx_int_t
string_piece_to_buffer_chain(
    ngx_pool_t* pool, StringPiece sp, ngx_chain_t** link_ptr,
    bool send_last_buf) {

  if (!send_last_buf && sp.size() == 0) {
    // Nothing to send, not even the metadata that this is the last buffer.
    return NGX_DECLINED;
  }

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
      CHECK(offset == 0);
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

typedef struct {
  net_instaweb::ProxyFetch* proxy_fetch;
  net_instaweb::NgxBaseFetch* base_fetch;
  bool data_received;
  int pipe_fd;
  ngx_connection_t* pagespeed_connection;
  ngx_http_request_t* r;
  bool is_resource_fetch;
  bool sent_headers;
  bool write_pending;
} ps_request_ctx_t;

ngx_int_t
ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

void*
ps_create_srv_conf(ngx_conf_t* cf);

char*
ps_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child);

char*
ps_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);

void
ps_release_request_context(void* data);

void
ps_set_buffered(ngx_http_request_t* r, bool on);

GoogleString
ps_determine_url(ngx_http_request_t* r);

ps_request_ctx_t*
ps_get_request_context(ngx_http_request_t* r);

void
ps_initialize_server_context(ps_srv_conf_t* cfg);

ngx_int_t
ps_update(ps_request_ctx_t* ctx, ngx_event_t* ev);

void
ps_connection_read_handler(ngx_event_t* ev);

ngx_int_t
ps_create_connection(ps_request_ctx_t* ctx);

namespace CreateRequestContext {
enum Response {
  kOk,
  kError,
  kNotUnderstood,
  kStaticContent,
  kInvalidUrl,
  kPagespeedDisabled,
};
} // namespace CreateRequestContext

CreateRequestContext::Response
ps_create_request_context(ngx_http_request_t* r, bool is_resource_fetch);

void
ps_send_to_pagespeed(ngx_http_request_t* r,
                     ps_request_ctx_t* ctx,
                     ps_srv_conf_t* cfg_s,
                     ngx_chain_t* in);

ngx_int_t
ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

ngx_int_t
ps_header_filter(ngx_http_request_t* r);

ngx_int_t
ps_init(ngx_conf_t* cf);

ngx_int_t
ps_init_process(ngx_cycle_t* cycle);

char*
ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

char*
ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

void
ps_ignore_sigpipe();

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

void
ps_ignore_sigpipe() {
  struct sigaction act;
  ngx_memzero(&act, sizeof(act));
  act.sa_handler = SIG_IGN;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  sigaction (SIGPIPE, &act, NULL);
}

#define NGX_PAGESPEED_MAX_ARGS 10
char*
ps_configure(ngx_conf_t* cf,
             net_instaweb::NgxRewriteOptions** options,
             net_instaweb::MessageHandler* handler) {
  if (*options == NULL) {
    net_instaweb::NgxRewriteOptions::Initialize();
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

  const char* status = (*options)->ParseAndSetOptions(
      args, n_args, cf->pool, handler);

  // nginx expects us to return a string literal but doesn't mark it const.
  return const_cast<char*>(status);
}

char*
ps_srv_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_srv_conf_t* cfg_s = static_cast<ps_srv_conf_t*>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));
  return ps_configure(cf, &cfg_s->options, cfg_s->handler);
}

char*
ps_loc_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ps_loc_conf_t* cfg_l = static_cast<ps_loc_conf_t*>(
          ngx_http_conf_get_module_loc_conf(cf, ngx_pagespeed));

  // TODO(jefftk): pass something to configure() to tell it that this option was
  // set in a location block so it can be more strict.  Not all options can be
  // set in location blocks.  (For now we'll allow them, which in practice means
  // they'll be ignored because they're read from the config before
  // location-specific options are applied.)
  return ps_configure(cf, &cfg_l->options, cfg_l->handler);
}

template <typename ConfT>
void*
ps_create_conf(ngx_conf_t* cf) {
  ConfT* cfg = static_cast<ConfT*>(ngx_pcalloc(cf->pool, sizeof(ConfT)));
  if (cfg == NULL) {
    return NGX_CONF_ERROR;
  }
  cfg->handler = new net_instaweb::GoogleMessageHandler();
  return cfg;
}

void*
ps_create_main_conf(ngx_conf_t* cf) {
  return ps_create_conf<ps_main_conf_t>(cf);
}

void*
ps_create_srv_conf(ngx_conf_t* cf) {
  return ps_create_conf<ps_srv_conf_t>(cf);
}

void*
ps_create_loc_conf(ngx_conf_t* cf) {
  return ps_create_conf<ps_loc_conf_t>(cf);
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
char*
ps_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child) {
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

  // We initialize the driver factory here and not in an init_main_conf function
  // because if there are no server blocks with pagespeed configuration
  // directives then we don't want it initialized.
  if (cfg_m->driver_factory == NULL) {
    // TODO(oschaaf): this ignores sigpipe messages from memcached.
    // however, it would be better to not have those signals generated
    // in the first place, as suppressing them this way may interfere
    // with other modules that actually are interested in these signals
    ps_ignore_sigpipe();
    net_instaweb::NgxRewriteDriverFactory::Initialize();
    // TODO(jefftk): We should call NgxRewriteDriverFactory::Terminate() when
    // we're done with it.  That never happens, though, because this is the
    // top-level config and so sticks around as long as we're running.

    ngx_http_core_loc_conf_t* clcf = static_cast<ngx_http_core_loc_conf_t*>(
        ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));
    cfg_m->driver_factory = new net_instaweb::NgxRewriteDriverFactory(clcf->error_log,
        clcf->resolver_timeout, clcf->resolver);
  }

  cfg_s->server_context = new net_instaweb::NgxServerContext(
      cfg_m->driver_factory);

  // The server context sets some options when we call global_options().  So let
  // it do that, then merge in options we got from parsing the config file.
  // Once we do that we're done with cfg_s->options.
  cfg_s->server_context->global_options()->Merge(*cfg_s->options);
  delete cfg_s->options;
  cfg_s->options = NULL;

  StringPiece filename_prefix =
      cfg_s->server_context->config()->file_cache_path();
  cfg_s->server_context->set_lock_manager(
      new net_instaweb::FileSystemLockManager(
          cfg_m->driver_factory->file_system(),
          filename_prefix.as_string(),
          cfg_m->driver_factory->scheduler(),
          cfg_m->driver_factory->message_handler()));

  cfg_m->driver_factory->InitServerContext(cfg_s->server_context);

  cfg_s->proxy_fetch_factory =
      new net_instaweb::ProxyFetchFactory(cfg_s->server_context);

  return NGX_CONF_OK;
}

char*
ps_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child) {
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

ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

void
ps_release_request_context(void* data) {
  ps_request_ctx_t* ctx = static_cast<ps_request_ctx_t*>(data);

  // proxy_fetch deleted itself if we called Done(), but if an error happened
  // before then we need to tell it to delete itself.
  //
  // If this is a resource fetch then proxy_fetch was never initialized.
  if (ctx->proxy_fetch != NULL) {
    ctx->proxy_fetch->Done(false /* failure */);
  }

  // BaseFetch doesn't delete itself in HandleDone() because we still need to
  // recieve notification via pipe and call CollectAccumulatedWrites.
  // TODO(jefftk): If we close the proxy_fetch above and not in the normal flow,
  // can this delete base_fetch while proxy_fetch still needs it? Is there a
  // race condition here?
  delete ctx->base_fetch;

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
void
ps_set_buffered(ngx_http_request_t* r, bool on) {
  if (on) {
    r->buffered |= NGX_HTTP_SSI_BUFFERED;
  } else {
    r->buffered &= ~NGX_HTTP_SSI_BUFFERED;
  }
}

GoogleString
ps_determine_url(ngx_http_request_t* r) {
  // Based on ngx_http_variable_scheme.
  bool is_https = false;
#if (NGX_HTTP_SSL)
  is_https = r->connection->ssl;
#endif

  // Based on ngx_http_variable_server_port.
  ngx_uint_t port;
  bool have_port = false;
#if (NGX_HAVE_INET6)
  if (r->connection->local_sockaddr->sa_family == AF_INET6) {
    port = ntohs(reinterpret_cast<struct sockaddr_in6*>(
        r->connection->local_sockaddr)->sin6_port);
    have_port= true;
  }
#endif
  if (!have_port) {
    port = ntohs(reinterpret_cast<struct sockaddr_in*>(
        r->connection->local_sockaddr)->sin_port);
  }

  GoogleString port_string;
  if ((is_https && port == 443) || (!is_https && port == 80)) {
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
      is_https ? "https://" : "http://", host, port_string,
      str_to_string_piece(r->unparsed_uri));
}

// Get the context for this request.  ps_create_request_context
// should already have been called to create it.
ps_request_ctx_t*
ps_get_request_context(ngx_http_request_t* r) {
  return static_cast<ps_request_ctx_t*>(
      ngx_http_get_module_ctx(r, ngx_pagespeed));
}

// Returns:
//   NGX_OK: pagespeed is done, request complete
//   NGX_AGAIN: pagespeed still working, needs to be called again later
//   NGX_ERROR: error
ngx_int_t
ps_update(ps_request_ctx_t* ctx, ngx_event_t* ev) {
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

void
ps_writer(ngx_http_request_t* r)
{
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

ngx_int_t
ngx_http_set_pagespeed_write_handler(ngx_http_request_t *r)
{
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

void
ps_connection_read_handler(ngx_event_t* ev) {
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

ngx_int_t
ps_create_connection(ps_request_ctx_t* ctx) {
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
ps_srv_conf_t*
ps_get_srv_config(ngx_http_request_t* r) {
  return static_cast<ps_srv_conf_t*>(
      ngx_http_get_module_srv_conf(r, ngx_pagespeed));
}
ps_loc_conf_t*
ps_get_loc_config(ngx_http_request_t* r) {
  return static_cast<ps_loc_conf_t*>(
      ngx_http_get_module_loc_conf(r, ngx_pagespeed));
}

// Wrapper around GetQueryOptions()
bool
ps_determine_request_options(
    ngx_http_request_t* r,
    ps_request_ctx_t* ctx,
    ps_srv_conf_t* cfg_s,
    net_instaweb::GoogleUrl* url,
    net_instaweb::RewriteOptions** request_options) {
  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  //
  // Sets option from request headers and url.
  net_instaweb::ServerContext::OptionsBoolPair query_options_success =
      cfg_s->server_context->GetQueryOptions(
          url, ctx->base_fetch->request_headers(), NULL);
  bool get_query_options_success = query_options_success.second;
  if (!get_query_options_success) {
    // Failed to parse query params or request headers.
    // TODO(jefftk): send a helpful error message to the visitor.
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ps_create_request_context: "
                  "parsing headers or query params failed.");
    return false;
  }

  // Will be NULL if there aren't any options set with query params or in
  // headers.
  *request_options = query_options_success.first;

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
bool
ps_determine_options(ngx_http_request_t* r,
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
  net_instaweb::RewriteOptions* request_options;
  bool ok = ps_determine_request_options(r, ctx, cfg_s, url, &request_options);
  if (!ok) {
    *options = NULL;
    return false;
  }

  // Because the caller takes memory ownership of any options we return, the
  // only situation in which we can avoid allocating a new RewriteOptions is if
  // the global options are ok as are.
  if (directory_options == NULL && request_options == NULL &&
      !global_options->running_furious()) {
    *options = NULL;
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
  } else if ((*options)->running_furious()) {
    (*options)->set_need_to_store_experiment_data(
        cfg_s->server_context->furious_matcher()->ClassifyIntoExperiment(
            *ctx->base_fetch->request_headers(), *options));
  }

  return true;
}


// Set us up for processing a request.
CreateRequestContext::Response
ps_create_request_context(ngx_http_request_t* r, bool is_resource_fetch) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);

  GoogleString url_string = ps_determine_url(r);
  net_instaweb::GoogleUrl url(url_string);

  if (!url.is_valid()) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "invalid url");

    // Let nginx deal with the error however it wants; we will see a NULL ctx in
    // the body filter or content handler and do nothing.
    return CreateRequestContext::kInvalidUrl;
  }

  if (is_resource_fetch && !cfg_s->server_context->IsPagespeedResource(url)) {
    if (url.PathSansLeaf() ==
        net_instaweb::NgxRewriteDriverFactory::kStaticJavaScriptPrefix) {
      return CreateRequestContext::kStaticContent;
    } else {
      DBG(r, "Passing on content handling for non-pagespeed resource '%s'",
          url_string.c_str());
      return CreateRequestContext::kNotUnderstood;
    }
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

  // Deletes itself when HandleDone is called, which happens when we call Done()
  // on the proxy fetch below.
  ctx->base_fetch = new net_instaweb::NgxBaseFetch(r, file_descriptors[1]);

  // If null, that means use global options.
  net_instaweb::RewriteOptions* custom_options;
  bool ok = ps_determine_options(r, ctx, &custom_options, &url);
  if (!ok) {
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
    ps_release_request_context(ctx);
    return CreateRequestContext::kPagespeedDisabled;
  }

  // TODO(jefftk): port ProxyInterface::InitiatePropertyCacheLookup so that we
  // have the propery cache in nginx.

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
    net_instaweb::RewriteDriver* driver;
    if (custom_options == NULL) {
      driver = cfg_s->server_context->NewRewriteDriver();
    } else {
      // NewCustomRewriteDriver takes ownership of custom_options.
      driver = cfg_s->server_context->NewCustomRewriteDriver(custom_options);
    }
    driver->set_log_record(ctx->base_fetch->log_record());

    // TODO(jefftk): FlushEarlyFlow would go here.

    // Will call StartParse etc.  The rewrite driver will take care of deleting
    // itself if necessary.
    ctx->proxy_fetch = cfg_s->proxy_fetch_factory->CreateNewProxyFetch(
        url_string, ctx->base_fetch, driver,
        NULL /* property_callback */,
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
void
ps_send_to_pagespeed(ngx_http_request_t* r,
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
    ctx->proxy_fetch->Write(StringPiece(reinterpret_cast<char*>(cur->buf->pos),
                                        cur->buf->last - cur->buf->pos),
                            cfg_s->handler);

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

ngx_int_t
ps_body_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return ngx_http_next_body_filter(r, in);
  }

  ps_request_ctx_t* ctx = ps_get_request_context(r);

  if (ctx == NULL) {
    // ctx is null iff we've decided to pass through this request unchanged.
    return ngx_http_next_body_filter(r, in);
  }

  // We don't want to handle requests with errors, but we should be dealing with
  // that in the header filter and not initializing ctx.
  CHECK(r->err_status == 0);

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
ngx_int_t
ps_set_cache_control(ngx_http_request_t* r, char* cache_control) {
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

ngx_int_t
ps_header_filter(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return ngx_http_next_header_filter(r);
  }

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
    case CreateRequestContext::kPagespeedDisabled:
    case CreateRequestContext::kStaticContent:
    case CreateRequestContext::kInvalidUrl:
      return ngx_http_next_header_filter(r);
    case CreateRequestContext::kOk:
      break;
  }

  // We're modifying content below, so switch to 'Transfer-Encoding: chunked'
  // and calculate on the fly.
  ngx_http_clear_content_length(r);

  // Pagespeed html doesn't need etags: it should never be cached.
  ngx_http_clear_etag(r);

  // An page may change without the underlying file changing, because of how
  // resources are included.  Pagespeed adds cache control headers for
  // resources instead of using the last modified header.
  ngx_http_clear_last_modified(r);

  // Don't cache html.  See mod_instaweb:instaweb_fix_headers_filter.
  ps_set_cache_control(r, const_cast<char*>("max-age=0, no-cache"));

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
  ngx_str_set(&x_pagespeed->value, MOD_PAGESPEED_VERSION_STRING);

  return ngx_http_next_header_filter(r);
}

ngx_int_t
ps_static_handler(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);

  StringPiece request_uri_path = str_to_string_piece(r->uri);

  // Strip out the common prefix url before sending to
  // StaticJavascriptManager.
  StringPiece file_name = request_uri_path.substr(
      strlen(net_instaweb::NgxRewriteDriverFactory::kStaticJavaScriptPrefix));
  StringPiece file_contents;
  StringPiece cache_header;
  bool found = cfg_s->server_context->static_javascript_manager()->GetJsSnippet(
      file_name, &file_contents, &cache_header);
  if (!found) {
    return NGX_DECLINED;
  }

  // Set and send headers.
  r->headers_out.status = NGX_HTTP_OK;

  // Content length
  r->headers_out.content_length_n = file_contents.size();
  r->headers_out.content_type.len = sizeof("text/javascript") - 1;
  r->headers_out.content_type_len = r->headers_out.content_type.len;
  r->headers_out.content_type.data =
      reinterpret_cast<u_char*>(const_cast<char*>("text/javascript"));
  r->headers_out.content_type_lowcase = r->headers_out.content_type.data;

  // Cache control
  char* cache_control_s = string_piece_to_pool_string(r->pool, cache_header);
  if (cache_control_s == NULL) {
    return NGX_ERROR;
  }
  ps_set_cache_control(r, cache_control_s);

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

// Handle requests for resources like example.css.pagespeed.ce.LyfcM6Wulf.css
// and for static content like /ngx_pagespeed_static/js_defer.q1EBmcgYOC.js
ngx_int_t
ps_content_handler(ngx_http_request_t* r) {
  ps_srv_conf_t* cfg_s = ps_get_srv_config(r);
  if (cfg_s->server_context == NULL) {
    // Pagespeed is on for some server block but not this one.
    return NGX_DECLINED;
  }

  // TODO(jefftk): return NGX_DECLINED for non-get non-head requests.

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed handler \"%V\"", &r->uri);

  switch (ps_create_request_context(
      r, true /* is a resource fetch */)) {
    case CreateRequestContext::kError:
      return NGX_ERROR;
    case CreateRequestContext::kNotUnderstood:
    case CreateRequestContext::kPagespeedDisabled:
    case CreateRequestContext::kInvalidUrl:
      return NGX_DECLINED;
    case CreateRequestContext::kStaticContent:
      return ps_static_handler(r);
    case CreateRequestContext::kOk:
      break;
  }

  ps_request_ctx_t* ctx =
      ps_get_request_context(r);
  CHECK(ctx != NULL);

  // Tell nginx we're still working on this one.
  r->count++;

  return NGX_DONE;
}

ngx_int_t
ps_init(ngx_conf_t* cf) {
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
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));
    if (h == NULL) {
      return NGX_ERROR;
    }
    *h = ps_content_handler;
  }

  return NGX_OK;
}

ngx_int_t
ps_init_process(ngx_cycle_t* cycle) {
  ps_main_conf_t* cfg_m = static_cast<ps_main_conf_t*>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_pagespeed));
  if (cfg_m->driver_factory != NULL) {
    if (cfg_m->driver_factory->InitNgxUrlAsyncFecther()) {
      return NGX_OK;
    }
  }
  return NGX_OK;
}

ngx_http_module_t ps_module = {
  NULL,  // preconfiguration
  ps_init,  // postconfiguration

  ps_create_conf<ps_main_conf_t>,
  NULL,  // initialize main configuration

  ps_create_conf<ps_srv_conf_t>,
  ps_merge_srv_conf,

  ps_create_conf<ps_loc_conf_t>,
  ps_merge_loc_conf
};

}  // namespace

}  // namespace ngx_psol

ngx_module_t ngx_pagespeed = {
  NGX_MODULE_V1,
  &ngx_psol::ps_module,
  ngx_psol::ps_commands,
  NGX_HTTP_MODULE,
  NULL,
  NULL,
  ngx_psol::ps_init_process,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};
