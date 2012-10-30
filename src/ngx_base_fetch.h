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

#ifndef NGX_BASE_FETCH_H_
#define NGX_BASE_FETCH_H_

extern "C" {
#include <ngx_http.h>
}

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class NgxBaseFetch : public AsyncFetch {
 public:
  NgxBaseFetch(ngx_http_request_t* r, int pipe_fd);
  virtual ~NgxBaseFetch();

  // Copies the response headers out of request_->headers_out->headers.
  void PopulateHeaders();

  // Puts a chain link in link_ptr if we have any output data buffered.  Returns
  // NGX_OK on success, NGX_ERROR on errors.  If there's no data to send, sends
  // data only if Done() has been called.  Indicates the end of output by
  // setting last_buf on the chain link
  //
  // Always sets link_ptr to a single chain link, never a full chain.
  //
  // Called by nginx.
  ngx_int_t CollectAccumulatedWrites(ngx_chain_t** link_ptr);

 private:
  ResponseHeaders response_headers_;
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success);

  // Indicate to nginx that we would like it to call
  // CollectAccumulatedWrites().
  void RequestCollection();

  ngx_http_request_t* request_;
  GoogleString buffer_;
  bool done_called_;
  int pipe_fd_;

  DISALLOW_COPY_AND_ASSIGN(NgxBaseFetch);
};

} // namespace net_instaweb

#endif  // NGX_BASE_FETCH_H_
