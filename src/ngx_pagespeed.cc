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
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/automatic/public/resource_fetch.h"

extern ngx_module_t ngx_pagespeed;

// Hacks for debugging.
#define DBG(r, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (r)->connection->log, 0, args)
#define PDBG(ctx, args...)                                       \
  ngx_log_error(NGX_LOG_DEBUG, (ctx)->pagespeed_connection->log, 0, args)
#define CDBG(cf, args...)                                     \
  ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, args)

StringPiece
ngx_http_pagespeed_str_to_string_piece(ngx_str_t s) {
  return StringPiece(reinterpret_cast<char*>(s.data), s.len);
}

char*
ngx_http_string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp) {
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

typedef struct {
  net_instaweb::NgxRewriteDriverFactory* driver_factory;
  net_instaweb::NgxServerContext* server_context;
  net_instaweb::NgxRewriteOptions* options;
  net_instaweb::ProxyFetchFactory* proxy_fetch_factory;
  net_instaweb::MessageHandler* handler;
} ngx_http_pagespeed_srv_conf_t;

typedef struct {
  ngx_http_pagespeed_srv_conf_t* cfg;
  net_instaweb::ProxyFetch* proxy_fetch;
  net_instaweb::NgxBaseFetch* base_fetch;
  bool data_received;
  int pipe_fd;
  ngx_connection_t* pagespeed_connection;
  ngx_http_request_t* r;
  bool is_resource_fetch;
  bool sent_headers;
  bool write_pending;
} ngx_http_pagespeed_request_ctx_t;

namespace {

ngx_int_t
ngx_http_pagespeed_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

void*
ngx_http_pagespeed_create_srv_conf(ngx_conf_t* cf);

char*
ngx_http_pagespeed_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child);

void
ngx_http_pagespeed_release_request_context(void* data);

void
ngx_http_pagespeed_set_buffered(ngx_http_request_t* r, bool on);

GoogleString
ngx_http_pagespeed_determine_url(ngx_http_request_t* r);

ngx_http_pagespeed_request_ctx_t*
ngx_http_pagespeed_get_request_context(ngx_http_request_t* r);

void
ngx_http_pagespeed_initialize_server_context(
    ngx_http_pagespeed_srv_conf_t* cfg);

ngx_int_t
ngx_http_pagespeed_update(ngx_http_pagespeed_request_ctx_t* ctx,
                          ngx_event_t* ev);

void
ngx_http_pagespeed_connection_read_handler(ngx_event_t* ev);

ngx_int_t
ngx_http_pagespeed_create_connection(ngx_http_pagespeed_request_ctx_t* ctx);

ngx_int_t
ngx_http_pagespeed_create_request_context(ngx_http_request_t* r,
                                          bool is_resource_fetch);

void
ngx_http_pagespeed_send_to_pagespeed(
    ngx_http_request_t* r,
    ngx_http_pagespeed_request_ctx_t* ctx,
    ngx_chain_t* in);

ngx_int_t
ngx_http_pagespeed_body_filter(ngx_http_request_t* r, ngx_chain_t* in);

ngx_int_t
ngx_http_pagespeed_header_filter(ngx_http_request_t* r);

ngx_int_t
ngx_http_pagespeed_init(ngx_conf_t* cf);

char*
ngx_http_pagespeed_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

ngx_command_t ngx_http_pagespeed_commands[] = {
  { ngx_string("pagespeed"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1|
    NGX_CONF_TAKE2|NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5,
    ngx_http_pagespeed_configure,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL },

  ngx_null_command
};

#define NGX_PAGESPEED_MAX_ARGS 10
char*
ngx_http_pagespeed_configure(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
  ngx_http_pagespeed_srv_conf_t* cfg =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(
          ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));

  if (cfg->options == NULL) {
    net_instaweb::NgxRewriteOptions::Initialize();
    cfg->options = new net_instaweb::NgxRewriteOptions();
    cfg->handler = new net_instaweb::GoogleMessageHandler();
  }

  // args[0] is always "pagespeed"; ignore it.
  ngx_uint_t n_args = cf->args->nelts - 1;

  // In ngx_http_pagespeed_commands we only register 'pagespeed' as taking up to
  // five arguments, so this check should never fire.
  CHECK(n_args <= NGX_PAGESPEED_MAX_ARGS);
  StringPiece args[NGX_PAGESPEED_MAX_ARGS];

  ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
  ngx_uint_t i;
  for (i = 0 ; i < n_args ; i++) {
    args[i] = ngx_http_pagespeed_str_to_string_piece(value[i+1]);
  }

  const char* status = cfg->options->ParseAndSetOptions(
      args, n_args, cf->pool, cfg->handler);

  // nginx expects us to return a string literal but doesn't mark it const.
  return const_cast<char*>(status);
}

void*
ngx_http_pagespeed_create_srv_conf(ngx_conf_t* cf) {
  ngx_http_pagespeed_srv_conf_t* conf;

  conf = static_cast<ngx_http_pagespeed_srv_conf_t*>(
      ngx_pcalloc(cf->pool, sizeof(ngx_http_pagespeed_srv_conf_t)));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }

  // set by ngx_pcalloc():
  //   conf->driver_factory = NULL;
  //   conf->server_context = NULL;

  return conf;
}

// nginx has hierarchical configuration.  It maintains configurations at many
// levels.  At various points it needs to merge configurations from different
// levels, and then it calls this.  First it creates the configuration at the
// new level, parsing any pagespeed directives, then it merges in the
// configuration from the level above.  This function should merge the parent
// configuration (prev) into the child (conf).  It's only more complex than
// conf->options->Merge() because of the cases where the parent or child didn't
// have any pagespeed directives.
char*
ngx_http_pagespeed_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child) {
  ngx_http_pagespeed_srv_conf_t* prev =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(parent);
  ngx_http_pagespeed_srv_conf_t* conf =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(child);

  if (prev->options == NULL) {
    // Nothing to do.
  } else if (conf->options == NULL && prev->options != NULL) {
    conf->options = prev->options->Clone();
  } else {  // Both non-null.
    conf->options->Merge(*prev->options);
  }
  return NGX_CONF_OK;
}

ngx_http_output_header_filter_pt ngx_http_next_header_filter;
ngx_http_output_body_filter_pt ngx_http_next_body_filter;

void
ngx_http_pagespeed_release_request_context(void* data) {
  ngx_http_pagespeed_request_ctx_t* ctx =
      static_cast<ngx_http_pagespeed_request_ctx_t*>(data);

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
ngx_http_pagespeed_set_buffered(ngx_http_request_t* r, bool on) {
  if (on) {
    r->buffered |= NGX_HTTP_SSI_BUFFERED;
  } else {
    r->buffered &= ~NGX_HTTP_SSI_BUFFERED;
  }
}

GoogleString
ngx_http_pagespeed_determine_url(ngx_http_request_t* r) {
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

  StringPiece host =
      ngx_http_pagespeed_str_to_string_piece(r->headers_in.server);
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
    host = ngx_http_pagespeed_str_to_string_piece(s);
  }

  return net_instaweb::StrCat(
      is_https ? "https://" : "http://", host, port_string,
      ngx_http_pagespeed_str_to_string_piece(r->unparsed_uri));
}

// Get the context for this request.  ngx_http_pagespeed_create_request_context
// should already have been called to create it.
ngx_http_pagespeed_request_ctx_t*
ngx_http_pagespeed_get_request_context(ngx_http_request_t* r) {
  return static_cast<ngx_http_pagespeed_request_ctx_t*>(
      ngx_http_get_module_ctx(r, ngx_pagespeed));
}

// Initialize the ngx_http_pagespeed_srv_conf_t by allocating and configuring
// the long-lived objects it contains.
// TODO(jefftk): This shouldn't be done on the first request but instead
// when we're done processing the configuration.
void
ngx_http_pagespeed_initialize_server_context(
    ngx_http_pagespeed_srv_conf_t* cfg) {
  net_instaweb::NgxRewriteDriverFactory::Initialize();
  // TODO(jefftk): We should call NgxRewriteDriverFactory::Terminate() when
  // we're done with it.

  CHECK(cfg->options != NULL);

  cfg->handler = new net_instaweb::GoogleMessageHandler();
  cfg->driver_factory = new net_instaweb::NgxRewriteDriverFactory();
  cfg->driver_factory->set_filename_prefix(cfg->options->file_cache_path());
  cfg->server_context = new net_instaweb::NgxServerContext(cfg->driver_factory);

  // The server context sets some options when we call global_options().  So let
  // it do that, then merge in options we got from parsing the config file.
  // Once we do that we're done with cfg->options.
  cfg->server_context->global_options()->Merge(*cfg->options);
  delete cfg->options;
  cfg->options = NULL;

  cfg->driver_factory->InitServerContext(cfg->server_context);
  cfg->proxy_fetch_factory =
      new net_instaweb::ProxyFetchFactory(cfg->server_context);
}

// Returns:
//   NGX_OK: pagespeed is done, request complete
//   NGX_AGAIN: pagespeed still working, needs to be called again later
//   NGX_ERROR: error
ngx_int_t
ngx_http_pagespeed_update(ngx_http_pagespeed_request_ctx_t* ctx,
                          ngx_event_t* ev) {
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
      perror("ngx_http_pagespeed_connection_read_handler");
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
ngx_http_pagespeed_writer(ngx_http_request_t* r)
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

    int rc = ngx_http_output_filter(r, NULL);

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
    r->write_event_handler = ngx_http_pagespeed_writer;

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
ngx_http_pagespeed_connection_read_handler(ngx_event_t* ev) {
  CHECK(ev != NULL);

  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  CHECK(c != NULL);

  ngx_http_pagespeed_request_ctx_t* ctx =
      static_cast<ngx_http_pagespeed_request_ctx_t*>(c->data);
  CHECK(ctx != NULL);

  int rc = ngx_http_pagespeed_update(ctx, ev);
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
    ngx_http_pagespeed_set_buffered(ctx->r, false);
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
ngx_http_pagespeed_create_connection(ngx_http_pagespeed_request_ctx_t* ctx) {
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
  c->read->handler = ngx_http_pagespeed_connection_read_handler;
  ngx_add_event(c->read, NGX_READ_EVENT, 0);

  return NGX_OK;
}

// Set us up for processing a request.
ngx_int_t
ngx_http_pagespeed_create_request_context(ngx_http_request_t* r,
                                          bool is_resource_fetch) {
  fprintf(stderr, "ngx_http_pagespeed_create_request_context\n");
  ngx_http_pagespeed_srv_conf_t* cfg =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(
          ngx_http_get_module_srv_conf(r, ngx_pagespeed));

  if (cfg->driver_factory == NULL) {
    // This is the first request handled by this server block.
    ngx_http_pagespeed_initialize_server_context(cfg);
  }

  GoogleString url_string = ngx_http_pagespeed_determine_url(r);
  net_instaweb::GoogleUrl url(url_string);

  if (!url.is_valid()) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "invalid url");

    // Let nginx deal with the error however it wants; we will see a NULL ctx in
    // the body filter or content handler and do nothing.
    return is_resource_fetch ? NGX_DECLINED : NGX_OK;
  }

  if (is_resource_fetch && !cfg->server_context->IsPagespeedResource(url)) {
    DBG(r, "Passing on content handling for non-pagespeed resource '%s'",
        url_string.c_str());
    return NGX_DECLINED;
  }

  int file_descriptors[2];
  int rc = pipe(file_descriptors);
  if (rc != 0) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "pipe() failed");
    return NGX_ERROR;
  }

  if (ngx_nonblocking(file_descriptors[0]) == -1) {
      ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                    ngx_nonblocking_n " pipe[0] failed");
      return NGX_ERROR;
  }

  if (ngx_nonblocking(file_descriptors[1]) == -1) {
      ngx_log_error(NGX_LOG_EMERG, r->connection->log, ngx_socket_errno,
                    ngx_nonblocking_n " pipe[1] failed");
      return NGX_ERROR;
  }

  ngx_http_pagespeed_request_ctx_t* ctx =
      new ngx_http_pagespeed_request_ctx_t();
  ctx->cfg = cfg;
  ctx->r = r;
  ctx->pipe_fd = file_descriptors[0];
  ctx->is_resource_fetch = is_resource_fetch;
  ctx->write_pending = false;
  ctx->pagespeed_connection = NULL;

  rc = ngx_http_pagespeed_create_connection(ctx);
  if (rc != NGX_OK) {
    close(file_descriptors[1]);

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ngx_http_pagespeed_create_request_context: "
                  "no pagespeed connection.");
    ngx_http_pagespeed_release_request_context(ctx);
    return NGX_ERROR;
  }

  // Deletes itself when HandleDone is called, which happens when we call Done()
  // on the proxy fetch below.
  ctx->base_fetch = new net_instaweb::NgxBaseFetch(r, file_descriptors[1]);

  // These are the options we use unless there are custom ones for this request.
  net_instaweb::RewriteOptions* global_options =
      ctx->cfg->server_context->global_options();

  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  //
  // Sets option from request headers and url.
  net_instaweb::ServerContext::OptionsBoolPair query_options_success =
      ctx->cfg->server_context->GetQueryOptions(
          &url, ctx->base_fetch->request_headers(), NULL);
  bool get_query_options_success = query_options_success.second;
  if (!get_query_options_success) {
    // Failed to parse query params or request headers.
    // TODO(jefftk): send a helpful error message to the visitor.
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ngx_http_pagespeed_create_request_context: "
                  "parsing headers or query params failed.");
    ngx_http_pagespeed_release_request_context(ctx);
    return NGX_ERROR;

  }

  // Will be NULL if there aren't any options set with query params or in
  // headers.
  net_instaweb::RewriteOptions* custom_options = query_options_success.first;

  if (custom_options != NULL) {
    // We need to combine the custom options with the global options, but
    // because merging is order dependent we can't just do a simple:
    //   custom_options->Merge(global_options);
    net_instaweb::RewriteOptions* query_options = custom_options;
    custom_options = global_options->Clone();
    custom_options->Merge(*query_options);
    delete query_options;
  }

  // This code is based off of ProxyInterface::ProxyRequestCallback and
  // ProxyFetchFactory::StartNewProxyFetch

  // If the global options say we're running furious (the experiment framework)
  // then clone them into custom_options so we can manipulate custom options
  // without affecting the global options.
  //
  // If custom_options were set above in GetQueryOptions() don't run furious.
  // We don't want experiments to be contaminated with unexpected settings.
  if (custom_options == NULL && global_options->running_furious()) {
    custom_options = global_options->Clone();
    custom_options->set_need_to_store_experiment_data(
        ctx->cfg->server_context->furious_matcher()->ClassifyIntoExperiment(
            *ctx->base_fetch->request_headers(), custom_options));
  }

  // If we have custom options then run if they say pagespeed is enabled.
  // Otherwise check the global options.
  if ((custom_options && !custom_options->enabled()) ||
      (!custom_options && !global_options->enabled())) {
    ngx_http_pagespeed_release_request_context(ctx);
    return NGX_DECLINED;
  }

  // TODO(jefftk): port ProxyInterface::InitiatePropertyCacheLookup so that we
  // have the propery cache in nginx.

  if (is_resource_fetch) {
    // TODO(jefftk): Set using_spdy appropriately.  See
    // ProxyInterface::ProxyRequestCallback
    net_instaweb::ResourceFetch::Start(
        url, custom_options /* null if there aren't custom options */,
        false /* using_spdy */, ctx->cfg->server_context, ctx->base_fetch);
  } else {
    // If we don't have custom options we can use NewRewriteDriver which reuses
    // rewrite drivers and so is faster because there's no wait to construct
    // them.  Otherwise we have to build a new one every time.
    net_instaweb::RewriteDriver* driver;
    if (custom_options == NULL) {
      driver = ctx->cfg->server_context->NewRewriteDriver();
    } else {
      // NewCustomRewriteDriver takes ownership of custom_options.
      driver = ctx->cfg->server_context->NewCustomRewriteDriver(
          custom_options);
    }
    driver->set_log_record(ctx->base_fetch->log_record());

    // TODO(jefftk): FlushEarlyFlow would go here.

    // Will call StartParse etc.  The rewrite driver will take care of deleting
    // itself if necessary.
    ctx->proxy_fetch = ctx->cfg->proxy_fetch_factory->CreateNewProxyFetch(
        url_string, ctx->base_fetch, driver,
        NULL /* property_callback */,
        NULL /* original_content_fetch */);
  }

  // Set up a cleanup handler on the request.
  ngx_http_cleanup_t* cleanup = ngx_http_cleanup_add(r, 0);
  if (cleanup == NULL) {
    ngx_http_pagespeed_release_request_context(ctx);
    return NGX_ERROR;
  }
  cleanup->handler = ngx_http_pagespeed_release_request_context;
  cleanup->data = ctx;
  ngx_http_set_ctx(r, ctx, ngx_pagespeed);

  return NGX_OK;
}

// Send each buffer in the chain to the proxy_fetch for optimization.
// Eventually it will make it's way, optimized, to base_fetch.
void
ngx_http_pagespeed_send_to_pagespeed(
    ngx_http_request_t* r,
    ngx_http_pagespeed_request_ctx_t* ctx,
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
                            ctx->cfg->handler);

    // We're done with buffers as we pass them through, so mark them as sent as
    // we go.
    cur->buf->pos = cur->buf->last;
  }

  if (last_buf) {
    ctx->proxy_fetch->Done(true /* success */);
    ctx->proxy_fetch = NULL;  // ProxyFetch deletes itself on Done().
  } else {
    // TODO(jefftk): Decide whether Flush() is warranted here.
    ctx->proxy_fetch->Flush(ctx->cfg->handler);
  }
}

ngx_int_t
ngx_http_pagespeed_body_filter(ngx_http_request_t* r, ngx_chain_t* in) {
  ngx_http_pagespeed_request_ctx_t* ctx =
      ngx_http_pagespeed_get_request_context(r);

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
    ngx_http_pagespeed_send_to_pagespeed(r, ctx, in);
  }

  ngx_http_pagespeed_set_buffered(r, true);
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

ngx_int_t
ngx_http_pagespeed_header_filter(ngx_http_request_t* r) {
  ngx_http_pagespeed_request_ctx_t* ctx =
      ngx_http_pagespeed_get_request_context(r);

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
          ngx_http_pagespeed_str_to_string_piece(r->headers_out.content_type));
  if (content_type == NULL || !content_type->IsHtmlLike()) {
    // Unknown or otherwise non-html content type: skip it.
    return ngx_http_next_header_filter(r);
  }

  int rc = ngx_http_pagespeed_create_request_context(
      r, false /* not a resource fetch */);
  if (rc == NGX_DECLINED) {
    // ModPagespeed=off
    return ngx_http_next_header_filter(r);
  } else if (rc != NGX_OK) {
    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    return rc;
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
  // Based on ngx_http_add_cache_control.
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
  ngx_str_set(&cache_control_headers[0]->value, "max-age=0, no-cache");

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

// Handle requests for resources like example.css.pagespeed.ce.LyfcM6Wulf.css
ngx_int_t
ngx_http_pagespeed_content_handler(ngx_http_request_t* r) {
  // TODO(jefftk): return NGX_DECLINED for non-get non-head requests.

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http pagespeed handler \"%V\"", &r->uri);

  int rc = ngx_http_pagespeed_create_request_context(
      r, true /* is a resource fetch */);
  if (rc != NGX_OK) {
    return rc;  // rc will be NGX_DECLINED if it's not a pagespeed resource.
  }

  ngx_http_pagespeed_request_ctx_t* ctx =
      ngx_http_pagespeed_get_request_context(r);
  CHECK(ctx != NULL);

  // Tell nginx we're still working on this one.
  r->count++;

  return NGX_DONE;
}

ngx_int_t
ngx_http_pagespeed_init(ngx_conf_t* cf) {
  ngx_http_pagespeed_srv_conf_t* cfg =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(
          ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));

  // Only put register pagespeed code to run if there was a "pagespeed"
  // configuration option set in the config file.  With "pagespeed off" we
  // consider every request and choose not to do anything, while with no
  // "pagespeed" directives we won't have any effect after nginx is done loading
  // its configuration.
  if (cfg->options != NULL) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_pagespeed_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_pagespeed_body_filter;

    ngx_http_core_main_conf_t* cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));
    ngx_http_handler_pt* h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));
    if (h == NULL) {
      return NGX_ERROR;
    }
    *h = ngx_http_pagespeed_content_handler;
  }

  return NGX_OK;
}

ngx_http_module_t ngx_http_pagespeed_module = {
  NULL,  // preconfiguration
  ngx_http_pagespeed_init,  // postconfiguration

  NULL,  // create main configuration
  NULL,  // init main configuration

  ngx_http_pagespeed_create_srv_conf,  // create server configuration
  ngx_http_pagespeed_merge_srv_conf,  // merge server configuration

  NULL,  // create location configuration
  NULL  // merge location configuration
};

}  // namespace

ngx_module_t ngx_pagespeed = {
  NGX_MODULE_V1,
  &ngx_http_pagespeed_module,
  ngx_http_pagespeed_commands,
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

