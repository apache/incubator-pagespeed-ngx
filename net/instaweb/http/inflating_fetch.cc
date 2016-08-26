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

#include "net/instaweb/http/public/inflating_fetch.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

InflatingFetch::InflatingFetch(AsyncFetch* fetch)
    : SharedAsyncFetch(fetch) {
  Reset();
}

InflatingFetch::~InflatingFetch() {
  Reset();
}

bool InflatingFetch::IsCompressionAllowedInRequest() {
  if (!request_checked_for_accept_encoding_) {
    request_checked_for_accept_encoding_ = true;
    ConstStringStarVector v;
    if (request_headers()->Lookup(HttpAttributes::kAcceptEncoding, &v)) {
      for (int i = 0, n = v.size(); i < n; ++i) {
        if (v[i] != NULL) {
          StringPiece value = *v[i];
          if (StringCaseEqual(value, HttpAttributes::kGzip) ||
              StringCaseEqual(value, HttpAttributes::kDeflate)) {
            // TODO(jmarantz): what if we want only deflate, but get gzip?
            // What if we want only gzip, but get deflate?  I think this will
            // rarely happen in practice but we could handle it here.
            compression_desired_ = true;
            break;
          }
        }
      }
    }
  }
  return compression_desired_;
}

void InflatingFetch::EnableGzipFromBackend() {
  if (!IsCompressionAllowedInRequest()) {
    request_headers()->Add(HttpAttributes::kAcceptEncoding,
                           HttpAttributes::kGzip);
  }
}

bool InflatingFetch::HandleWrite(const StringPiece& sp,
                                 MessageHandler* handler) {
  if (inflate_failure_) {
    return false;
  }
  if (inflater_.get() == NULL) {
    return SharedAsyncFetch::HandleWrite(sp, handler);
  }

  DCHECK(!inflater_->HasUnconsumedInput());
  bool status = false;
  if (!inflater_->error()) {
    status = inflater_->SetInput(sp.data(), sp.size());
    if (status && !inflater_->error()) {
      char buf[kStackBufferSize];
      while (inflater_->HasUnconsumedInput()) {
        int size = inflater_->InflateBytes(buf, sizeof buf);
        if (inflater_->error() || (size < 0)) {
          handler->Message(kWarning, "inflation failure, size=%d", size);
          inflate_failure_ = true;
          break;
        } else {
          status = SharedAsyncFetch::HandleWrite(
              StringPiece(buf, size), handler);
        }
      }
    } else {
      handler->MessageS(kWarning, "inflation failure SetInput returning false");
      inflate_failure_ = true;
    }
  }
  return status && !inflate_failure_;
}

// Inflate a HTTPValue, if it was gzip compressed.
bool InflatingFetch::UnGzipValueIfCompressed(const HTTPValue& src,
                                             ResponseHeaders* headers,
                                             HTTPValue* dest,
                                             MessageHandler* handler) {
  if (!src.Empty() && headers->IsGzipped()) {
    GoogleString inflated;
    StringWriter inflate_writer(&inflated);
    StringPiece content;
    src.ExtractContents(&content);
    if (GzipInflater::Inflate(content, GzipInflater::kGzip, &inflate_writer)) {
      if (!headers->HasValue(HttpAttributes::HttpAttributes::kVary,
                             HttpAttributes::kAcceptEncoding)) {
        headers->Add(HttpAttributes::HttpAttributes::kVary,
                     HttpAttributes::kAcceptEncoding);
      }
      headers->RemoveAll(HttpAttributes::kTransferEncoding);
      headers->Remove(HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
      headers->SetContentLength(inflated.length());
      content.set(inflated.c_str(), inflated.length());
      dest->Clear();
      dest->Write(content, handler);
      dest->SetHeaders(headers);
      return true;
    }
  }
  return false;
}

bool InflatingFetch::GzipValue(int compression_level,
                               const HTTPValue& http_value,
                               HTTPValue* compressed_value,
                               ResponseHeaders* headers,
                               MessageHandler* handler) {
  StringPiece content;
  GoogleString deflated;
  int64 content_length;
  http_value.ExtractContents(&content);
  StringWriter deflate_writer(&deflated);
  if (!headers->IsGzipped() &&
      GzipInflater::Deflate(content, GzipInflater::kGzip, compression_level,
                            &deflate_writer)) {
    if (!headers->HasValue(HttpAttributes::HttpAttributes::kVary,
                           HttpAttributes::kAcceptEncoding)) {
      headers->Add(HttpAttributes::HttpAttributes::kVary,
                   HttpAttributes::kAcceptEncoding);
    }
    if (!headers->FindContentLength(&content_length)) {
      content_length = content.size();
    }
    headers->RemoveAll(HttpAttributes::kTransferEncoding);
    headers->SetOriginalContentLength(content_length);
    headers->Add(HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
    headers->SetContentLength(deflated.length());
    compressed_value->SetHeaders(headers);
    compressed_value->Write(deflated, NULL);
    return true;
  }
  return false;
}

// If we did not request gzipped/deflated content but the site gave it
// to us anyway, then interpose an inflating Writer.
//
// As of Dec 6, 2011 this URL serves gzipped content to clients that
// don't claim to accept it:
//   http://cache.boston.com/universal/js/bcom_global_scripts.js
// This is referenced from http://boston.com.
void InflatingFetch::HandleHeadersComplete() {
  ConstStringStarVector v;
  if (!IsCompressionAllowedInRequest() &&
      response_headers()->Lookup(HttpAttributes::kContentEncoding, &v)) {
    // Look for an encoding to strip.  We only look at the *last* encoding.
    // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
    for (int i = v.size() - 1; i >= 0; --i) {
      if (v[i] != NULL) {
        const StringPiece& value = *v[i];
        if (!value.empty()) {
          if (StringCaseEqual(value, HttpAttributes::kGzip)) {
            InitInflater(GzipInflater::kGzip, value);
          } else if (StringCaseEqual(value, HttpAttributes::kDeflate)) {
            InitInflater(GzipInflater::kDeflate, value);
          }
          break;  // Stop on the last non-empty value.
        }
      }
    }
  }
  SharedAsyncFetch::HandleHeadersComplete();
}

void InflatingFetch::InitInflater(GzipInflater::InflateType type,
                                  const StringPiece& value) {
  response_headers()->Remove(HttpAttributes::kContentEncoding, value);
  response_headers()->RemoveAll(HttpAttributes::kContentLength);
  response_headers()->ComputeCaching();

  // TODO(jmarantz): Consider integrating with a free-store of Inflater
  // objects to avoid re-initializing these on every request.
  inflater_.reset(new GzipInflater(type));
  if (!inflater_->Init()) {
    inflate_failure_ = true;
    inflater_.reset(NULL);
  }
}

void InflatingFetch::HandleDone(bool success) {
  SharedAsyncFetch::HandleDone(success && !inflate_failure_);
  delete this;
}

void InflatingFetch::Reset() {
  if (inflater_.get() != NULL) {
    inflater_->ShutDown();
    inflater_.reset(NULL);
  }
  request_checked_for_accept_encoding_ = false;
  compression_desired_ = false;
  inflate_failure_ = false;
  SharedAsyncFetch::Reset();
}

}  // namespace net_instaweb
