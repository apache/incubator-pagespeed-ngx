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

// Author: x.dinic@gmail.com (Junmin Xiong)
//
// The fetch is started by the main thread. It will fetch the remote resource
// from the specific url asynchronously.

#ifndef NET_INSTAWEB_NGX_FETCH_H_
#define NET_INSTAWEB_NGX_FETCH_H_

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

#include "ngx_url_async_fetcher.h"
#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"

namespace net_instaweb {

typedef bool (*response_handler_pt)(ngx_connection_t* c);

class NgxUrlAsyncFetcher;
class NgxFetch : public PoolElement<NgxFetch> {
 public:
  NgxFetch(const GoogleString& url,
           AsyncFetch* async_fetch,
           MessageHandler* message_handler,
           ngx_msec_t timeout_ms,
           ngx_log_t* log);
  ~NgxFetch();

  // Start the fetch.
  bool Start(NgxUrlAsyncFetcher* fetcher);
  // Show the completed url, for logging purposes.
  const char* str_url();
  // This fetch task is done. Call Done() on the async_fetch. It will copy the
  // buffer to cache.
  void CallbackDone(bool success);

  // Show the bytes received.
  size_t bytes_received();
  void bytes_received_add(int64 x);
  int64 fetch_start_ms();
  void set_fetch_start_ms(int64 start_ms);
  int64 fetch_end_ms();
  void set_fetch_end_ms(int64 end_ms);
  MessageHandler* message_handler();

  int get_major_version() {
    return static_cast<int>(status_->http_version / 1000);
  }
  int get_minor_version() {
    return static_cast<int>(status_->http_version % 1000);
  }
  int get_status_code() {
    return static_cast<int>(status_->code);
  }
  ngx_event_t* timeout_event() {
    return timeout_event_;
  }
  void set_timeout_event(ngx_event_t* x) {
    timeout_event_ = x;
  }
  void release_resolver() {
    if (resolver_ctx_ != NULL && resolver_ctx_ != NGX_NO_RESOLVER) {
      ngx_resolve_name_done(resolver_ctx_);
      resolver_ctx_ = NULL;
    }
  }

 private:
  response_handler_pt response_handler;
  // Do the initialized work and start the resolver work.
  bool Init();
  bool ParseUrl();
  // Prepare the request and write it to remote server.
  int InitRequest();
  // Create the connection with remote server.
  int Connect();
  void set_response_handler(response_handler_pt handler) {
    response_handler = handler;
  }
  // Only the Static functions could be used in callbacks.
  static void NgxFetchResolveDone(ngx_resolver_ctx_t* ctx);
  // Write the request.
  static void NgxFetchWrite(ngx_event_t* wev);
  // Wait for the response.
  static void NgxFetchRead(ngx_event_t* rev);
  // Read and parse the first status line.
  static bool NgxFetchHandleStatusLine(ngx_connection_t* c);
  // Read and parse the HTTP headers.
  static bool NgxFetchHandleHeader(ngx_connection_t* c);
  // Read the response body.
  static bool NgxFetchHandleBody(ngx_connection_t* c);
  // Cancel the fetch when it's timeout.
  static void NgxFetchTimeout(ngx_event_t* tev);

  // Add the pagespeed User-Agent.
  void FixUserAgent();
  void FixHost();

  const GoogleString str_url_;
  ngx_url_t url_;
  NgxUrlAsyncFetcher* fetcher_;
  AsyncFetch* async_fetch_;
  ResponseHeadersParser parser_;
  MessageHandler* message_handler_;
  int64 bytes_received_;
  int64 fetch_start_ms_;
  int64 fetch_end_ms_;
  int64 timeout_ms_;
  bool done_;
  int64 content_length_;
  bool content_length_known_;

  struct sockaddr_in sin_;
  ngx_log_t* log_;
  ngx_buf_t* out_;
  ngx_buf_t* in_;
  ngx_pool_t* pool_;
  ngx_http_request_t* r_;
  ngx_http_status_t* status_;
  ngx_event_t* timeout_event_;
  ngx_connection_t* connection_;
  ngx_resolver_ctx_t* resolver_ctx_;

  DISALLOW_COPY_AND_ASSIGN(NgxFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_NGX_FETCH_H_
