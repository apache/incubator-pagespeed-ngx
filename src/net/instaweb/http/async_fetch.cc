/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//         sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/async_fetch.h"

#include "base/logging.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"

namespace net_instaweb {

AsyncFetch::~AsyncFetch() {
  if (owns_response_headers_) {
    delete response_headers_;
  }
  if (owns_request_headers_) {
    delete request_headers_;
  }
}

bool AsyncFetch::Write(const StringPiece& sp, MessageHandler* handler) {
  bool ret = true;
  if (!sp.empty()) {  // empty-writes should be no-ops.
    if (!headers_complete_) {
      HeadersComplete();
    }
    ret = HandleWrite(sp, handler);
  }
  return ret;
}

bool AsyncFetch::Flush(MessageHandler* handler) {
  if (!headers_complete_) {
    HeadersComplete();
  }
  return HandleFlush(handler);
}

void AsyncFetch::HeadersComplete() {
  DCHECK_NE(0, response_headers()->status_code());
  if (headers_complete_) {
    LOG(DFATAL) << "AsyncFetch::HeadersComplete() called twice.";
  } else {
    headers_complete_ = true;
    HandleHeadersComplete();
  }
}

void AsyncFetch::Done(bool success) {
  if (!headers_complete_) {
    if (!success &&
        (response_headers()->status_code() == 0)) {
      // Failing fetches might not set status codes but we expect
      // successful ones to.
      response_headers()->set_status_code(HttpStatus::kNotFound);
    }
    HeadersComplete();
  }
  HandleDone(success);
}

// Sets the request-headers to the specifid pointer.  The caller must
// guarantee that the pointed-to headers remain valid as long as the
// AsyncFetch is running.
void AsyncFetch::set_request_headers(RequestHeaders* headers) {
  DCHECK(!owns_request_headers_);
  if (owns_request_headers_) {
    delete request_headers_;
  }
  request_headers_ = headers;
  owns_request_headers_ = false;
}

RequestHeaders* AsyncFetch::request_headers() {
  // TODO(jmarantz): Consider DCHECKing that only const reads occur
  // after headers_complete_.
  if (request_headers_ == NULL) {
    request_headers_ = new RequestHeaders;
    owns_request_headers_ = true;
  }
  return request_headers_;
}

const RequestHeaders* AsyncFetch::request_headers() const {
  CHECK(request_headers_ != NULL);
  return request_headers_;
}

ResponseHeaders* AsyncFetch::response_headers() {
  if (response_headers_ == NULL) {
    response_headers_ = new ResponseHeaders;
    owns_response_headers_ = true;
  }
  return response_headers_;
}

void AsyncFetch::set_response_headers(ResponseHeaders* headers) {
  DCHECK(!owns_response_headers_);
  if (owns_response_headers_) {
    delete response_headers_;
  }
  response_headers_ = headers;
  owns_response_headers_ = false;
}

StringAsyncFetch::~StringAsyncFetch() {
}

bool AsyncFetchUsingWriter::HandleWrite(const StringPiece& sp,
                                        MessageHandler* handler) {
  return writer_->Write(sp, handler);
}

bool AsyncFetchUsingWriter::HandleFlush(MessageHandler* handler) {
  return writer_->Flush(handler);
}

AsyncFetchUsingWriter::~AsyncFetchUsingWriter() {
}

SharedAsyncFetch::SharedAsyncFetch(AsyncFetch* base_fetch)
    : base_fetch_(base_fetch) {
  set_response_headers(base_fetch->response_headers());
  set_request_headers(base_fetch->request_headers());
}

SharedAsyncFetch::~SharedAsyncFetch() {
}

}  // namespace net_instaweb
