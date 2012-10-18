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

#ifndef NGX_BASE_FETCH_H_
#define NGX_BASE_FETCH_H_

extern "C" {
#include <ngx_http.h>
}

#include "net/instaweb/http/public/async_fetch.h"

namespace net_instaweb {

class NgxBaseFetch : public AsyncFetch {
 public:
  explicit NgxBaseFetch(ngx_http_request_t* r);
  virtual ~NgxBaseFetch();

  // Copies the response headers out of request_->headers_out->headers.
  void PopulateHeaders();
 private:
  ResponseHeaders response_headers_;
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success);
  
  ngx_http_request_t* request_;
  DISALLOW_COPY_AND_ASSIGN(NgxBaseFetch);
};

} // namespace net_instaweb

#endif  // NGX_BASE_FETCH_H_
