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
// PageSpeed needs some way to talk to the internet and request resources.  For
// example, if it's optimizing www.example.com/index.html and it sees html with
// <img src="//images.example.com/cat.jpg"> and images.example.com is authorized
// for rewriting in the config, then it needs to fetch cat.jpg from
// images.example.com and optimize it.  In apache (always) and nginx (by
// default) we use a fetcher called "serf".  This works fine, but it does run
// its own event loop.  To be more efficient, this is a "native" fetcher that
// uses nginx's event loop.
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
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"
#include "pagespeed/kernel/thread/pthread_mutex.h"


namespace net_instaweb {

typedef bool (*response_handler_pt)(ngx_connection_t* c);

class NgxUrlAsyncFetcher;
class NgxConnection;

class NgxConnection : public PoolElement<NgxConnection> {
 public:
  NgxConnection(MessageHandler* handler, int max_keepalive_requests);
  ~NgxConnection();
  void SetSock(u_char *sockaddr, socklen_t socklen) {
    socklen_ = socklen;
    ngx_memcpy(&sockaddr_, sockaddr, socklen);
  }
  // Close ensures that NgxConnection deletes itself at the appropriate time,
  // which can be after receiving a non-keepalive response, or when the remote
  // server closes the connection when the NgxConnection is pooled and idle.
  void Close();

  // Once keepalive is disabled, it can't be toggled back on.
  void set_keepalive(bool k) { keepalive_ = keepalive_ && k; }
  bool keepalive() { return keepalive_; }

  typedef Pool<NgxConnection> NgxConnectionPool;

  static NgxConnection* Connect(ngx_peer_connection_t* pc,
                                MessageHandler* handler,
                                int max_keepalive_requests);
  static void IdleWriteHandler(ngx_event_t* ev);
  static void IdleReadHandler(ngx_event_t* ev);
  // Terminate will cleanup any idle connections upon shutdown.
  static void Terminate();

  static NgxConnectionPool connection_pool;
  static PthreadMutex connection_pool_mutex;

  // c_ is owned by NgxConnection and freed in ::Close()
  ngx_connection_t* c_;
  static const int64 keepalive_timeout_ms;
  static const GoogleString ka_header;

 private:
  int max_keepalive_requests_;
  bool keepalive_;
  socklen_t socklen_;
  u_char sockaddr_[NGX_SOCKADDRLEN];
  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(NgxConnection);
};

class NgxFetch : public PoolElement<NgxFetch> {
 public:
  NgxFetch(const GoogleString& url,
           AsyncFetch* async_fetch,
           MessageHandler* message_handler,
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
  static void ResolveDoneHandler(ngx_resolver_ctx_t* ctx);
  // Write the request.
  static void ConnectionWriteHandler(ngx_event_t* wev);
  // Wait for the response.
  static void ConnectionReadHandler(ngx_event_t* rev);
  // Read and parse the first status line.
  static bool HandleStatusLine(ngx_connection_t* c);
  // Read and parse the HTTP headers.
  static bool HandleHeader(ngx_connection_t* c);
  // Read the response body.
  static bool HandleBody(ngx_connection_t* c);
  // Cancel the fetch when it's timeout.
  static void TimeoutHandler(ngx_event_t* tev);

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
  NgxConnection* connection_;
  ngx_resolver_ctx_t* resolver_ctx_;

  DISALLOW_COPY_AND_ASSIGN(NgxFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_NGX_FETCH_H_
