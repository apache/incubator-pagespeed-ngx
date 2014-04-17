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

extern "C" {
#include <nginx.h>
}

#include "ngx_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "base/logging.h"

#include <algorithm>
#include <string>
#include <vector>

#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
  NgxFetch::NgxFetch(const GoogleString& url,
                     AsyncFetch* async_fetch,
                     MessageHandler* message_handler,
                     ngx_msec_t timeout_ms,
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
      ngx_close_connection(connection_);
    }
    if (pool_ != NULL) {
      ngx_destroy_pool(pool_);
    }
  }

  // This function is called by NgxUrlAsyncFetcher::StartFetch.
  bool NgxFetch::Start(NgxUrlAsyncFetcher* fetcher) {
    fetcher_ = fetcher;
    return Init();
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
      message_handler_->Message(kError, "NgxFetch: ParseUrl() failed");
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
    timeout_event_->handler = NgxFetchTimeout;
    timeout_event_->log = log_;

    ngx_add_timer(timeout_event_, fetcher_->fetch_timeout_);
    r_ = static_cast<ngx_http_request_t*>(ngx_pcalloc(pool_,
                                           sizeof(ngx_http_request_t)));
    if (r_ == NULL) {
      message_handler_->Message(kError,
                                "NgxFetch: ngx_pcalloc failed for timer");
      return false;
    }
    status_ = static_cast<ngx_http_status_t*>(ngx_pcalloc(pool_,
                                              sizeof(ngx_http_status_t)));
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
    if (0 != fetcher_->proxy_.url.len) {
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
      }

      resolver_ctx_->data = this;
      resolver_ctx_->name.data = tmp_url->host.data;
      resolver_ctx_->name.len = tmp_url->host.len;

#if (nginx_version < 1005008)
      resolver_ctx_->type = NGX_RESOLVE_A;
#endif

      resolver_ctx_->handler = NgxFetchResolveDone;
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
    if (connection_) {
      ngx_close_connection(connection_);
      connection_ = NULL;
    }

    async_fetch_->Done(success);

    if (fetcher_ != NULL) {
      if (fetcher_->track_original_content_length()
          && async_fetch_->response_headers()->Has(
            HttpAttributes::kXOriginalContentLength)) {
        async_fetch_->extra_response_headers()->SetOriginalContentLength(
            bytes_received_);
      }
      fetcher_->FetchComplete(this);
    }

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
  void NgxFetch::NgxFetchResolveDone(ngx_resolver_ctx_t* resolver_ctx) {
    NgxFetch* fetch = static_cast<NgxFetch*>(resolver_ctx->data);
    NgxUrlAsyncFetcher* fetcher = fetch->fetcher_;
    if (resolver_ctx->state != NGX_OK) {
      if (fetch->timeout_event() != NULL && fetch->timeout_event()->timer_set) {
        ngx_del_timer(fetch->timeout_event());
        fetch->set_timeout_event(NULL);
      }
      fetch->message_handler()->Message(
          kWarning, "NgxFetch: failed to resolve host [%.*s]",
          static_cast<int>(resolver_ctx->name.len), resolver_ctx->name.data);
      fetch->CallbackDone(false);
      return;
    }
    ngx_memzero(&fetch->sin_, sizeof(fetch->sin_));

#if (nginx_version < 1005008)
    fetch->sin_.sin_addr.s_addr = resolver_ctx->addrs[0];
#else

    struct sockaddr_in *sin;
    sin = reinterpret_cast<struct sockaddr_in *>(
        resolver_ctx->addrs[0].sockaddr);
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

    fetch->message_handler()->Message(
        kInfo, "NgxFetch: Resolved host [%.*s] to [%s]",
        static_cast<int>(resolver_ctx->name.len), resolver_ctx->name.data,
        ip_address);

    fetch->release_resolver();

    if (fetch->InitRequest() != NGX_OK) {
      fetch->message_handler()->Message(kError, "NgxFetch: InitRequest failed");
      fetch->CallbackDone(false);
    }
  }

  // prepare the send data for this fetch, and hook write event.
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
           + request_headers->Value(i).length()
           + 4;  // for ": \r\n"
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

    response_handler = NgxFetchHandleStatusLine;
    int rc = Connect();
    if (rc == NGX_AGAIN) {
      return NGX_OK;
    } else if (rc < NGX_OK) {
      return rc;
    }

    NgxFetchWrite(connection_->write);
    return NGX_OK;
  }

  int NgxFetch::Connect() {
    ngx_peer_connection_t pc;
    ngx_memzero(&pc, sizeof(pc));
    pc.sockaddr = (struct sockaddr *) &sin_;
    pc.socklen = sizeof(struct sockaddr_in);
    pc.name = &url_.host;

    // get callback is dummy function, it just returns NGX_OK
    pc.get = ngx_event_get_peer;
    pc.log_error = NGX_ERROR_ERR;
    pc.log = fetcher_->log_;
    pc.rcvbuf = -1;

    int rc = ngx_event_connect_peer(&pc);
    if (rc == NGX_ERROR || rc == NGX_DECLINED || rc == NGX_BUSY) {
      return rc;
    }

    connection_ = pc.connection;
    connection_->write->handler = NgxFetchWrite;
    connection_->read->handler = NgxFetchRead;
    connection_->data = this;

    // Timer set in Init() is still in effect.
    return rc;
  }

  // When the fetch sends the request completely, it will hook the read event,
  // and prepare to parse the response.
  void NgxFetch::NgxFetchWrite(ngx_event_t* wev) {
    ngx_connection_t* c = static_cast<ngx_connection_t*>(wev->data);
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
    ngx_buf_t* out = fetch->out_;

    while (out->pos < out->last) {
      int n = c->send(c, out->pos, out->last - out->pos);
      if (n >= 0) {
        out->pos += n;
      } else if (n == NGX_AGAIN) {
        if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
          fetch->CallbackDone(false);
        }
        // Timer set in Init() is still in effect.
        return;
      } else {
        c->error = 1;
        fetch->CallbackDone(false);
        return;
      }
    }

    if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
      c->error = 1;
      fetch->CallbackDone(false);
    }

    return;
  }

  void NgxFetch::NgxFetchRead(ngx_event_t* rev) {
    ngx_connection_t* c = static_cast<ngx_connection_t*>(rev->data);
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);

    for (;;) {
      int n = c->recv(
          c, fetch->in_->start, fetch->in_->end - fetch->in_->start);

      if (n == NGX_AGAIN) {
        break;
      }

      if (n == 0) {
        // If the content length was not known, we assume that we have read
        // all if we at least parsed the headers.
        // If we do know the content length, having a mismatch on the bytes read
        // will be interpreted as an error.
        if (fetch->content_length_known_) {
          fetch->CallbackDone(fetch->content_length_ == fetch->bytes_received_);
        } else {
          fetch->CallbackDone(fetch->parser_.headers_complete());
        }

        return;
      } else if (n > 0) {
        fetch->in_->pos = fetch->in_->start;
        fetch->in_->last = fetch->in_->start + n;
        if (!fetch->response_handler(c)) {
          fetch->CallbackDone(false);
          return;
        }

        if (fetch->done_) {
          fetch->CallbackDone(true);
          return;
        }
      }

      if (!rev->ready) {
        break;
      }
    }

    if (fetch->done_) {
      fetch->CallbackDone(true);
      return;
    }

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
      fetch->CallbackDone(false);
    }

    // Timer set in Init() is still in effect.
  }

  // Parse the status line: "HTTP/1.1 200 OK\r\n"
  bool NgxFetch::NgxFetchHandleStatusLine(ngx_connection_t* c) {
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
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
    fetch->set_response_handler(NgxFetchHandleHeader);
    return fetch->response_handler(c);
  }

  // Parse the HTTP headers
  bool NgxFetch::NgxFetchHandleHeader(ngx_connection_t* c) {
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
    char* data = reinterpret_cast<char*>(fetch->in_->pos);
    size_t size = fetch->in_->last - fetch->in_->pos;
    size_t n = fetch->parser_.ParseChunk(StringPiece(data, size),
        fetch->message_handler_);
    if (n > size) {
      return false;
    } else if (fetch->parser_.headers_complete()) {
      if (fetch->async_fetch_->response_headers()->FindContentLength(
              &fetch->content_length_)) {
        if (fetch->content_length_ < 0) {
          fetch->message_handler_->Message(
              kError, "Negative content-length in response header");
          return false;
        } else {
          fetch->content_length_known_ = true;
        }
      }

      if (fetch->fetcher_->track_original_content_length()
         && fetch->content_length_known_) {
        fetch->async_fetch_->response_headers()->SetOriginalContentLength(
            fetch->content_length_);
      }

      fetch->in_->pos += n;
      fetch->set_response_handler(NgxFetchHandleBody);
      return fetch->response_handler(c);
    }
    return true;
  }

  // Read the response body
  bool NgxFetch::NgxFetchHandleBody(ngx_connection_t* c) {
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
    char* data = reinterpret_cast<char*>(fetch->in_->pos);
    size_t size = fetch->in_->last - fetch->in_->pos;
    if (size == 0) {
      return true;
    }

    fetch->bytes_received_add(size);

    if (fetch->async_fetch_->Write(StringPiece(data, size),
        fetch->message_handler())) {
      if (fetch->content_length_known_ &&
          fetch->bytes_received_ == fetch->content_length_) {
        fetch->done_ = true;
      }
      return true;
    }
    return false;
  }

  void NgxFetch::NgxFetchTimeout(ngx_event_t* tev) {
    NgxFetch* fetch = static_cast<NgxFetch*>(tev->data);
    fetch->CallbackDone(false);
  }

  void NgxFetch::FixUserAgent() {
    GoogleString user_agent;
    ConstStringStarVector v;
    RequestHeaders* request_headers = async_fetch_->request_headers();
    if (request_headers->Lookup(HttpAttributes::kUserAgent, &v)) {
      for (int i = 0, n = v.size(); i < n; i++) {
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
        " ", kModPagespeedSubrequestUserAgent,
        "/" MOD_PAGESPEED_VERSION_STRING "-" LASTCHANGE_STRING);
    if (!StringPiece(user_agent).ends_with(version)) {
      user_agent += version;
    }
    request_headers->Add(HttpAttributes::kUserAgent, user_agent);
  }
}  // namespace net_instaweb
