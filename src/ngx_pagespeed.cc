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

#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/string.h"
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
} ngx_http_pagespeed_loc_conf_t;

typedef struct {
  scoped_ptr<net_instaweb::NgxRewriteDriverFactory> driver_factory;
  net_instaweb::ServerContext* server_context;
} ngx_http_pagespeed_module_ctx_t;

typedef struct {
  net_instaweb::RewriteDriver* driver;
  scoped_ptr<GoogleString> output;
  scoped_ptr<net_instaweb::StringWriter> writer;
} ngx_http_pagespeed_request_ctx_t;

// TODO(jefftk): Giant hack.  Need to make this not be global.
static ngx_http_pagespeed_module_ctx_t* context = NULL;

static ngx_command_t ngx_http_pagespeed_commands[] = {
  { ngx_string("pagespeed"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_pagespeed_loc_conf_t, active),
    NULL },

  { ngx_string("pagespeed_cache"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_pagespeed_loc_conf_t, cache_dir),
    NULL },

  ngx_null_command
};

static void*
ngx_http_pagespeed_create_loc_conf(ngx_conf_t* cf)
{
  ngx_http_pagespeed_loc_conf_t* conf;

  conf = static_cast<ngx_http_pagespeed_loc_conf_t*>(
      ngx_pcalloc(cf->pool, sizeof(ngx_http_pagespeed_loc_conf_t)));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }
  conf->active = NGX_CONF_UNSET;

  // set by ngx_pcalloc():
  //   conf->cache_dir = { 0, NULL };

  return conf;
}

static char*
ngx_http_pagespeed_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child)
{
  ngx_http_pagespeed_loc_conf_t* prev =
      static_cast<ngx_http_pagespeed_loc_conf_t*>(parent);
  ngx_http_pagespeed_loc_conf_t* conf =
      static_cast<ngx_http_pagespeed_loc_conf_t*>(child);

  ngx_conf_merge_value(conf->active, prev->active, 0);  // Default off.
  ngx_conf_merge_str_value(conf->cache_dir, prev->cache_dir, "");

  return NGX_CONF_OK;
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_int_t
ngx_http_pagespeed_header_filter(ngx_http_request_t* r)
{
  // We're modifying content below, so switch to 'Transfer-Encoding: chunked'
  // and calculate on the fly.
  ngx_http_clear_content_length(r);
  return ngx_http_next_header_filter(r);
}

// Add a buffer to the end of the buffer chain indicating that we were processed
// through ngx_pagespeed.
static ngx_int_t
ngx_http_pagespeed_note_processed(ngx_http_request_t* r,
                                            ngx_chain_t* in) {
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

// Get the context for this request.  If one doesn't exist yet, create it.  When
// the request finishes, call ngx_http_pagespeed_release_request_context.
static ngx_http_pagespeed_request_ctx_t*
ngx_http_pagespeed_request_context(ngx_http_request_t* r) {
  ngx_http_pagespeed_request_ctx_t* ctx =
      static_cast<ngx_http_pagespeed_request_ctx_t*>(
          ngx_http_get_module_ctx(r, ngx_pagespeed));

  if (ctx != NULL) {
    return ctx;
  }

  // Set up things we do once at the beginning of the request.

  // TODO(jefftk): figure out how to get the real url out of r.  Because we're
  // currently blocking nginx's main event loop, however, fixing this now would
  // unbreak the code that makes a fetch back to ourselves and make us
  // deadlock.
  StringPiece url("http://localhost");

  ctx = new ngx_http_pagespeed_request_ctx_t;
  ctx->driver = context->server_context->NewRewriteDriver();

  // TODO(jefftk): replace this with a writer that generates proper nginx
  // buffers and puts them in the chain.  Or avoids the double
  // copy some other way.
  ctx->output.reset(new GoogleString());
  ctx->writer.reset(new net_instaweb::StringWriter(ctx->output.get()));
  ctx->driver->SetWriter(ctx->writer.get());

  // For testing we always want to perform any optimizations we can, so we
  // wait until everything is done rather than using a deadline, the way we
  // want to eventually.
  ctx->driver->set_fully_rewrite_on_flush(true);

  ngx_http_set_ctx(r, ctx, ngx_pagespeed);
  bool ok = ctx->driver->StartParse(url);
  if (!ok) {
    ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                  "Failed to StartParse on url %*s",
                  url.size(), url.data());
    return NULL;
  }

  return ctx;
}

// Replace each buffer chain with a new one that's been optimized.
static ngx_int_t
ngx_http_pagespeed_optimize_and_replace_buffer(ngx_http_request_t* r,
                                               ngx_chain_t* in) {
  ngx_http_pagespeed_request_ctx_t* ctx = ngx_http_pagespeed_request_context(r);
  if (ctx == NULL) {
    return NGX_ERROR;
  }

  u_char* file_contents = NULL;
  StringPiece buffer_contents;
  ngx_chain_t* cur;
  int last_buf = 0;
  int last_in_chain = 0;
  for (cur = in; cur != NULL;) {
    last_buf = cur->buf->last_buf;
    last_in_chain = cur->buf->last_in_chain;
    if (cur->buf->file != NULL) {
      ssize_t file_size = cur->buf->file_last - cur->buf->file_pos;
      // TODO(jefftk): if file_size is big enough can we still read it all at
      // once?
      file_contents = static_cast<u_char*>(ngx_pnalloc(r->pool, file_size));
      if (file_contents == NULL) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                      "failed to allocate %d bytes", file_size);
        return NGX_ERROR;
      }
      ssize_t n = ngx_read_file(cur->buf->file, file_contents, file_size,
                                cur->buf->file_pos);
      if (n != file_size) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                      "Failed to read file; got %d bytes expected %d bytes",
                      n, file_size);
        return NGX_ERROR;
      }
      buffer_contents = StringPiece(reinterpret_cast<char*>(file_contents),
                                    file_size);
    } else {
      buffer_contents = StringPiece(reinterpret_cast<char*>(cur->buf->pos),
                                    cur->buf->last - cur->buf->pos);
    }
    ctx->driver->ParseText(buffer_contents);
    if (file_contents != NULL) {
      // TODO(jefftk): the pfree calls fail with NGX_DECLINED, but I think
      // that's just NGINX only bothering to free large allocs.
      ngx_pfree(r->pool, file_contents);
      file_contents = NULL;
    }
    buffer_contents= StringPiece(NULL, 0);

    // We're done with buffers as we pass them to the rewrite driver, so free
    // them and their chain links as we go.  Don't free the first buffer (in)
    // which we need below.
    ngx_chain_t* next_link = cur->next;
    if (cur != in) {
      ngx_pfree(r->pool, cur->buf);
      ngx_pfree(r->pool, cur);
    }
    cur = next_link;
  }
  in->next = NULL;  // We freed all the later buffers.

  // Prepare the new buffer.
  ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(r->pool));
  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                  "failed to allocate buffer");

    return NGX_ERROR;
  }

  b->temporary = 1;
  b->last_buf = last_buf;
  b->last_in_chain = last_in_chain;
  in->next = NULL;

  // replace the first link's buffer with our new one.
  ngx_pfree(r->pool, in->buf);
  in->buf = b;

  if (last_buf) {
    ctx->driver->FinishParse();
  } else {
    ctx->driver->Flush();
  }

  // TODO(jefftk): need to store how much went out on previous flushes and only
  // copy here the new stuff.  Keep the count in the request context.
  b->pos = b->start = static_cast<u_char*>(
      ngx_pnalloc(r->pool, ctx->output->length()));
  if (b->pos == NULL) {
    ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0,
                  "failed to allocate %d bytes", ctx->output->length());
    return NGX_ERROR;
  }
  ctx->output->copy(reinterpret_cast<char*>(b->pos), ctx->output->length());
  b->last = b->end = b->pos + ctx->output->length();

  if (last_buf) {
    ngx_http_pagespeed_release_request_context(r, ctx);
    ctx = NULL;
  }

  return NGX_OK;
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
ngx_http_pagespeed_init(ngx_conf_t* cf)
{
  ngx_http_pagespeed_loc_conf_t* pagespeed_config;
  pagespeed_config = static_cast<ngx_http_pagespeed_loc_conf_t*>(
    ngx_http_conf_get_module_loc_conf(cf, ngx_pagespeed));

  if (pagespeed_config->active) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_pagespeed_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_pagespeed_body_filter;

    // TODO(jefftk): move this to be per-server-block and not global.
    context = new ngx_http_pagespeed_module_ctx_t();

    net_instaweb::NgxRewriteDriverFactory::Initialize();
    // TODO(jefftk): We should call NgxRewriteDriverFactory::Terminate() when
    // we're done with it.
    context->driver_factory.reset(new net_instaweb::NgxRewriteDriverFactory());
    context->driver_factory->set_filename_prefix(StringPiece(
        reinterpret_cast<char*>(pagespeed_config->cache_dir.data),
        pagespeed_config->cache_dir.len));
    context->server_context = context->driver_factory->CreateServerContext();

    net_instaweb::NullMessageHandler handler;

    // Turn on some filters so we can see if this is working.
    net_instaweb::RewriteOptions* global_options =
        context->server_context->global_options();
    global_options->EnableFiltersByCommaSeparatedList(
        "collapse_whitespace,remove_comments,remove_quotes", &handler);
  }

  return NGX_OK;
}

static ngx_http_module_t ngx_http_pagespeed_module = {
  NULL,
  ngx_http_pagespeed_init,  // Post configuration.
  NULL,
  NULL,
  NULL,
  NULL,
  ngx_http_pagespeed_create_loc_conf,
  ngx_http_pagespeed_merge_loc_conf
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

