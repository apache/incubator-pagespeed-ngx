// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#include "base/logging.h"
#include "net/instaweb/apache/apache_writer.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/string_util.h"

#include "apr_strings.h"  // for apr_pstrdup    // NOLINT
#include "httpd.h"                              // NOLINT
#include "http_protocol.h"                      // NOLINT

namespace net_instaweb {

ApacheWriter::ApacheWriter(request_rec* r)
    : request_(r),
      headers_out_(false),
      disable_downstream_header_filters_(false),
      strip_cookies_(false),
      content_length_(AsyncFetch::kContentLengthUnknown) {
}

ApacheWriter::~ApacheWriter() {
}

bool ApacheWriter::Write(const StringPiece& str, MessageHandler* handler) {
  DCHECK(headers_out_);
  ap_rwrite(str.data(), str.size(), request_);
  return true;
}

bool ApacheWriter::Flush(MessageHandler* handler) {
  DCHECK(headers_out_);
  ap_rflush(request_);
  return true;
}

void ApacheWriter::OutputHeaders(ResponseHeaders* response_headers) {
  DCHECK(!headers_out_);
  if (headers_out_) {
    return;
  }
  headers_out_ = true;

  // Apache2 defaults to set the status line as HTTP/1.1.  If the
  // original content was HTTP/1.0, we need to force the server to use
  // HTTP/1.0.  I'm not sure why/whether we need to do this; it was in
  // mod_static from the sdpy project, which is where I copied this
  // code from.
  if ((response_headers->major_version() == 1) &&
      (response_headers->minor_version() == 0)) {
    apr_table_set(request_->subprocess_env, "force-response-1.0", "1");
  }

  const char* content_type = response_headers->Lookup1(
      HttpAttributes::kContentType);

  // It doesn't matter how the origin transferred the request to us;
  // Apache will fill this data in when it issues the response.
  response_headers->RemoveAll(HttpAttributes::kTransferEncoding);
  response_headers->RemoveAll(HttpAttributes::kContentLength);
  if (content_length_ != AsyncFetch::kContentLengthUnknown) {
    ap_set_content_length(request_, content_length_);
  }

  ResponseHeadersToApacheRequest(*response_headers, request_);
  request_->status = response_headers->status_code();
  if (disable_downstream_header_filters_) {
    DisableDownstreamHeaderFilters(request_);
  }
  if (strip_cookies_ && response_headers->Sanitize()) {
    response_headers->ComputeCaching();
  }

  if (content_type != NULL) {
    // ap_set_content_type does not make a copy of the string, we need
    // to duplicate it.  Note that we will update the content type below,
    // after transforming the headers.
    ap_set_content_type(request_, apr_pstrdup(request_->pool, content_type));
  }

  // We don't set the content-length here, because we don't know it yet.
}

}  // namespace net_instaweb
