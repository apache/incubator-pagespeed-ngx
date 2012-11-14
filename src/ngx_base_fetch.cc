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
#include "ngx_pagespeed.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r, int pipe_fd)
    : request_(r), done_called_(false), last_buf_sent_(false),
      pipe_fd_(pipe_fd) {
  PopulateRequestHeaders();
}

NgxBaseFetch::~NgxBaseFetch() { }

void NgxBaseFetch::PopulateRequestHeaders() {
  CopyHeadersFromTable<RequestHeaders>(&request_->headers_in.headers,
                                       request_headers());
}

void NgxBaseFetch::PopulateResponseHeaders() {
  CopyHeadersFromTable<ResponseHeaders>(&request_->headers_out.headers,
                                        response_headers());

  // Manually copy over the content type because it's not included in
  // request_->headers_out.headers.
  response_headers()->Add(
      HttpAttributes::kContentType,
      ngx_http_pagespeed_str_to_string_piece(
          request_->headers_out.content_type));
}

template<class HeadersT>
void NgxBaseFetch::CopyHeadersFromTable(ngx_list_t* headers_from,
                                        HeadersT* headers_to) {
  // http_version is the version number of protocol; 1.1 = 1001. See
  // NGX_HTTP_VERSION_* in ngx_http_request.h 
  headers_to->set_major_version(request_->http_version / 1000);
  headers_to->set_minor_version(request_->http_version % 1000);

  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  ngx_list_part_t* part = &headers_from->part;
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

    StringPiece key = ngx_http_pagespeed_str_to_string_piece(header[i].key);
    StringPiece value = ngx_http_pagespeed_str_to_string_piece(header[i].value);

    headers_to->Add(key, value);
  }
}

bool NgxBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  Lock();
  buffer_.append(sp.data(), sp.size());
  Unlock();
  return true;
}

ngx_int_t NgxBaseFetch::CopyBufferToNginx(ngx_chain_t** link_ptr) {
  if ((last_buf_sent_ || !done_called_) && buffer_.length() == 0) {
    // Nothing to send.  But if done_called_ then we can't short circuit because
    // we need to set last_buf unless last_buf_sent_.
    return NGX_DECLINED;
  }

  // Below, *link_ptr will be NULL if we're starting the chain, and the head
  // chain link.
  *link_ptr = NULL;

  // If non-null, the current last link in the chain.
  ngx_chain_t* tail_link = NULL;

  // How far into buffer_ we're currently working on.
  ngx_uint_t offset;

  // TODO(jefftk): look up the nginx buffer size properly.
  ngx_uint_t max_buffer_size = 8192;  // 8k
  for (offset = 0 ;
       offset < buffer_.length() ||
           // If the pagespeed buffer is empty but Done() has been called we
           // need to pass through an empty buffer to nginx to communicate
           // last_buf.  Otherwise we shouldn't generate empty buffers.
           (offset == 0 && buffer_.length() == 0);
       offset += max_buffer_size) {

    // Prepare a new nginx buffer to put our buffered writes into.
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_calloc_buf(request_->pool));
    if (b == NULL) {
      return NGX_ERROR;
    }

    if (buffer_.length() == 0) {
      CHECK(offset == 0);
      b->pos = b->start = b->end = b->last = NULL;
      // The purpose of this buffer is just to pass along last_buf.
      b->sync = 1;
    } else {
      CHECK(buffer_.length() > offset);
      ngx_uint_t b_size = buffer_.length() - offset;
      if (b_size > max_buffer_size) {
        b_size = max_buffer_size;
      }

      b->start = b->pos = static_cast<u_char*>(
          ngx_palloc(request_->pool, b_size));
      if (b->pos == NULL) {
        return NGX_ERROR;
      }

      // Copy our writes over.  We're copying from buffer_[offset] up to
      // buffer_[offset + b_size] into b which has size b_size.
      buffer_.copy(reinterpret_cast<char*>(b->pos), b_size, offset);
      b->last = b->end = b->pos + b_size;

      b->temporary = 1;  // Identify this buffer as in-memory and mutable.
    }

    // Prepare a chain link.
    ngx_chain_t* cl = static_cast<ngx_chain_t*>(
        ngx_alloc_chain_link(request_->pool));
    if (cl == NULL) {
      return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    if (*link_ptr == NULL) {
      // This is the first link in the returned chain.
      *link_ptr = cl;
    } else {
      // Link us into the chain.
      CHECK(tail_link != NULL);
      tail_link->next = cl;
    }

    tail_link = cl;
  }

  // Done with buffer contents now.
  buffer_.clear();

  CHECK(tail_link != NULL);
  if (done_called_) {
    tail_link->buf->last_buf = true;
    last_buf_sent_ = true;
  }

  return NGX_OK;
}

// There may also be a race condition if this is called between the last Write()
// and Done() such that we're sending an empty buffer with last_buf set, which I
// think nginx will reject.
ngx_int_t NgxBaseFetch::CollectAccumulatedWrites(ngx_chain_t** link_ptr) {
  Lock();
  ngx_int_t rc = CopyBufferToNginx(link_ptr);
  Unlock();

  if (rc == NGX_DECLINED) {
    *link_ptr = NULL;
    return NGX_OK;
  }
  return rc;
}

ngx_int_t NgxBaseFetch::CollectHeaders(ngx_http_headers_out_t* headers_out) {
  // Copy from response_headers() into headers_out.

  Lock();
  const ResponseHeaders* pagespeed_headers = response_headers();
  Unlock();

  headers_out->status = pagespeed_headers->status_code();

  ngx_int_t i;
  for (i = 0 ; i < pagespeed_headers->NumAttributes() ; i++) {
    const GoogleString& name_gs = pagespeed_headers->Name(i);
    const GoogleString& value_gs = pagespeed_headers->Value(i);

    ngx_str_t name, value;
    name.len = name_gs.length();
    name.data = reinterpret_cast<u_char*>(const_cast<char*>(name_gs.data()));
    value.len = value_gs.length();
    value.data = reinterpret_cast<u_char*>(const_cast<char*>(value_gs.data()));

    // TODO(jefftk): If we're setting a cache control header we'd like to
    // prevent any downstream code from changing it.  Specifically, if we're
    // serving a cache-extended resource the url will change if the resource
    // does and so we've given it a long lifetime.  If the site owner has done
    // something like set all css files to a 10-minute cache lifetime, that
    // shouldn't apply to our generated resources.  See Apache code in
    // net/instaweb/apache/header_util:AddResponseHeadersToRequest

    // Make copies of name and value to put into headers_out.

    u_char* value_s = ngx_pstrdup(request_->pool, &value);
    if (value_s == NULL) {
      return NGX_ERROR;
    }

    if (STR_EQ_LITERAL(name, "Content-Type")) {
      // Unlike all the other headers, content_type is just a string.
      headers_out->content_type.data = value_s;
      headers_out->content_type.len = value.len;
      headers_out->content_type_len = value.len;
      // In ngx_http_test_content_type() nginx will allocate and calculate
      // content_type_lowcase if we leave it as null.
      headers_out->content_type_lowcase = NULL;
      continue;
    }

    u_char* name_s = ngx_pstrdup(request_->pool, &name);
    if (name_s == NULL) {
      return NGX_ERROR;
    }

    ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&headers_out->headers));
    if (header == NULL) {
      return NGX_ERROR;
    }

    header->hash = 1;  // Include this header in the output.
    header->key.len = name.len;
    header->key.data = name_s;
    header->value.len = value.len;
    header->value.data = value_s;

    // Populate the shortcuts to commonly used headers.
    if (STR_EQ_LITERAL(name, "Date")) {
      headers_out->date = header;
    } else if (STR_EQ_LITERAL(name, "Etag")) {
      headers_out->etag = header;
    } else if (STR_EQ_LITERAL(name, "Expires")) {
      headers_out->expires = header;
    } else if (STR_EQ_LITERAL(name, "Last-Modified")) {
      headers_out->last_modified = header;
    } else if (STR_EQ_LITERAL(name, "Location")) {
      headers_out->location = header;
    } else if (STR_EQ_LITERAL(name, "Server")) {
      headers_out->server = header;
    }
  }

  return NGX_OK;
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
  RequestCollection();  // Headers available.
}

bool NgxBaseFetch::HandleFlush(MessageHandler* handler) {
  RequestCollection();  // A new part of the response body is available.
  return true;
}

void NgxBaseFetch::HandleDone(bool success) {
  done_called_ = true;
  close(pipe_fd_);  // Indicates to nginx that we're done with the rewrite.
  pipe_fd_ = -1;
}

}  // namespace net_instaweb
