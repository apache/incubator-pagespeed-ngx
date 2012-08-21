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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/apache/apr_mem_cache.h"

#include "apr_pools.h"

#include "net/instaweb/apache/apr_mem_cache_servers.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/stack_buffer.h"

namespace {

// We can't store arbitrary keys in memcached, so encode the actual
// key in the value.  Thus in the unlikely event of a hash collision,
// we can reject the mismatched full key when reading.
//
// We encode the length as the first two bytes.  Keys of length >=
// 65535 bytes are passed to the fallback cache.  We write size 65535
// (0xffff) for keys whose values are found in the fallback cache.  In
// that case all that is stored in memcached is this 2-byte sentinel.
//
// After the encoded size, we have the actual key and value data.  We
// could also do this with protobufs, and if the encoding were any
// more complex we should change.  However I believe even with zero
// copy streams, protobufs will force us to copy the final value at
// least one extra time, and that value can be large.
//
// Our largest key-size limit is 65534.  We store data >1Mb in the
// fallback cache, as memcached cannot handle large items.  In Apache
// we'll use the file-cache as a fallback.
const int kKeyLengthEncodingBytes = 2;  // maximum 2^16 = 64k byte keys.
const size_t kKeyMaxLength = (1 << (kKeyLengthEncodingBytes * CHAR_BIT)) - 2;
const size_t kFallbackCacheSentinel = kKeyMaxLength + 1;  // 65535

}  // namespace

namespace net_instaweb {

class Timer;
class ThreadSystem;

AprMemCache::AprMemCache(AprMemCacheServers* servers,
                         CacheInterface* fallback_cache,
                         MessageHandler* handler)
    : servers_(servers),
      fallback_cache_(fallback_cache),
      message_handler_(handler) {
  CHECK(fallback_cache);
  CHECK(servers_->valid_server_spec());
}

AprMemCache::~AprMemCache() {
}

void AprMemCache::DecodeValueMatchingKeyAndCallCallback(
    const GoogleString& key, const char* data, size_t data_len,
    Callback *callback) {
  bool decoding_error = true;

  if (data_len >= 2) {
    size_t byte0 = static_cast<uint8>(data[0]);
    size_t byte1 = static_cast<uint8>(data[1]);
    size_t key_size = byte0 + (byte1 << CHAR_BIT);
    if (key_size == kFallbackCacheSentinel) {
      if (data_len == 2) {
        // Note that after a fallback miss, we will leave the
        // forwarding pointer in memcached.  This way, multiple Apache
        // servers can share memcached servers.  If we were to remove
        // the key from memcached on a fallback miss, that would
        // effectively evict the key from Apache servers that have the
        // fallback item.
        fallback_cache_->Get(key, callback);
        decoding_error = false;
      }
    } else {
      size_t overhead = key_size + kKeyLengthEncodingBytes;
      if (overhead <= data_len) {
        decoding_error = false;
        StringPiece encoded_key(data + kKeyLengthEncodingBytes, key_size);
        if (encoded_key == key) {
          GoogleString* value = callback->value()->get();
          value->assign(data + overhead, data_len - overhead);
          ValidateAndReportResult(key, CacheInterface::kAvailable, callback);
        } else {
          // TODO(jmarantz): bump hash-key collision stats?
          ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
        }
      }
    }
  }
  if (decoding_error) {
    message_handler_->Message(
        kError, "AprMemCache::Get decoding error on key %s",
        key.c_str());
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
}

void AprMemCache::Get(const GoogleString& key, Callback* callback) {
  apr_pool_t* temp_pool = NULL;
  apr_pool_create(&temp_pool, NULL);
  CHECK(temp_pool != NULL) << "apr_pool_t allocation failure";
  StringPiece result;
  if (servers_->Get(key, temp_pool, &result)) {
    DecodeValueMatchingKeyAndCallCallback(key, result.data(), result.size(),
                                          callback);
  } else {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
  apr_pool_destroy(temp_pool);
}

void AprMemCache::MultiGet(MultiGetRequest* request) {
  apr_pool_t* data_pool = NULL;
  apr_pool_create(&data_pool, NULL);
  CHECK(data_pool != NULL) << "apr_pool_t data_pool allocation failure";
  AprMemCacheServers::ResultVector results;
  if (servers_->MultiGet(request, data_pool, &results)) {
    for (int i = 0, n = results.size(); i < n; ++i) {
      KeyCallback& key_callback = (*request)[i];
      const AprMemCacheServers::Result& result = results[i];
      if (result.first == CacheInterface::kAvailable) {
        const StringPiece& value = result.second;
        DecodeValueMatchingKeyAndCallCallback(key_callback.key,
                                              value.data(), value.size(),
                                              key_callback.callback);
      } else {
        ValidateAndReportResult(key_callback.key, CacheInterface::kNotFound,
                                key_callback.callback);
      }
    }
  } else {
    for (int i = 0, n = request->size(); i < n; ++i) {
      KeyCallback& key_callback = (*request)[i];
      ValidateAndReportResult(key_callback.key,
                              CacheInterface::kNotFound,
                              key_callback.callback);
    }
  }
  delete request;
  apr_pool_destroy(data_pool);
}

void AprMemCache::Put(const GoogleString& key, SharedString* value) {
  GoogleString* str = value->get();
  size_t encoded_size = str->size() + key.size() + kKeyLengthEncodingBytes;
  size_t encoded_key_size = key.size();
  GoogleString encoded_value;
  if ((encoded_key_size > kKeyMaxLength) ||
      (encoded_size >= kValueSizeThreshold)) {
    encoded_key_size = kFallbackCacheSentinel;
    encoded_value.reserve(2);
    fallback_cache_->Put(key, value);
  } else {
    encoded_value.reserve(encoded_size);
  }

  // value format for values stored in
  //    memcached:   [2 byte size of key < 65535][key][value]
  //    fallback:    [2 bytes sentinel 65536]
  // We always store the key in the value in memcached to detect hash
  // collisions, so to do minimize the overhead we always use 2 bytes
  // for that, thus limiting our key-sizes to less than 65535.
  encoded_value.append(1, static_cast<char>(encoded_key_size & 0xff));
  encoded_value.append(1, static_cast<char>((encoded_key_size >> CHAR_BIT)
                                            & 0xff));
  if (encoded_key_size != kFallbackCacheSentinel) {
    encoded_value.append(key.data(), key.size());
    encoded_value.append(str->data(), str->size());
  }
  servers_->Set(key, encoded_value);
}

void AprMemCache::Delete(const GoogleString& key) {
  servers_->Delete(key);
}

bool AprMemCache::GetStatus(GoogleString* buffer) {
  return servers_->GetStatus(buffer);
}

}  // namespace net_instaweb
