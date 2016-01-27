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
//  - The fetch is started by the main thread.
//  - Resolver event was hooked when a NgxFetch starting. It could
//    lookup the IP of the domain asynchronously from the DNS server.
//  - When NgxFetchResolveDone is called, It will create the request and the
//    connection. Add the write and read event to the epoll structure.
//  - The read handler parses the response. Add the response to the buffer at
//    last.

// TODO(oschaaf): Currently the first applicable connection is picked from the
// pool when re-using connections. Perhaps it would be worth it to pick the one
// that was active the longest time ago to keep a larger pool available.
// TODO(oschaaf): style: reindent namespace according to google C++ style guide
// TODO(oschaaf): Retry mechanism for failures on a re-used k-a connection.
// Currently we don't think it's going to be an issue, see the comments at
// https://github.com/pagespeed/ngx_pagespeed/pull/781.

extern "C" {
#include <nginx.h>
}

#include "ngx_fetch.h"

#include "base/logging.h"

#include <algorithm>
#include <string>
#include <typeinfo>
#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/public/global_constants.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/pool_element.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

namespace net_instaweb {

NgxConnection::NgxConnectionPool NgxConnection::connection_pool;
PthreadMutex NgxConnection::connection_pool_mutex;
// Default keepalive 60s.
const int64 NgxConnection::keepalive_timeout_ms = 60000;
const GoogleString NgxConnection::ka_header =
    StrCat("keep-alive ",
           Integer64ToString(NgxConnection::keepalive_timeout_ms));

NgxConnection::NgxConnection(MessageHandler* handler,
                             int max_keepalive_requests) {
  c_ = NULL;
  max_keepalive_requests_ = max_keepalive_requests;
  handler_ = handler;
  // max_keepalive_requests specifies the number of http requests that are
  // allowed to be performed over a single connection. So, a
  // max_keepalive_requests of 1 effectively disables keepalive.
  keepalive_ = max_keepalive_requests_ > 1;
}

NgxConnection::~NgxConnection() {
  CHECK(c_ == NULL) << "NgxConnection: Underlying connection should be NULL";
}

void NgxConnection::Terminate() {
  for (NgxConnectionPool::iterator p = connection_pool.begin();
       p != connection_pool.end(); ++p) {
    NgxConnection* nc = *p;
    ngx_close_connection(nc->c_);
    nc->c_ = NULL;
    delete nc;
  }
  connection_pool.Clear();
}

NgxConnection* NgxConnection::Connect(ngx_peer_connection_t* pc,
                                      MessageHandler* handler,
                                      int max_keepalive_requests) {
  NgxConnection* nc;
  {
    ScopedMutex lock(&NgxConnection::connection_pool_mutex);

    for (NgxConnectionPool::iterator p = connection_pool.begin();
         p != connection_pool.end(); ++p) {
      nc = *p;

      if (ngx_memn2cmp(static_cast<u_char*>(nc->sockaddr_),
                       reinterpret_cast<u_char*>(pc->sockaddr),
                       nc->socklen_, pc->socklen) == 0) {
        CHECK(nc->c_->idle) << "Pool should only contain idle connections!";

        nc->c_->idle = 0;
        nc->c_->log = pc->log;
        nc->c_->read->log = pc->log;
        nc->c_->write->log = pc->log;
        if (nc->c_->pool != NULL) {
          nc->c_->pool->log = pc->log;
        }

        if (nc->c_->read->timer_set) {
          ngx_del_timer(nc->c_->read);
        }
        connection_pool.Remove(nc);

        ngx_log_error(NGX_LOG_DEBUG, pc->log, 0,
                      "NgxFetch: re-using connection %p (pool size: %l)",
                      nc, connection_pool.size());
        return nc;
      }
    }
  }

  int rc = ngx_event_connect_peer(pc);
  if (rc == NGX_ERROR || rc == NGX_DECLINED || rc == NGX_BUSY) {
    return NULL;
  }

  // NgxConnection deletes itself if NgxConnection::Close()
  nc = new NgxConnection(handler, max_keepalive_requests);
  nc->SetSock(reinterpret_cast<u_char*>(pc->sockaddr), pc->socklen);
  nc->c_ = pc->connection;
  return nc;
}

void NgxConnection::Close() {
  bool removed_from_pool = false;

  {
    ScopedMutex lock(&NgxConnection::connection_pool_mutex);
    for (NgxConnectionPool::iterator p = connection_pool.begin();
         p != connection_pool.end(); ++p) {
      if (*p == this) {
        // When we get here, that means that the connection either has timed
        // out or has been closed remotely.
        connection_pool.Remove(this);
        ngx_log_error(NGX_LOG_DEBUG, c_->log, 0,
                      "NgxFetch: removed connection %p (pool size: %l)",
                      this, connection_pool.size());
        removed_from_pool = true;
        break;
      }
    }
  }

  max_keepalive_requests_--;

  if (c_->read->timer_set) {
    ngx_del_timer(c_->read);
  }

  if (c_->write->timer_set) {
    ngx_del_timer(c_->write);
  }

  if (!keepalive_ || max_keepalive_requests_ <= 0 || removed_from_pool) {
    ngx_close_connection(c_);
    c_ = NULL;
    delete this;
    return;
  }

  ngx_add_timer(c_->read, static_cast<ngx_msec_t>(
      NgxConnection::keepalive_timeout_ms));

  c_->data = this;
  c_->read->handler = NgxConnection::IdleReadHandler;
  c_->write->handler = NgxConnection::IdleWriteHandler;
  c_->idle = 1;

  // This connection should not be associated with current fetch.
  c_->log = ngx_cycle->log;
  c_->read->log = ngx_cycle->log;
  c_->write->log = ngx_cycle->log;
  if (c_->pool != NULL) {
    c_->pool->log = ngx_cycle->log;
  }

  // Allow this connection to be re-used, by adding it to the connection pool.
  {
    ScopedMutex lock(&NgxConnection::connection_pool_mutex);
    connection_pool.Add(this);
    ngx_log_error(NGX_LOG_DEBUG, c_->log, 0,
                  "NgxFetch: Added connection %p (pool size: %l - "
                  " max_keepalive_requests_ %d)",
                  this, connection_pool.size(), max_keepalive_requests_);
  }
}

void NgxConnection::IdleWriteHandler(ngx_event_t* ev) {
  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  u_char buf[1];
  int n = c->recv(c, buf, 1);
  if (c->write->timedout) {
    DCHECK(false) << "NgxFetch: write timeout not expected." << n;
  }
  if (n == NGX_AGAIN) {
    return;
  }
}

void NgxConnection::IdleReadHandler(ngx_event_t* ev) {
  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  NgxConnection* nc = static_cast<NgxConnection*>(c->data);

  if (c->read->timedout) {
    nc->set_keepalive(false);
    nc->Close();
    return;
  }

  char buf[1];
  int n;

  // not a timeout event, we should check connection
  n = recv(c->fd, buf, 1, MSG_PEEK);
  if (n == -1 && ngx_socket_errno == NGX_EAGAIN) {
    if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
      nc->set_keepalive(false);
      nc->Close();
      return;
    }

    return;
  }

  nc->set_keepalive(false);
  nc->Close();
}

NgxFetch::NgxFetch(const GoogleString& url,
                   AsyncFetch* async_fetch,
                   MessageHandler* message_handler,
                   ngx_log_t* log)
    : str_url_(url),
      fetcher_(NULL),
      async_fetch_(async_fetch),
      parser_(async_fetch->response_headers()),
      message_handler_(message_handler),
      bytes_received_(0),
      fetch_start_ms_(0),
      fetch_end_ms_(0),
      done_(false),
      content_length_(-1),
      content_length_known_(false),
      resolver_ctx_(NULL) {
  ngx_memzero(&url_, sizeof(url_));
  log_ = log;
  pool_ = NULL;
  timeout_event_ = NULL;
  connection_ = NULL;
}

NgxFetch::~NgxFetch() {
  if (timeout_event_ != NULL && timeout_event_->timer_set) {
    ngx_del_timer(timeout_event_);
  }
  if (connection_ != NULL) {
    connection_->Close();
    connection_ = NULL;
  }
  if (pool_ != NULL) {
    ngx_destroy_pool(pool_);
    pool_ = NULL;
  }
}

// This function is called by NgxUrlAsyncFetcher::StartFetch.
bool NgxFetch::Start(NgxUrlAsyncFetcher* fetcher) {
  fetcher_ = fetcher;
  bool ok = Init();
  if (ok) {
    ngx_log_error(NGX_LOG_DEBUG, log_, 0, "NgxFetch %p: initialized",
                  this);
  }  // else Init() will have emitted a reason
  return ok;
}

// Create the pool, parse the url, add the timeout event and
// hook the DNS resolver if needed. Else we connect directly.
// When this returns false, our caller (NgxUrlAsyncFetcher::StartFetch)
// will call fetch->CallbackDone()
bool NgxFetch::Init() {
  pool_ = ngx_create_pool(12288, log_);
  if (pool_ == NULL) {
    message_handler_->Message(kError, "NgxFetch: ngx_create_pool failed");
    return false;
  }

  if (!ParseUrl()) {
    message_handler_->Message(kError, 
                              "NgxFetch: ParseUrl() failed for [%s]:%s",
                              str_url_.c_str(), url_.err);
    return false;
  }

  timeout_event_ = static_cast<ngx_event_t*>(
      ngx_pcalloc(pool_, sizeof(ngx_event_t)));
  if (timeout_event_ == NULL) {
    message_handler_->Message(kError,
                              "NgxFetch: ngx_pcalloc failed for timeout");
    return false;
  }

  timeout_event_->data = this;
  timeout_event_->handler = NgxFetch::TimeoutHandler;
  timeout_event_->log = log_;

  ngx_add_timer(timeout_event_, fetcher_->fetch_timeout_);
  r_ = static_cast<ngx_http_request_t*>(
      ngx_pcalloc(pool_, sizeof(ngx_http_request_t)));

  if (r_ == NULL) {
    message_handler_->Message(kError,
                              "NgxFetch: ngx_pcalloc failed for timer");
    return false;
  }
  status_ = static_cast<ngx_http_status_t*>(
      ngx_pcalloc(pool_, sizeof(ngx_http_status_t)));

  if (status_ == NULL) {
    message_handler_->Message(kError,
                              "NgxFetch: ngx_pcalloc failed for status");
    return false;
  }

  // The host is either a domain name or an IP address.  First check
  // if it's a valid IP address and only if that fails fall back to
  // using the DNS resolver.

  // Maybe we have a Proxy.
  ngx_url_t* tmp_url = &url_;
  if (fetcher_->proxy_.url.len != 0) {
    tmp_url = &fetcher_->proxy_;
  }

  GoogleString s_ipaddress(reinterpret_cast<char*>(tmp_url->host.data),
                           tmp_url->host.len);
  ngx_memzero(&sin_, sizeof(sin_));
  sin_.sin_family = AF_INET;
  sin_.sin_port = htons(tmp_url->port);
  sin_.sin_addr.s_addr = inet_addr(s_ipaddress.c_str());

  if (sin_.sin_addr.s_addr == INADDR_NONE) {
    // inet_addr returned INADDR_NONE, which means the hostname
    // isn't a valid IP address.  Check DNS.
    ngx_resolver_ctx_t temp;
    temp.name.data = tmp_url->host.data;
    temp.name.len = tmp_url->host.len;
    resolver_ctx_ = ngx_resolve_start(fetcher_->resolver_, &temp);
    if (resolver_ctx_ == NULL || resolver_ctx_ == NGX_NO_RESOLVER) {
      // TODO(oschaaf): this spams the log, but is useful in the fetcher's
      // current state
      message_handler_->Message(
          kError, "NgxFetch: Couldn't start resolving, "
          "is there a proper resolver configured in nginx.conf?");
      return false;
    } else {
      ngx_log_error(NGX_LOG_DEBUG, log_, 0,
                    "NgxFetch %p: start resolve for: %s",
                    this, s_ipaddress.c_str());
    }

    resolver_ctx_->data = this;
    resolver_ctx_->name.data = tmp_url->host.data;
    resolver_ctx_->name.len = tmp_url->host.len;

#if (nginx_version < 1005008)
    resolver_ctx_->type = NGX_RESOLVE_A;
#endif

    resolver_ctx_->handler = NgxFetch::ResolveDoneHandler;
    resolver_ctx_->timeout = fetcher_->resolver_timeout_;

    if (ngx_resolve_name(resolver_ctx_) != NGX_OK) {
      message_handler_->Message(kWarning,
                                "NgxFetch: ngx_resolve_name failed");
      return false;
    }
  } else {
    if (InitRequest() != NGX_OK) {
      message_handler()->Message(kError, "NgxFetch: InitRequest failed");
      return false;
    }
  }
  return true;
}

const char* NgxFetch::str_url() {
  return str_url_.c_str();
}

// This function should be called only once. The only argument is sucess or
// not.
void NgxFetch::CallbackDone(bool success) {
  ngx_log_error(NGX_LOG_DEBUG, log_, 0, "NgxFetch %p: CallbackDone: %s",
                this, success ? "OK":"FAIL");

  if (async_fetch_ == NULL) {
    LOG(FATAL)
        << "BUG: NgxFetch callback called more than once on same fetch"
        << str_url_.c_str() << "(" << this << ").Please report this at"
        << "https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss";
    return;
  }

  release_resolver();

  if (timeout_event_ && timeout_event_->timer_set) {
    ngx_del_timer(timeout_event_);
    timeout_event_ = NULL;
  }

  if (connection_ != NULL) {
    // Connection will be re-used only on responses that specify
    // 'Connection: keep-alive' in their headers.
    bool keepalive = false;

    if (success) {
      ConstStringStarVector v;
      if (async_fetch_->response_headers()->Lookup(
              StringPiece(HttpAttributes::kConnection), &v)) {
        for (size_t i = 0; i < v.size(); i++) {
          if (*v[i] == "keep-alive") {
            keepalive = true;
            break;
          } else if (*v[i] == "close") {
            break;
          }
        }
      }
      ngx_log_error(NGX_LOG_DEBUG, log_, 0,
                    "NgxFetch %p: connection %p attempt keep-alive: %s",
                    this, connection_, keepalive ? "Yes":"No");
    }

    connection_->set_keepalive(keepalive);
    connection_->Close();
    connection_ = NULL;
  }

  if (fetcher_ != NULL) {
    if (fetcher_->track_original_content_length()
        && async_fetch_->response_headers()->Has(
            HttpAttributes::kXOriginalContentLength)) {
      async_fetch_->extra_response_headers()->SetOriginalContentLength(
          bytes_received_);
    }
    fetcher_->FetchComplete(this);
  }
  async_fetch_->Done(success);
  async_fetch_ = NULL;
}

size_t NgxFetch::bytes_received() {
  return bytes_received_;
}
void NgxFetch::bytes_received_add(int64 x) {
  bytes_received_ += x;
}

int64 NgxFetch::fetch_start_ms() {
  return fetch_start_ms_;
}

void NgxFetch::set_fetch_start_ms(int64 start_ms) {
  fetch_start_ms_ = start_ms;
}

int64 NgxFetch::fetch_end_ms() {
  return fetch_end_ms_;
}

void NgxFetch::set_fetch_end_ms(int64 end_ms) {
  fetch_end_ms_ = end_ms;
}

MessageHandler* NgxFetch::message_handler() {
  return message_handler_;
}

bool NgxFetch::ParseUrl() {
  url_.url.len = str_url_.length();
  url_.url.data = static_cast<u_char*>(ngx_palloc(pool_, url_.url.len));
  if (url_.url.data == NULL) {
    return false;
  }
  str_url_.copy(reinterpret_cast<char*>(url_.url.data), str_url_.length(), 0);

  return NgxUrlAsyncFetcher::ParseUrl(&url_, pool_);
}

// Issue a request after the resolver is done
void NgxFetch::ResolveDoneHandler(ngx_resolver_ctx_t* resolver_ctx) {
  NgxFetch* fetch = static_cast<NgxFetch*>(resolver_ctx->data);
  NgxUrlAsyncFetcher* fetcher = fetch->fetcher_;

  if (resolver_ctx->state != NGX_OK) {
    if (fetch->timeout_event() != NULL && fetch->timeout_event()->timer_set) {
      ngx_del_timer(fetch->timeout_event());
      fetch->set_timeout_event(NULL);
    }
    fetch->message_handler()->Message(
        kWarning, "NgxFetch %p: failed to resolve host [%.*s]", fetch,
        static_cast<int>(resolver_ctx->name.len), resolver_ctx->name.data);
    fetch->CallbackDone(false);
    return;
  }

  ngx_uint_t i;
  // Find the first ipv4 address. We don't support ipv6 yet.
  for (i = 0; i < resolver_ctx->naddrs; i++) {
    // Old versions of nginx and tengine have a different definition of addrs,
    // work around to make sure we are using the right type (ngx_addr_t*).
    ngx_addr_t* ngx_addrs = reinterpret_cast<ngx_addr_t*>(resolver_ctx->addrs);
    if (typeid(*ngx_addrs) == typeid(*resolver_ctx->addrs)) {
      if (reinterpret_cast<struct sockaddr_in*>(ngx_addrs[i].sockaddr)
              ->sin_family == AF_INET) {
        break;
      }
    } else {
      // We're using an old version that uses in_addr_t* for addrs.
      break;
    }
  }

  // If no suitable ipv4 address was found, we fail.
  if (i == resolver_ctx->naddrs) {
    if (fetch->timeout_event() != NULL && fetch->timeout_event()->timer_set) {
      ngx_del_timer(fetch->timeout_event());
      fetch->set_timeout_event(NULL);
    }
    fetch->message_handler()->Message(
        kWarning, "NgxFetch %p: no suitable address for host [%.*s]", fetch,
        static_cast<int>(resolver_ctx->name.len), resolver_ctx->name.data);
    fetch->CallbackDone(false);
  }

  ngx_memzero(&fetch->sin_, sizeof(fetch->sin_));

#if (nginx_version < 1005008)
  fetch->sin_.sin_addr.s_addr = resolver_ctx->addrs[i];
#else
  struct sockaddr_in* sin;

  sin = reinterpret_cast<struct sockaddr_in*>(
      resolver_ctx->addrs[i].sockaddr);

  fetch->sin_.sin_family = sin->sin_family;
  fetch->sin_.sin_addr.s_addr = sin->sin_addr.s_addr;
#endif

  fetch->sin_.sin_family = AF_INET;
  fetch->sin_.sin_port = htons(fetch->url_.port);

  // Maybe we have Proxy
  if (0 != fetcher->proxy_.url.len) {
    fetch->sin_.sin_port = htons(fetcher->proxy_.port);
  }

  char* ip_address = inet_ntoa(fetch->sin_.sin_addr);

  ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                "NgxFetch %p: Resolved host [%V] to [%s]", fetch,
                &resolver_ctx->name, ip_address);

  fetch->release_resolver();

  if (fetch->InitRequest() != NGX_OK) {
    fetch->message_handler()->Message(kError, "NgxFetch: InitRequest failed");
    fetch->CallbackDone(false);
  }
}

// Prepare the request data for this fetch, and hook the write event.
int NgxFetch::InitRequest() {
  in_ = ngx_create_temp_buf(pool_, 4096);
  if (in_ == NULL) {
    return NGX_ERROR;
  }

  FixUserAgent();

  RequestHeaders* request_headers = async_fetch_->request_headers();
  ConstStringStarVector v;
  size_t size = 0;
  bool have_host = false;
  GoogleString port;

  response_handler = NgxFetch::HandleStatusLine;
  int rc = Connect();
  if (rc == NGX_AGAIN || rc == NGX_OK) {
    if (connection_->keepalive()) {
      request_headers->Add(HttpAttributes::kConnection,
                           NgxConnection::ka_header);
    }
    const char* method = request_headers->method_string();
    size_t method_len = strlen(method);

    size = (method_len +
            1 /* for the space */ +
            url_.uri.len +
            sizeof(" HTTP/1.0\r\n") - 1);

    for (int i = 0; i < request_headers->NumAttributes(); i++) {
      // if no explicit host header is given in the request headers,
      // we need to derive it from the url.
      if (StringCaseEqual(request_headers->Name(i), "Host")) {
        have_host = true;
      }

      // name: value\r\n
      size += request_headers->Name(i).length()
          + request_headers->Value(i).length() + 4;  // 4 for ": \r\n"
    }

    if (!have_host) {
      port = StrCat(":", IntegerToString(url_.port));
      // for "Host: " + host + ":" + port + "\r\n"
      size += url_.host.len + 8 + port.size();
    }

    size += 2;  // "\r\n";
    out_ = ngx_create_temp_buf(pool_, size);

    if (out_ == NULL) {
      return NGX_ERROR;
    }

    out_->last = ngx_cpymem(out_->last, method, method_len);
    out_->last = ngx_cpymem(out_->last, " ", 1);
    out_->last = ngx_cpymem(out_->last, url_.uri.data, url_.uri.len);
    out_->last = ngx_cpymem(out_->last, " HTTP/1.0\r\n", 11);

    if (!have_host) {
      out_->last = ngx_cpymem(out_->last, "Host: ", 6);
      out_->last = ngx_cpymem(out_->last, url_.host.data, url_.host.len);
      out_->last = ngx_cpymem(out_->last, port.c_str(), port.size());
      out_->last = ngx_cpymem(out_->last, "\r\n", 2);
    }

    for (int i = 0; i < request_headers->NumAttributes(); i++) {
      const GoogleString& name = request_headers->Name(i);
      const GoogleString& value = request_headers->Value(i);
      out_->last = ngx_cpymem(out_->last, name.c_str(), name.length());
      *(out_->last++) = ':';
      *(out_->last++) = ' ';
      out_->last = ngx_cpymem(out_->last, value.c_str(), value.length());
      *(out_->last++) = CR;
      *(out_->last++) = LF;
    }
    *(out_->last++) = CR;
    *(out_->last++) = LF;
    if (rc == NGX_AGAIN) {
      return NGX_OK;
    }
  } else if (rc < NGX_OK) {
    return rc;
  }
  CHECK(rc == NGX_OK);
  NgxFetch::ConnectionWriteHandler(connection_->c_->write);
  return NGX_OK;
}

int NgxFetch::Connect() {
  ngx_peer_connection_t pc;
  ngx_memzero(&pc, sizeof(pc));
  pc.sockaddr = (struct sockaddr*)&sin_;
  pc.socklen = sizeof(struct sockaddr_in);
  pc.name = &url_.host;

  // get callback is dummy function, it just returns NGX_OK
  pc.get = ngx_event_get_peer;
  pc.log_error = NGX_ERROR_ERR;
  pc.log = fetcher_->log_;
  pc.rcvbuf = -1;


  connection_ = NgxConnection::Connect(&pc, message_handler(),
                                       fetcher_->max_keepalive_requests_);
  ngx_log_error(NGX_LOG_DEBUG, fetcher_->log_, 0,
                "NgxFetch %p Connect() connection %p for [%s]",
                this, connection_, str_url());

  if (connection_ == NULL) {
    return NGX_ERROR;
  }

  connection_->c_->write->handler = NgxFetch::ConnectionWriteHandler;
  connection_->c_->read->handler = NgxFetch::ConnectionReadHandler;
  connection_->c_->data = this;

  // Timer set in Init() is still in effect.
  return NGX_OK;
}

// When the fetch sends the request completely, it will hook the read event,
// and prepare to parse the response. Timer set in Init() is still in effect.
void NgxFetch::ConnectionWriteHandler(ngx_event_t* wev) {
  ngx_connection_t* c = static_cast<ngx_connection_t*>(wev->data);
  NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
  ngx_buf_t* out = fetch->out_;
  bool ok = true;
  while (wev->ready && out->pos < out->last) {
    int n = c->send(c, out->pos, out->last - out->pos);
    ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                  "NgxFetch %p: ConnectionWriteHandler "
                  "send result %d", fetch, n);

    if (n >= 0) {
      out->pos += n;
    } else if (n == NGX_AGAIN) {
      break;
    } else {
      ok = false;
      break;
    }
  }

  if (ok) {
    if (out->pos == out->last) {
      ok = ngx_handle_read_event(c->read, 0) == NGX_OK;
    } else {
      ok = ngx_handle_write_event(c->write, 0) == NGX_OK;
    }
  }

  if (!ok) {
    fetch->message_handler()->Message(
        kWarning, "NgxFetch %p: failed to hook next event", fetch);
    c->error = 1;
    fetch->CallbackDone(false);
  }
}

// Timer set in Init() is still in effect.
void NgxFetch::ConnectionReadHandler(ngx_event_t* rev) {
  ngx_connection_t* c = static_cast<ngx_connection_t*>(rev->data);
  NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
  bool ok = true;

  while(rev->ready) {
    int n = c->recv(
        c, fetch->in_->start, fetch->in_->end - fetch->in_->start);

    ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                  "NgxFetch %p: ConnectionReadHandler "
                  "recv result %d", fetch, n);

    if (n == NGX_AGAIN) {
      break;
    } else if (n == 0) {
      // If the content length was not known, we assume that we have read
      // all if we at least parsed the headers.
      // If we do know the content length, having a mismatch on the bytes read
      // will be interpreted as an error.
      ok = (fetch->content_length_known_ && fetch->content_length_ == fetch->bytes_received_)
          || fetch->parser_.headers_complete();
      fetch->done_ = true;
      break;
    } else if (n > 0) {
      fetch->in_->pos = fetch->in_->start;
      fetch->in_->last = fetch->in_->start + n;
      ok = fetch->response_handler(c);
      if (fetch->done_ || !ok) {
        break;
      }
    }
  }

  if (!ok) {
    fetch->CallbackDone(false);
  } else if (fetch->done_) {
    fetch->CallbackDone(true);
  } else if (ngx_handle_read_event(rev, 0) != NGX_OK) {
    fetch->CallbackDone(false);
  }
}

// Parse the status line: "HTTP/1.1 200 OK\r\n"
bool NgxFetch::HandleStatusLine(ngx_connection_t* c) {
  NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
  ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                "NgxFetch %p: Handle status line", fetch);

  // This function only works after Nginx-1.1.4. Before nginx-1.1.4,
  // ngx_http_parse_status_line didn't save http_version.
  ngx_int_t n = ngx_http_parse_status_line(fetch->r_, fetch->in_,
                                           fetch->status_);
  if (n == NGX_ERROR) {  // parse status line error
    fetch->message_handler()->Message(
        kWarning, "NgxFetch: failed to parse status line");
    return false;
  } else if (n == NGX_AGAIN) {  // not completed
    return true;
  }

  ResponseHeaders* response_headers =
      fetch->async_fetch_->response_headers();
  response_headers->SetStatusAndReason(
      static_cast<HttpStatus::Code>(fetch->get_status_code()));
  response_headers->set_major_version(fetch->get_major_version());
  response_headers->set_minor_version(fetch->get_minor_version());

  fetch->in_->pos += n;
  fetch->set_response_handler(NgxFetch::HandleHeader);
  if ((fetch->in_->last - fetch->in_->pos) > 0) {
    return fetch->response_handler(c);
  }
  return true;
}

// Parse the HTTP headers
bool NgxFetch::HandleHeader(ngx_connection_t* c) {
  NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
  char* data = reinterpret_cast<char*>(fetch->in_->pos);
  size_t size = fetch->in_->last - fetch->in_->pos;
  size_t n = fetch->parser_.ParseChunk(StringPiece(data, size),
                                       fetch->message_handler_);

  ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                "NgxFetch %p: Handle headers", fetch);

  if (n > size) {
    return false;
  } else if (fetch->parser_.headers_complete()) {
    // TODO(oschaaf): We should also check if the request method was HEAD
    // - but I don't think PSOL uses that at this point.
    if (fetch->get_status_code() == 304 || fetch->get_status_code() == 204) {
      fetch->done_ = true;
    } else if (fetch->async_fetch_->response_headers()->FindContentLength(
            &fetch->content_length_)) {
      if (fetch->content_length_ < 0) {
        fetch->message_handler_->Message(
            kError, "Negative content-length in response header");
        return false;
      } else {
        fetch->content_length_known_ = true;
        if (fetch->content_length_ == 0) {
          fetch->done_ = true;
        }
      }
    }

    if (fetch->fetcher_->track_original_content_length()
        && fetch->content_length_known_) {
      fetch->async_fetch_->response_headers()->SetOriginalContentLength(
          fetch->content_length_);
    }

    fetch->in_->pos += n;
    if (!fetch->done_) {
      fetch->set_response_handler(NgxFetch::HandleBody);
      if ((fetch->in_->last - fetch->in_->pos) > 0) {
        return fetch->response_handler(c);
      }
    }
  } else {
    fetch->in_->pos += n;
  }
  return true;
}

// Read the response body
bool NgxFetch::HandleBody(ngx_connection_t* c) {
  NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
  char* data = reinterpret_cast<char*>(fetch->in_->pos);
  size_t size = fetch->in_->last - fetch->in_->pos;

  fetch->bytes_received_add(size);

  ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                "NgxFetch %p: Handle body (%d bytes)", fetch, size);

  if ( fetch->async_fetch_->Write(StringPiece(data, size),
                                  fetch->message_handler()) ) {
    if (fetch->bytes_received_ == fetch->content_length_) {
      fetch->done_ = true;
    }
    fetch->in_->pos += size;
  } else {
    ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                  "NgxFetch %p: async fetch write failure", fetch);
    return false;
  }
  return true;
}

void NgxFetch::TimeoutHandler(ngx_event_t* tev) {
  NgxFetch* fetch = static_cast<NgxFetch*>(tev->data);
  ngx_log_error(NGX_LOG_DEBUG, fetch->log_, 0,
                "NgxFetch %p: TimeoutHandler called", fetch);
  fetch->CallbackDone(false);
}

void NgxFetch::FixUserAgent() {
  GoogleString user_agent;
  ConstStringStarVector v;
  RequestHeaders* request_headers = async_fetch_->request_headers();
  if (request_headers->Lookup(HttpAttributes::kUserAgent, &v)) {
    for (size_t i = 0, n = v.size(); i < n; i++) {
      if (i != 0) {
        user_agent += " ";
      }

      if (v[i] != NULL) {
        user_agent += *(v[i]);
      }
    }
    request_headers->RemoveAll(HttpAttributes::kUserAgent);
  }
  if (user_agent.empty()) {
    user_agent += "NgxNativeFetcher";
  }
  GoogleString version = StrCat(
      " (", kModPagespeedSubrequestUserAgent,
      "/" MOD_PAGESPEED_VERSION_STRING "-" LASTCHANGE_STRING ")");
  if (!StringPiece(user_agent).ends_with(version)) {
    user_agent += version;
  }
  request_headers->Add(HttpAttributes::kUserAgent, user_agent);
}

}  // namespace net_instaweb
