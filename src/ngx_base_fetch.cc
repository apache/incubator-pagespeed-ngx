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

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r, int pipe_fd,
                           NgxServerContext* server_context,
                           const RequestContextPtr& request_ctx,
                           PreserveCachingHeaders preserve_caching_headers)
    : AsyncFetch(request_ctx),
      request_(r),
      server_context_(server_context),
      done_called_(false),
      last_buf_sent_(false),
      pipe_fd_(pipe_fd),
      references_(2),
      handle_error_(true),
      preserve_caching_headers_(preserve_caching_headers) {
  if (pthread_mutex_init(&mutex_, NULL)) CHECK(0);
}

NgxBaseFetch::~NgxBaseFetch() {
  pthread_mutex_destroy(&mutex_);
}

void NgxBaseFetch::Lock() {
  pthread_mutex_lock(&mutex_);
}

void NgxBaseFetch::Unlock() {
  pthread_mutex_unlock(&mutex_);
}

bool NgxBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  Lock();
  buffer_.append(sp.data(), sp.size());
  Unlock();
  return true;
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

  int rc = string_piece_to_buffer_chain(
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
  ngx_int_t rc;
  Lock();
  rc = CopyBufferToNginx(link_ptr);
  Unlock();
  return rc;
}

ngx_int_t NgxBaseFetch::CollectHeaders(ngx_http_headers_out_t* headers_out) {
  const ResponseHeaders* pagespeed_headers = response_headers();

  if (content_length_known()) {
     headers_out->content_length = NULL;
     headers_out->content_length_n = content_length();
  }

  return copy_response_headers_to_ngx(request_, *pagespeed_headers,
                                      preserve_caching_headers_);
}

void NgxBaseFetch::RequestCollection() {
  int rc;
  char c = 'A';  // What byte we write is arbitrary.
  while (true) {
    rc = write(pipe_fd_, &c, 1);
    if (rc == 1) {
      break;
    } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      // TODO(jefftk): is this rare enough that spinning isn't a problem?  Could
      // we get into a case where the pipe fills up and we spin forever?

    } else {
      perror("NgxBaseFetch::RequestCollection");
      break;
    }
  }
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
  DecrefAndDeleteIfUnreferenced();
}

void NgxBaseFetch::DecrefAndDeleteIfUnreferenced() {
  // Creates a full memory barrier.
  if (__sync_add_and_fetch(&references_, -1) == 0) {
    delete this;
  }
}

void NgxBaseFetch::HandleDone(bool success) {
  // TODO(jefftk): it's possible that instead of locking here we can just modify
  // CopyBufferToNginx to only read done_called_ once.
  Lock();
  done_called_ = true;
  Unlock();

  close(pipe_fd_);  // Indicates to nginx that we're done with the rewrite.
  pipe_fd_ = -1;

  DecrefAndDeleteIfUnreferenced();
}

}  // namespace net_instaweb
