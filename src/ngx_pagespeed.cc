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

/*
 * Usage:
 *   server {
 *     pagespeed    on|off;
 *   }
 */

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include "ngx_rewrite_driver_factory.h"
#include "ngx_base_fetch.h"

#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/rewriter/public/furious_matcher.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/null_message_handler.h"

extern ngx_module_t ngx_pagespeed;

// Hacks for debugging.
#define DBG(r, args...)                                       \
  ngx_log_error(NGX_LOG_ALERT, (r)->connection->log, 0, args)
#define CDBG(cf, args...)                                     \
  ngx_conf_log_error(NGX_LOG_ALERT, cf, 0, args)

typedef struct {
  ngx_flag_t active;
  ngx_str_t cache_dir;
  net_instaweb::NgxRewriteDriverFactory* driver_factory;
  net_instaweb::ServerContext* server_context;
} ngx_http_pagespeed_srv_conf_t;

typedef struct {
  net_instaweb::RewriteDriver* driver;
  net_instaweb::ProxyFetch* proxy_fetch;
} ngx_http_pagespeed_request_ctx_t;

static ngx_command_t ngx_http_pagespeed_commands[] = {
  { ngx_string("pagespeed"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_SRV_CONF_OFFSET,
    offsetof(ngx_http_pagespeed_srv_conf_t, active),
    NULL },

  { ngx_string("pagespeed_cache"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_SRV_CONF_OFFSET,
    offsetof(ngx_http_pagespeed_srv_conf_t, cache_dir),
    NULL },

  ngx_null_command
};

static StringPiece
ngx_http_pagespeed_str_to_string_piece(ngx_str_t* s) {
  return StringPiece(reinterpret_cast<char*>(s->data), s->len);
}

static void*
ngx_http_pagespeed_create_srv_conf(ngx_conf_t* cf)
{
  ngx_http_pagespeed_srv_conf_t* conf;

  conf = static_cast<ngx_http_pagespeed_srv_conf_t*>(
      ngx_pcalloc(cf->pool, sizeof(ngx_http_pagespeed_srv_conf_t)));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }
  conf->active = NGX_CONF_UNSET;

  // set by ngx_pcalloc():
  //   conf->cache_dir = { 0, NULL };
  //   conf->driver_factory = NULL;
  //   conf->server_context = NULL;

  return conf;
}

static char*
ngx_http_pagespeed_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child)
{
  ngx_http_pagespeed_srv_conf_t* prev =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(parent);
  ngx_http_pagespeed_srv_conf_t* conf =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(child);

  ngx_conf_merge_value(conf->active, prev->active, 0);  // Default off.
  ngx_conf_merge_str_value(conf->cache_dir, prev->cache_dir, "");

  return NGX_CONF_OK;
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

// Add a buffer to the end of the buffer chain indicating that we were processed
// through ngx_pagespeed.
static ngx_int_t
ngx_http_pagespeed_note_processed(ngx_http_request_t* r, ngx_chain_t* in) {
  // Find the end of the buffer chain.
  ngx_chain_t* chain_link;
  int chain_contains_last_buffer = 0;
  for (chain_link = in; chain_link != NULL; chain_link = chain_link->next) {
    if (chain_link->buf->last_buf) {
      chain_contains_last_buffer = 1;
      if (chain_link->next != NULL) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                      "Chain link thinks its last but has a child.");
        return NGX_ERROR;
      }
      break;  // Chain link now is the last link in the chain.
    }
  }

  if (!chain_contains_last_buffer) {
    // None of the buffers had last_buf set, meaning we have an incomplete chain
    // and are still waiting to get the final buffer.  Wait until we're called
    // again with the last buffer.
    return NGX_OK;
  }

  // Prepare a new buffer to put the note into.
  ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(r->pool));
  if (b == NULL) {
    return NGX_ERROR;
  }

  // Write to the new buffer.
  const char note[] = "<!-- Processed through ngx_pagespeed using PSOL version "
      MOD_PAGESPEED_VERSION_STRING " -->\n";
  int note_len = strlen(note);
  b->start = b->pos = static_cast<u_char*>(ngx_pnalloc(r->pool, note_len));
  strncpy((char*)b->pos, note, note_len);
  b->end = b->last = b->pos + note_len;
  b->temporary = 1;

  // Link the new buffer into the buffer chain.
  ngx_chain_t* added_link = static_cast<ngx_chain_t*>(
      ngx_alloc_chain_link(r->pool));
  if (added_link == NULL) {
    return NGX_ERROR;
  }

  added_link->buf = b;

  // Add our new link to the buffer chain.
  added_link->next = NULL;
  chain_link->next = added_link;

  // Mark our new link as the end of the chain.
  chain_link->buf->last_buf = 0;
  added_link->buf->last_buf = 1;
  chain_link->buf->last_in_chain = 0;
  added_link->buf->last_in_chain = 1;

  return NGX_OK;
}

static void
ngx_http_pagespeed_release_request_context(
    ngx_http_request_t* r, ngx_http_pagespeed_request_ctx_t* ctx) {

    // release request context
    ngx_http_set_ctx(r, NULL, ngx_pagespeed);
    delete ctx;
}


static GoogleString
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
      ngx_http_pagespeed_str_to_string_piece(&r->headers_in.server);
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
    host = ngx_http_pagespeed_str_to_string_piece(&s);
  }

  return net_instaweb::StrCat(
      is_https ? "https://" : "http://", host, port_string,
      ngx_http_pagespeed_str_to_string_piece(&r->unparsed_uri));
}

// Get the context for this request.  ngx_http_pagespeed_create_request_context
// should already have been called to create it.
static ngx_http_pagespeed_request_ctx_t*
ngx_http_pagespeed_get_request_context(ngx_http_request_t* r) {
  return static_cast<ngx_http_pagespeed_request_ctx_t*>(
      ngx_http_get_module_ctx(r, ngx_pagespeed));
}

// Initialize the ngx_http_pagespeed_srv_conf_t by allocating and configuring
// the long-lived objects it contains.
// TODO(jefftk): This shouldn't be done on the first request but instead
// when we're done processing the configuration.
static void
ngx_http_pagespeed_initialize_server_context(
    ngx_http_pagespeed_srv_conf_t* cfg) {
  net_instaweb::NgxRewriteDriverFactory::Initialize();
  // TODO(jefftk): We should call NgxRewriteDriverFactory::Terminate() when
  // we're done with it.

  cfg->driver_factory = new net_instaweb::NgxRewriteDriverFactory();
  cfg->driver_factory->set_filename_prefix(
      ngx_http_pagespeed_str_to_string_piece(&cfg->cache_dir));
  cfg->server_context = cfg->driver_factory->CreateServerContext();

  // In real use, with filters coming from the user, this would be some other
  // kind of message handler that actually sent errors back to the user.
  net_instaweb::NullMessageHandler handler;

  // Turn on some filters so we can see if this is working.  These filters are
  // specifically chosen as ones that don't make requests for subresources or do
  // work that needs to complete asynchronously.  They should be fast enough to
  // run in the line of the request.
  net_instaweb::RewriteOptions* global_options =
      cfg->server_context->global_options();
  global_options->SetRewriteLevel(net_instaweb::RewriteOptions::kPassThrough);
  global_options->EnableFiltersByCommaSeparatedList(
      "collapse_whitespace,remove_comments,remove_quotes", &handler);
}


// Set us up for processing a request.  When the request finishes, call
// ngx_http_pagespeed_release_request_context.
static ngx_int_t
ngx_http_pagespeed_create_request_context(ngx_http_request_t* r) {
  ngx_http_pagespeed_srv_conf_t* cfg =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(
          ngx_http_get_module_srv_conf(r, ngx_pagespeed));

  // TODO(jefftk): make a proper async_fetch out of r
  // Deletes itself when HandleDone is called, which happens when we call Done()
  // on the proxy fetch below.
  net_instaweb::NgxBaseFetch* base_fetch = new net_instaweb::NgxBaseFetch();

  if (cfg->driver_factory == NULL) {
    // This is the first request handled by this server block.
    ngx_http_pagespeed_initialize_server_context(cfg);
  }

  // Pull the server context out of the per-server config.
  net_instaweb::ServerContext* server_context =
      static_cast<ngx_http_pagespeed_srv_conf_t*>(
          ngx_http_get_module_srv_conf(r, ngx_pagespeed))->server_context;

  if (server_context == NULL) {
    ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                  "ServerContext should have been initialized.");
    return NGX_ERROR;
  }

  GoogleString url_string = ngx_http_pagespeed_determine_url(r);
  net_instaweb::GoogleUrl request_url = net_instaweb::GoogleUrl(url_string);

  ngx_http_pagespeed_request_ctx_t* ctx =
      new ngx_http_pagespeed_request_ctx_t();

  // This code is based off of ProxyInterface::ProxyRequestCallback and
  // ProxyFetchFactory::StartNewProxyFetch

  // If the global options say we're running furious (the experiment framework)
  // then clone them into custom_options so we can manipulate custom options
  // without affecting the global options.
  net_instaweb::RewriteOptions* custom_options = NULL;
  net_instaweb::RewriteOptions* global_options =
      cfg->server_context->global_options();
  if (global_options->running_furious()) {
    custom_options = global_options->Clone();
    custom_options->set_need_to_store_experiment_data(
        cfg->server_context->furious_matcher()->ClassifyIntoExperiment(
            *base_fetch->request_headers(), custom_options));
  }

  // TODO(jefftk): port ProxyInterface::InitiatePropertyCacheLookup so that we
  // have the propery cache in nginx.
  net_instaweb::ProxyFetchPropertyCallbackCollector* property_callback = NULL;

  // If we don't have custom options we can use NewRewriteDriver which reuses
  // rewrite drivers and so is faster because there's no wait to construct
  // them.  Otherwise we have to build a new one every time.
  if (custom_options == NULL) {
    ctx->driver = cfg->server_context->NewRewriteDriver();
  } else {
    // NewCustomRewriteDriver takes ownership of custom_options.
    ctx->driver = cfg->server_context->NewCustomRewriteDriver(custom_options);
  }
  ctx->driver->set_log_record(base_fetch->log_record());

  // TODO(jefftk): FlushEarlyFlow would go here.

  // Will call StartParse etc.  Takes ownership of property_callback.
  ctx->proxy_fetch = new net_instaweb::ProxyFetch(
      url_string, false /* cross_domain */, property_callback, base_fetch,
      NULL /* original_content_fetch */, ctx->driver, cfg->server_context,
      NULL /* timer */, NULL /* ProxyFetchFactory */);

  ngx_http_set_ctx(r, ctx, ngx_pagespeed);
  return NGX_OK;
}

// Replace each buffer chain with a new one that's been optimized.
static ngx_int_t
ngx_http_pagespeed_optimize_and_replace_buffer(ngx_http_request_t* r,
                                               ngx_chain_t* in) {
  ngx_http_pagespeed_request_ctx_t* ctx =
      ngx_http_pagespeed_get_request_context(r);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  ngx_chain_t* cur;
  int last_buf = 0;
  int last_in_chain = 0;
  for (cur = in; cur != NULL;) {
    last_buf = cur->buf->last_buf;
    last_in_chain = cur->buf->last_in_chain;

    ctx->proxy_fetch->Write(StringPiece(reinterpret_cast<char*>(cur->buf->pos),
                                        cur->buf->last - cur->buf->pos));

    // We're done with buffers as we pass them through, so free them and their
    // chain links as we go.
    ngx_chain_t* next_link = cur->next;
    ngx_pfree(r->pool, cur->buf);
    ngx_pfree(r->pool, cur);
    cur = next_link;
  }
  in = NULL;  // We freed all the buffers.

  if (last_buf) {
    ctx->proxy_fetch->Done(true /* success */);
    ngx_http_pagespeed_release_request_context(r, ctx);
    ctx = NULL;
  } else {
    // TODO(jefftk): Decide whether ctx->proxy_fetch->Flush is warranted here.
  }

  return NGX_DONE; // No output.

  // In the future I think we need to return NGX_AGAIN after adding some sort of
  // notification pipe to nginx's main event loop so it knows when to wake us
  // back up.  For now we're just dumping all output from pagespeed to the error
  // log, so don't worry about it.
}

static ngx_int_t
ngx_http_pagespeed_body_filter(ngx_http_request_t* r, ngx_chain_t* in)
{
  ngx_int_t rc;

  rc = ngx_http_pagespeed_optimize_and_replace_buffer(r, in);
  if (rc != NGX_OK) {
    return rc;
  }

  rc = ngx_http_pagespeed_note_processed(r, in);
  if (rc != NGX_OK) {
    return rc;
  }

  return ngx_http_next_body_filter(r, in);
}

static ngx_int_t
ngx_http_pagespeed_header_filter(ngx_http_request_t* r)
{
  // We're modifying content below, so switch to 'Transfer-Encoding: chunked'
  // and calculate on the fly.
  ngx_http_clear_content_length(r);

  r->filter_need_in_memory = 1;

  int rc = ngx_http_pagespeed_create_request_context(r);
  if (rc != NGX_OK) {
    return rc;
  }

  return ngx_http_next_header_filter(r);
}

static ngx_int_t
ngx_http_pagespeed_init(ngx_conf_t* cf)
{
  ngx_http_pagespeed_srv_conf_t* pagespeed_config;
  pagespeed_config = static_cast<ngx_http_pagespeed_srv_conf_t*>(
    ngx_http_conf_get_module_srv_conf(cf, ngx_pagespeed));

  if (pagespeed_config->active) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_pagespeed_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_pagespeed_body_filter;
  }

  return NGX_OK;
}

static ngx_http_module_t ngx_http_pagespeed_module = {
  NULL,  // preconfiguration
  ngx_http_pagespeed_init,  // postconfiguration

  NULL,  // create main configuration
  NULL,  // init main configuration

  ngx_http_pagespeed_create_srv_conf,  // create server configuration
  ngx_http_pagespeed_merge_srv_conf,  // merge server configuration

  NULL,  // create location configuration
  NULL  // merge location configuration
};

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

