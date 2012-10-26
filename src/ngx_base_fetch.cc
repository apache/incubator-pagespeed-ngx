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
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r, int pipe_fd)
    : request_(r), done_called_(false), pipe_fd_(pipe_fd) {
}

NgxBaseFetch::~NgxBaseFetch() { }

void NgxBaseFetch::PopulateHeaders() {
  // http_version is the version number of protocol; 1.1 = 1001. See
  // NGX_HTTP_VERSION_* in ngx_http_request.h 
  response_headers()->set_major_version(request_->http_version / 1000);
  response_headers()->set_minor_version(request_->http_version % 1000);

  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  ngx_list_part_t* part = &request_->headers_out.headers.part;
  ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }

    StringPiece key = StringPiece(
        reinterpret_cast<char*>(header[i].key.data), header[i].key.len);
    StringPiece value = StringPiece(
        reinterpret_cast<char*>(header[i].value.data), header[i].value.len);

    response_headers()->Add(key, value);
  }

  // For some reason content_type is not included in
  // request_->headers_out.headers, which means I don't fully understand how
  // headers_out works, but manually copying over content type works.
  StringPiece content_type = StringPiece(
      reinterpret_cast<char*>(request_->headers_out.content_type.data),
      request_->headers_out.content_type.len);
  response_headers()->Add("Content-Type", content_type);
}

bool NgxBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  // TODO(jefftk): acquire lock on buffer_ here.
  buffer_.append(sp.data(), sp.size());
  // TODO(jefftk): release lock here.
  return true;
}

// TODO(jefftk): this is vulnerable to race conditions.  Memory inconsistencies
// between threads can mean that chain links get dropped, which is of course
// very bad.  To fix this we should protect buffer_ with a lock that gets
// acquired both here and in HandleWrite so that we make sure they both have a
// consistent view of memory.
//
// There may also be a race condition if this is called between the last Write()
// and Done() such that we're sending an empty buffer with last_buf set, which I
// think nginx will reject.
ngx_int_t NgxBaseFetch::CollectAccumulatedWrites(ngx_chain_t** link_ptr) {
  // TODO(jefftk): acquire lock on buffer_ here.

  if (!done_called_ && buffer_.length() == 0) {
    // Nothing to send.  But if done_called_ then we can't short circuit because
    // we need to set last_buf.
    *link_ptr = NULL;
    return NGX_OK;
  }

  // Prepare a new nginx buffer to put our buffered writes into.
  ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(request_->pool));
  if (b == NULL) {
    return NGX_ERROR;
  }
  b->start = b->pos = static_cast<u_char*>(
      ngx_pnalloc(request_->pool, buffer_.length()));
  if (b->pos == NULL) {
    return NGX_ERROR;
  }

  // Copy our writes over.
  buffer_.copy(reinterpret_cast<char*>(b->pos), buffer_.length());
  b->last = b->end = b->pos + buffer_.length();

  if (buffer_.length()) {
    b->temporary = 1;
  } else {
    b->sync = 1;
  }

  // Done with buffer contents now.
  buffer_.clear();

  // TODO(jefftk): release lock here.

  b->last_buf = b->last_in_chain = done_called_;
  
  // Prepare a chain link for our new buffer.
  *link_ptr = static_cast<ngx_chain_t*>(
      ngx_alloc_chain_link(request_->pool));
  if (*link_ptr == NULL) {
    return NGX_ERROR;
  }

  (*link_ptr)->buf = b;
  (*link_ptr)->next = NULL;
  return NGX_OK;
}

void NgxBaseFetch::RequestCollection() {
  int rc;
  char c = done_called_ ? 'F' : 'D';
  do {
    rc = write(pipe_fd_, &c, 1);
    if (rc < 0) {
      perror("NgxBaseFetch::RequestCollection");
    }
  } while (rc != 1);  // Keep trying.
}

bool NgxBaseFetch::HandleFlush(MessageHandler* handler) {
  RequestCollection();  // data available.
  return true;
}

void NgxBaseFetch::HandleDone(bool success) {
  done_called_ = true;
  RequestCollection();  // finished; can close the pipe.
  close(pipe_fd_);
  pipe_fd_ = -1;
}

}  // namespace net_instaweb
