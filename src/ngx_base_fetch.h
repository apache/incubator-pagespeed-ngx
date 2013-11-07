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
// Notifies nginx via pipe to call CollectAccumulatedWrites() on flush.
//
//  - nginx creates a base fetch and passes it to a new proxy fetch.
//  - The proxy fetch manages rewriting and thread complexity, and through
//    several chained steps passes rewritten html to HandleWrite().
//  - Written data is buffered.
//  - When Flush() is called the base fetch writes a byte to a pipe nginx is
//    watching so nginx knows to call CollectAccumulatedWrites() to pick up the
//    rewritten html.
//  - When Done() is called the base fetch closes the pipe, which tells nginx to
//    make a final call to CollectAccumulatedWrites().
//
// This class is referred two in two places: the proxy fetch and nginx's
// request.  It must stay alive until both are finished.  The proxy fetch will
// call Done() to indicate this; nginx will call Release().  Once both Done()
// and Release() have been called this class will delete itself.

#ifndef NGX_BASE_FETCH_H_
#define NGX_BASE_FETCH_H_

extern "C" {
#include <ngx_http.h>
}

#include <pthread.h>

#include "ngx_pagespeed.h"

#include "ngx_server_context.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class NgxBaseFetch : public AsyncFetch {
 public:
  NgxBaseFetch(ngx_http_request_t* r, int pipe_fd,
               NgxServerContext* server_context,
               const RequestContextPtr& request_ctx,
               PreserveCachingHeaders preserve_caching_headers);
  virtual ~NgxBaseFetch();

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

  // Called by nginx when it's done with us.
  void Release();
  void set_handle_error(bool x) { handle_error_ = x; }

 private:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

  // Indicate to nginx that we would like it to call
  // CollectAccumulatedWrites().
  void RequestCollection();

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
  void DecrefAndDeleteIfUnreferenced();

  ngx_http_request_t* request_;
  GoogleString buffer_;
  NgxServerContext* server_context_;
  bool done_called_;
  bool last_buf_sent_;
  int pipe_fd_;
  // How many active references there are to this fetch. Starts at two,
  // decremented once when Done() is called and once when Release() is called.
  int references_;
  pthread_mutex_t mutex_;
  bool handle_error_;
  PreserveCachingHeaders preserve_caching_headers_;

  DISALLOW_COPY_AND_ASSIGN(NgxBaseFetch);
};

}  // namespace net_instaweb

#endif  // NGX_BASE_FETCH_H_
