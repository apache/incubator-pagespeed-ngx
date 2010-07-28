/**
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/util.pb.h"

namespace net_instaweb {


bool HTTPCache::IsCurrentlyValid(const MetaData& headers) {
  if (force_caching_) {
    return true;
  }
  return headers.CacheExpirationTimeMs() > timer_->NowMs();
}

bool HTTPCache::Get(const char* key, HTTPValue* value,
                    MessageHandler* handler) {
  SharedString cache_buffer;
  SimpleMetaData headers;
  return (cache_->Get(key, &cache_buffer, handler) &&
          value->Link(&cache_buffer, handler) &&
          value->ExtractHeaders(&headers, handler) &&
          IsCurrentlyValid(headers));
}

void HTTPCache::Put(const char* key, HTTPValue* value,
                    MessageHandler* handler) {
  cache_->Put(key, value->share(), handler);
}

void HTTPCache::Put(const char* key, const MetaData& headers,
                    const StringPiece& content,
                    MessageHandler* handler) {
  if (!IsCurrentlyValid(headers)) {
    return;
  }

  HTTPValue value;
  value.SetHeaders(headers);
  value.Write(content, handler);
  Put(key, &value, handler);
}

CacheInterface::KeyState HTTPCache::Query(const char* key,
                                          MessageHandler* handler) {
  return cache_->Query(key, handler);
}

}  // namespace net_instaweb
