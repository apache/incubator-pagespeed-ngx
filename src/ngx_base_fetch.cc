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

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r) : request_(r) { }

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
  buffer_.append(sp.data(), sp.size());
  return true;
}

bool NgxBaseFetch::HandleFlush(MessageHandler* handler) {
  handler->Message(kInfo, "HandleFlush() -> '%s'", buffer_.c_str());
  buffer_.clear();
  return true;
}
void NgxBaseFetch::HandleDone(bool success) {
  delete this;
}

}  // namespace net_instaweb
