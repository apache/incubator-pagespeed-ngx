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
//
// Collects output from pagespeed and buffers it until nginx asks for it.
// Notifies nginx via NgxEventConnection to call ReadCallback() when
// the headers are computed, when a flush should be performed, and when done.
//
//  - nginx creates a base fetch and passes it to a new proxy fetch.
//  - The proxy fetch manages rewriting and thread complexity, and through
//    several chained steps passes rewritten html to HandleWrite().
//  - Written data is buffered.
//  - When HandleHeadersComplete(), HandleFlush(), or HandleDone() is called by
//    PSOL, events are written to NgxEventConnection which will end up being
//    handled by ReadCallback() on nginx's thread.
//    When applicable, request processing will be continued via a call to
//    ps_base_fetch_handler().
//  - ps_base_fetch_handler() will pull the header and body bytes from PSOL
//    via CollectAccumulatedWrites() and write those to the module's output.
//
// This class is referred to in three places: the proxy fetch, nginx's request,
// and pending events written to the associated NgxEventConnection. It must stay
// alive until the proxy fetch and nginx request are finished, and no more
// events are pending.
//  - The proxy fetch will call Done() to indicate this.
//  - nginx will call Detach() when the associated request is handled
//    completely (e.g. the request context is about to be destroyed).
//  - ReadCallback() will call DecrementRefCount() on instances associated to
//    events it handles.
//
// When the last reference is dropped, this class will delete itself.
//
// TODO(jmarantz): consider sharing the cache-invalidation infrastructure
// with ApacheFetch, using a common base class.

#ifndef NGX_BASE_FETCH_H_
#define NGX_BASE_FETCH_H_

extern "C" {
#include <ngx_http.h>
}

#include <pthread.h>

#include "ngx_pagespeed.h"

#include "ngx_event_connection.h"
#include "ngx_server_context.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/headers.h"

namespace net_instaweb {

enum NgxBaseFetchType {
  kIproLookup,
  kHtmlTransform,
  kPageSpeedResource,
  kAdminPage,
  kPageSpeedProxy
};

class NgxBaseFetch : public AsyncFetch {
 public:
  NgxBaseFetch(StringPiece url, ngx_http_request_t* r,
               NgxServerContext* server_context,
               const RequestContextPtr& request_ctx,
               PreserveCachingHeaders preserve_caching_headers,
               NgxBaseFetchType base_fetch_type,
               const RewriteOptions* options);
  virtual ~NgxBaseFetch();

  // Statically initializes event_connection, require for PSOL and nginx to
  // communicate.
  static bool Initialize(ngx_cycle_t* cycle);

  // Attempts to finish up request processing queued up in the named pipe and
  // PSOL for a fixed amount of time. If time is up, a fast and rough shutdown
  // is attempted.
  // Statically terminates and NULLS event_connection.
  static void Terminate();

  static void ReadCallback(const ps_event_data& data);

  // Puts a chain in link_ptr if we have any output data buffered.  Returns
  // NGX_OK on success, NGX_ERROR on errors.  If there's no data to send, sends
  // data only if Done() has been called.  Indicates the end of output by
  // setting last_buf on the last buffer in the chain.
  //
  // Sets link_ptr to a chain of as many buffers are needed for the output.
  //
  // Called by nginx in response to seeing a byte on the pipe.
  ngx_int_t CollectAccumulatedWrites(ngx_chain_t** link_ptr);

  // Copies response headers into headers_out.
  //
  // Called by nginx before calling CollectAccumulatedWrites() for the first
  // time for resource fetches.  Not called at all for proxy fetches.
  ngx_int_t CollectHeaders(ngx_http_headers_out_t* headers_out);

  // Called by nginx to decrement the refcount.
  int DecrementRefCount();

  // Called by pagespeed to increment the refcount.
  int IncrementRefCount();

  // Detach() is called when the nginx side releases this base fetch. It
  // sets detached_ to true and decrements the refcount. We need to know
  // this to be able to handle events which nginx request context has been
  // released while the event was in-flight.
  void Detach() { detached_ = true; DecrementRefCount(); }

  bool detached() { return detached_; }

  ngx_http_request_t* request() { return request_; }
  NgxBaseFetchType base_fetch_type() { return base_fetch_type_; }

  bool IsCachedResultValid(const ResponseHeaders& headers) override;

 private:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

  // Indicate to nginx that we would like it to call
  // CollectAccumulatedWrites().
  void RequestCollection(char type);

  // Lock must be acquired first.
  // Returns:
  //   NGX_ERROR: failure
  //   NGX_AGAIN: still has buffer to send, need to checkout link_ptr
  //   NGX_OK: done, HandleDone has been called
  // Allocates an nginx buffer, copies our buffer_ contents into it, clears
  // buffer_.
  ngx_int_t CopyBufferToNginx(ngx_chain_t** link_ptr);

  void Lock();
  void Unlock();

  // Called by Done() and Release().  Decrements our reference count, and if
  // it's zero we delete ourself.
  int DecrefAndDeleteIfUnreferenced();

  static NgxEventConnection* event_connection;

  // Live count of NgxBaseFetch instances that are currently in use.
  static int active_base_fetches;

  GoogleString url_;
  ngx_http_request_t* request_;
  GoogleString buffer_;
  NgxServerContext* server_context_;
  const RewriteOptions* options_;
  bool need_flush_;
  bool done_called_;
  bool last_buf_sent_;
  // How many active references there are to this fetch. Starts at two,
  // decremented once when Done() is called and once when Detach() is called.
  // Incremented for each event written by pagespeed for this NgxBaseFetch, and
  // decremented on the nginx side for each event read for it.
  int references_;
  pthread_mutex_t mutex_;
  NgxBaseFetchType base_fetch_type_;
  PreserveCachingHeaders preserve_caching_headers_;
  // Set to true just before the nginx side releases its reference
  bool detached_;
  bool suppress_;

  DISALLOW_COPY_AND_ASSIGN(NgxBaseFetch);
};

}  // namespace net_instaweb

#endif  // NGX_BASE_FETCH_H_
