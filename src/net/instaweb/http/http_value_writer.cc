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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/http/public/http_value_writer.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"

namespace net_instaweb {

void HTTPValueWriter::SetHeaders(ResponseHeaders* headers) {
  if (cache_->IsCacheableContentLength(headers)) {
    value_->SetHeaders(headers);
  } else {
    has_buffered_ = false;
    value_->Clear();
  }
}

bool HTTPValueWriter::Write(const StringPiece& str, MessageHandler* handler) {
  if (has_buffered_ &&
      cache_->IsCacheableBodySize(str.size() + value_->contents_size())) {
    // IsCacheableContentLength is only able to detect if a response is
    // of cacheable size when the response has content tye header. If we
    // receive the response chunked, then we need to buffer up before
    // discovering if the response is uncacheable.
    return value_->Write(str, handler);
  }
  has_buffered_ = false;
  value_->Clear();
  return false;
}

bool HTTPValueWriter::CheckCanCacheElseClear(ResponseHeaders* headers) {
  if (!cache_->IsCacheableContentLength(headers)) {
    has_buffered_ = false;
    value_->Clear();
  }
  return has_buffered_;
}

}  // namespace net_instaweb
