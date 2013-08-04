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

#include "ngx_base_fetch.h"
#include "ngx_list_iterator.h"

#include "ngx_pagespeed.h"

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

ngx_connection_t *NgxBaseFetch::pipe_conn_ = NULL;
int NgxBaseFetch::pipefds_[2] = {-1, -1};

// static member functions
ngx_int_t NgxBaseFetch::Initialize(ngx_log_t *log, MessageHandler *handler) {
  if (pipe_conn_) {
    return NGX_OK;
  }
  if (::pipe(pipefds_) != 0) {
    handler->Message(net_instaweb::kError, "NgxBaseFetch pipe() failed");
    return NGX_ERROR;
  }

  pipe_conn_ = ngx_get_connection(pipefds_[0], log);
  if (pipe_conn_ == NULL) {
    if (close(pipefds_[0]) == -1) {
      ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                "close() NgxBaseFetch read fd failed");
    }
    if (close(pipefds_[1]) == -1) {
      ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                "close() NgxBaseFetch write fd failed");
    }
    return NGX_ERROR;
  }


  pipe_conn_->pool = NULL;
  ngx_event_t *rev = pipe_conn_->read;
  ngx_event_t *wev = pipe_conn_->write;
  rev->log =log;
  wev->log =log;
#if (NGX_THREADS)
  rev->lock = &pipe_conn_->lock;
  wev->lock = &pipe_conn_->lock;
  rev->own_lock = &pipe_conn_->lock;
  wev->own_lock = &pipe_conn_->lock;
#endif
  rev->handler = NgxBaseFetch::ngx_event_handler;

  if (ngx_nonblocking(pipefds_[0]) == -1) {
        handler->Message(net_instaweb::kError,
        "NgxBaseFetch read fd" ngx_nonblocking_n " failed");
    goto failed;
  }

  // only EPOLL event has both add_event and add_connection
  // same as ngx_add_channel_event
  if (ngx_add_conn && (ngx_event_flags & NGX_USE_EPOLL_EVENT) == 0) {
    if (ngx_add_conn(pipe_conn_) == NGX_ERROR) {
      goto failed;
    }
  } else {
    if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {
      goto failed;
    }
  }

  return NGX_OK;
 failed:
  Terminate(log);
  return NGX_ERROR;
}

void NgxBaseFetch::Terminate(ngx_log_t *log) {
  if (pipe_conn_ == NULL) {
    return;
  }

  // Free connection and close read peer
  if (ngx_event_flags & NGX_USE_EPOLL_EVENT) {
    ngx_del_conn(pipe_conn_, 0);
  }

  ngx_close_connection(pipe_conn_);

  // close write peer.
  if (close(pipefds_[1]) == -1) {
    ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
         "close() NgxBaseFetch write fd failed");
  }
  pipe_conn_ = NULL;
}

// handle NgxBaseFetch event(Flush/HeadersComplete/Done),
// and ignore the event corresponding to SKIP.
// SKIP should only be specified in ps_release_request_context,
// so that released NgxBaseFetch event will be ignored.
//
// modified from ngx_channel_handler
void NgxBaseFetch::ProcessSignalExcept(NgxBaseFetch *except) {
  for ( ;; ) {
    NgxBaseFetch *buf[512];
    ssize_t size = ::read(pipe_conn_->fd,
         static_cast<void *>(buf), sizeof(buf[0]));

    if (size == -1) {
      if (errno == EINTR) {
        continue;
      } else if (errno == EAGAIN) {
        return;
      } else {
        // shoud not happen
        assert(0);
      }
    }

    assert(size % sizeof(buf[0]) == 0);

    ngx_uint_t i;
    for (i = 0; i < size / sizeof(buf[0]); i++) {
      NgxBaseFetch *fetch = buf[i];
      if (fetch == except) {
        continue;
      }

      ngx_http_request_t *r = fetch->request_;
      ngx_http_finalize_request(r,
             ngx_psol::ps_base_fetch_handler(r));
    }
  }
}

void NgxBaseFetch::ngx_event_handler(ngx_event_t *ev) {
  if (ev->timedout) {
    ev->timedout = 0;
    return;
  }
  ngx_log_debug0(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                 "NgxBaseFetch::event_handler");
  ProcessSignalExcept(NULL);
}

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r,
                           NgxServerContext* server_context,
                           const RequestContextPtr& request_ctx,
                           bool modify_caching_headers)
    : AsyncFetch(request_ctx),
      request_(r),
      server_context_(server_context),
      done_called_(false),
      last_buf_sent_(false),
      references_(2),
      mutex_(server_context->thread_system()->NewMutex()),
      pending_(false),
      handle_error_(true),
      modify_caching_headers_(modify_caching_headers) {
}

NgxBaseFetch::~NgxBaseFetch() {
}


void NgxBaseFetch::PopulateRequestHeaders() {
  ngx_psol::copy_request_headers_from_ngx(request_, request_headers());
}

void NgxBaseFetch::PopulateResponseHeaders() {
  ngx_psol::copy_response_headers_from_ngx(request_, response_headers());
}

// should only be called in nginx thread
ngx_int_t NgxBaseFetch::CopyBufferToNginx(ngx_chain_t** link_ptr) {
  CHECK(!(done_called_ && last_buf_sent_))
        << "CopyBufferToNginx() was called after the last buffer was sent";

  // there is no buffer to send
  if (!done_called_ && buffer_.empty()) {
    *link_ptr = NULL;
    return NGX_AGAIN;
  }

  int rc = ngx_psol::string_piece_to_buffer_chain(
      request_->pool, buffer_, link_ptr, done_called_ /* send_last_buf */);
  if (rc != NGX_OK) {
    return rc;
  }

  // Done with buffer contents now.
  buffer_.clear();

  if (done_called_) {
    last_buf_sent_ = true;
    return NGX_OK;
  }

  return NGX_AGAIN;
}

// There may also be a race condition if this is called between the last Write()
// and Done() such that we're sending an empty buffer with last_buf set, which I
// think nginx will reject.
ngx_int_t NgxBaseFetch::CollectAccumulatedWrites(ngx_chain_t** link_ptr) {
  ScopedMutex scoped_mutex(mutex_.get());
  if (pending_) {
    pending_ = false;
    return CopyBufferToNginx(link_ptr);
  }
  *link_ptr = NULL;
  return NGX_AGAIN;
}

ngx_int_t NgxBaseFetch::CollectHeaders(ngx_http_headers_out_t* headers_out) {
  const ResponseHeaders* pagespeed_headers = response_headers();

  if (content_length_known()) {
    if (headers_out->content_length) {
      headers_out->content_length->hash = 0;
      headers_out->content_length = NULL;
    }
    headers_out->content_length_n = content_length();
  }

  return ngx_psol::copy_response_headers_to_ngx(request_, *pagespeed_headers,
                                                modify_caching_headers_);
}

// following function should only be called in pagespeed thread.
void NgxBaseFetch::SignalNoLock() {
  if (pending_) {
    return;
  }

  pending_ = true;
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, request_->connection->log, 0,
                 "NgxBaseFetch::SignalNoLock");

  while (true) {
    NgxBaseFetch *fetch = this;
    ssize_t size = ::write(pipefds_[1],
                      static_cast<void *>(&fetch), sizeof(fetch));
    if (size == -1 && errno == EINTR) {
      continue;
    }
    assert(size == sizeof(fetch));
    return;
  }
}

void NgxBaseFetch::RequestCollection() {
  ScopedMutex scoped_mutex(mutex_.get());
  SignalNoLock();
}

bool NgxBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  ScopedMutex scoped_mutex(mutex_.get());
  buffer_.append(sp.data(), sp.size());
  // TODO(chaizhenhua): send signal?
  return true;
}

void NgxBaseFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  bool status_ok = (status_code != 0) && (status_code < 400);

  if (status_ok || handle_error_) {
    // If this is a 404 response we need to count it in the stats.
    if (response_headers()->status_code() == HttpStatus::kNotFound) {
      server_context_->rewrite_stats()->resource_404_count()->Add(1);
    }
  }

  RequestCollection();  // Headers available.
}

bool NgxBaseFetch::HandleFlush(MessageHandler* handler) {
  RequestCollection();  // A new part of the response body is available.
  return true;
}

void NgxBaseFetch::Release() {
  ScopedMutex scoped_mutex(mutex_.get());
  // disable signal
  pending_ = true;
  scoped_mutex.Release();

  // Process pending signals except this one.
  ProcessSignalExcept(this);
  if (references_.BarrierIncrement(-1) == 0) {
    delete this;
  }
}

void NgxBaseFetch::HandleDone(bool success) {
  // TODO(jefftk): it's possible that instead of locking here we can just modify
  // CopyBufferToNginx to only read done_called_ once.
  ScopedMutex scoped_mutex(mutex_.get());
  if (done_called_ == true) {
    return;
  }
  done_called_ = true;
  SignalNoLock();
  scoped_mutex.Release();
  if (references_.BarrierIncrement(-1) == 0) {
    delete this;
  }
}

}  // namespace net_instaweb
