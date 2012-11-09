
#include "ngx_fetch.h"
#include "net/instaweb/util/public/basictypes.h"
#include "base/logging.h"

#include <algorithm>
#include <string>
#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
  NgxFetch::NgxFetch(const GoogleString& url,
      AsyncFetch* async_fetch,
      MessageHandler* message_handler,
      int64 timeout_ms)
    : str_url_(url),
      fetcher_(NULL),
      async_fetch_(async_fetch),
      parser_(async_fetch->response_headers()),
      message_handler_(message_handler),
      bytes_received_(0),
      fetch_start_ms_(0),
      fetch_end_ms_(0) {
        ngx_memzero(&url_, sizeof(ngx_url_t));
        log_ = NULL;
        pool_ = NULL;
        timeout_event_ = NULL;
        connection_ = NULL;
  }

  NgxFetch::~NgxFetch() {
    if (connection_ != NULL) {
      ngx_close_connection(connection_);
    }
    if (pool_ != NULL) {
      ngx_destroy_pool(pool_);
    }
  }

  bool NgxFetch::Start(NgxUrlAsyncFetcher* fetcher) {
    return true;
  }

  bool NgxFetch::Init() {
    //TODO: log when return false;

    pool_ = ngx_create_pool(4096, log_);
    if (pool_ == false) { return false; }

    timeout_event_ = static_cast<ngx_event_t *>(ngx_pcalloc(pool_, sizeof(ngx_event_t)));
    if (timeout_event_ == NULL) { return false; }

    ngx_event_add_timer(timeout_event_, static_cast<ngx_msec_t> (timeout_ms_));
    ParseUrl();

    ngx_resolver_ctx_t temp;
    temp.name.data = url_.host.data;
    temp.name.len = url_.host.len;
    resolver_ctx_ = ngx_resolve_start(fetcher_->resolver_, &temp);
    if (resolver_ctx_ == NULL) { return false; }
    if (resolver_ctx_ == NGX_NO_RESOLVER) { return false; }

    resolver_ctx_->data = this;
    resolver_ctx_->name.data = url_.host.data;
    resolver_ctx_->name.len = url_.host.len;
    resolver_ctx_->type = NGX_RESOLVE_A;
    resolver_ctx_->handler = NgxFetchResolveDone;
    resolver_ctx_->data = NULL;
    resolver_ctx_->timeout = fetcher_->resolver_timeout_;

    if (resolver_ctx_ == NGX_NO_RESOLVER) { return false; }
    if (ngx_resolve_name(resolver_ctx_) != NGX_OK) { return false; }

    return true;

  }

  const char* NgxFetch::str_url() { return str_url_.c_str(); }

  void NgxFetch::Cancel() {
  }

  void NgxFetch::CallbackDone(bool success) {
  }

  size_t NgxFetch::bytes_received() { return bytes_received_; }

  int64 NgxFetch::fetch_start_ms() { return fetch_start_ms_; }

  void NgxFetch::set_fetch_start_ms(int64 start_ms) {
    fetch_start_ms_ = start_ms;
  }

  int64 NgxFetch::fetch_end_ms() { return fetch_end_ms_; }

  void NgxFetch::set_fetch_end_ms(int64 end_ms) { fetch_end_ms_ = end_ms; }

  MessageHandler* NgxFetch::message_handler() { return message_handler_; }

  bool NgxFetch::ParseUrl() {
    return true;
  }

  void NgxFetch::NgxFetchResolveDone(ngx_resolver_ctx_t* resolver_ctx) {
    NgxFetch* fetch = static_cast<NgxFetch*>(resolver_ctx->data);
    if (resolver_ctx->state != NGX_OK) {
      fetch->Cancel();
      return;
    }

    ngx_resolve_name_done(resolver_ctx);
    // resolver_ctx_ = NULL;


    if (fetch->InitRquest() != NGX_OK) {
      // TODO : LOG;
      fetch->Cancel();
    }

    return;
  }

  int NgxFetch::Connect() {
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(url_.port);
    sin.sin_addr.s_addr = resolver_ctx_->addrs[0];

    ngx_peer_connection_t pc;
    ngx_memzero(&pc, sizeof(ngx_peer_connection_t));
    pc.sockaddr = (struct sockaddr *) &sin;
    pc.socklen = sizeof(struct sockaddr_in);
    pc.name = &url_.host;

    // just return NGX_OK
    pc.get = ngx_event_get_peer;
    pc.rcvbuf = -1;
    pc.local = NULL;
    pc.log = fetcher_->log_;

    int rc = ngx_event_connect_peer(&pc);
    connection_ = pc.connection;
    connection_->write->handler = NgxFetchWrite;
    connection_->read->handler = NgxFetchRead;
    connection_->data = this;

    if (ngx_handle_write_event(connection_->write, 0) != NGX_OK) {
      return NGX_ERROR;
    }

    //TODO set write timeout when rc == NGX_AGAIN
    return rc;
  }

  int NgxFetch::InitRquest() {

    in_ = static_cast<ngx_buf_t *>(ngx_calloc_buf(pool_));
    if (in_ == NULL) { return NGX_ERROR; }
    // 4096 or size of memory page
    in_->start = static_cast<u_char*>(ngx_palloc(pool_, 4096));
    if (in_->start == NULL) { return NGX_ERROR; }
    in_->pos = in_->start;
    in_->last = in_->start + 4096;

    out_ = static_cast<ngx_buf_t *>(ngx_calloc_buf(pool_));
    if (out_ == NULL) {
      return NGX_ERROR;
    }

    FixUserAgent();
    FixHost();
    RequestHeaders* request_headers = async_fetch_->request_headers();
    ConstStringStarVector v;
    size_t size = 0;
    size = sizeof("GET ") - 1 + url_.uri.len + sizeof("HTTP/1.1\r\n") - 1;
    for (int i = 0; i < request_headers->NumAttributes(); i++) {

      // name: value\r\n
      size += request_headers->Name(i).length()
            + request_headers->Value(i).length()
            + 4; // for ": \r\n"
    }
    size += 2; // "\r\n";
    out_->start = static_cast<u_char*>(ngx_palloc(pool_, size));
    if (out_->start == NULL) {
      return NGX_ERROR;
    }
    out_->pos = out_->start;
    out_->last = out_->start;
    out_->end = out_->start + size;

    out_->last = ngx_cpymem(out_->last, (u_char *) "GET ", 4);
    out_->last = ngx_cpymem(out_->last, url_.uri.data, url_.uri.len);
    out_->last = ngx_cpymem(out_->last, (u_char *) "HTTP/1.1\r\n", 10);

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

    int rc = Connect();
    if (rc == NGX_AGAIN) {
      return NGX_OK;
    } else if (rc < NGX_OK) {
      return rc;
    }

    while (out_->pos < out_->last) {
      int n = connection_->send(connection_, out_->pos, out_->last - out_->pos);
      if (n >= 0) {
        out_->pos += n;
      } else if (n == NGX_AGAIN) {
        if (ngx_handle_write_event(connection_->write, 0) != NGX_OK) {
          return NGX_OK;
        }

        return NGX_OK;
      } else {
        // TODO: try again when send failed ?
        connection_->error = 1;
        return NGX_ERROR;
      }
    }

    return ngx_handle_read_event(connection_->read, 0);
  }

  void NgxFetch::NgxFetchWrite(ngx_event_t* wev) {
    ngx_connection_t* c = static_cast<ngx_connection_t*>(wev->data);
    NgxFetch* fetch = static_cast<NgxFetch*>(c->data);
    ngx_buf_t* out = fetch->out_;

    // TODO: set write event timeout

    while (out->pos < out->last) {
      int n = c->send(c, out->pos, out->last - out->pos);
      if (n >= 0) {
        out->pos += n;
      } else if (n == NGX_AGAIN) {
        // add send timeout, 10 only used for test
        // ngx_add_timer(c->write, 10);
        if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
          // write event can not be hook
          fetch->Cancel();
        }
        return;
      } else {
        c->error = 1;
        fetch->Cancel();
        return;
      }
    }

    if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
      c->error = 1;
      fetch->Cancel();
    }

    return;
  }

  void NgxFetch::NgxFetchRead(ngx_event_t* rev) {
  }

  void NgxFetch::NgxFetchTimeout(ngx_event_t* tev) {
  }

  void NgxFetch::FixHost() {
  }

  void NgxFetch::FixUserAgent() {
  }
} // namespace net_instaweb
