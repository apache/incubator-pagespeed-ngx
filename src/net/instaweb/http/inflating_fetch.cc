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
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/stack_buffer.h"

namespace net_instaweb {

InflatingFetch::InflatingFetch(AsyncFetch* fetch)
  : SharedAsyncFetch(fetch),
    request_checked_for_accept_encoding_(false),
    compression_desired_(false),
    inflate_failure_(false) {
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
      handler->Message(kWarning, "inflation failure SetInput returning false");
      inflate_failure_ = true;
    }
  }
  return status && !inflate_failure_;
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
}

void InflatingFetch::Reset() {
  if (inflater_.get() != NULL) {
    inflater_->ShutDown();
    inflater_.reset(NULL);
    inflate_failure_ = false;
  }
  SharedAsyncFetch::Reset();
}

}  // namespace net_instaweb
