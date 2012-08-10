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

// This code is experimental -- it needs tuning & a lot more testing.  In
// particular, we need to have some way to batch up the requests and do
// a multiget.

#include "net/instaweb/apache/apr_mem_cache.h"

#include "apr_memcache.h"
#include "apr_pools.h"

#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/stack_buffer.h"

namespace {

// Defaults copied from Apache 2.4 src distribution:
// src/modules/cache/mod_socache_memcache.c
const int kDefaultMemcachedPort = 11211;
const int kDefaultServerMin = 0;
const int kDefaultServerSmax = 1;
const int kDefaultServerTtl = 600;

// Experimentally it seems large values larger than 1M bytes result in
// a failure, e.g. from load-tests:
//     [Fri Jul 20 10:29:34 2012] [error] [mod_pagespeed 0.10.0.0-1699 @1522]
//     AprMemCache::Put error: Internal error on key
//     http://example.com/image.jpg, value-size 1393146
// So it's probably faster not to send such large requests to the server in
// the first place.

const size_t kValueSizeThreshold = 1 * 1000 * 1000;

// We can't store arbitrary keys in memcached, so encode the actual
// key in the value.  Thus in the unlikely event of a hash collision,
// we can reject the mismatched full key when reading.
//
// We encode the length as the first two bytes, ignoring attempts to Put
// any key larger than 64k.  Following that we have the actual key and
// value data.  We could also do this with protobufs, and if the encoding
// were any more complex we should change.  However I believe even with
// zero copy streams, protobufs will force us to copy the final value
// at least one extra time, and that value can be large.
const int kKeyLengthEncodingBytes = 2;  // maximum 2^16 = 64k byte keys.
const size_t kKeyMaxLength = 1 << (kKeyLengthEncodingBytes * CHAR_BIT);

}  // namespace

namespace net_instaweb {

class Timer;
class ThreadSystem;

AprMemCache::AprMemCache(const StringPiece& servers, int thread_limit,
                         Hasher* hasher, MessageHandler* handler)
    : valid_server_spec_(false),
      thread_limit_(thread_limit),
      memcached_(NULL),
      hasher_(hasher),
      message_handler_(handler) {
  apr_pool_create(&pool_, NULL);

  // Don't try to connect on construction; we don't want to bother creating
  // connections to the memcached servers in the root process.  But do parse
  // the server spec so we can determine its validity.
  //
  // TODO(jmarantz): consider doing an initial connect/disconnect during
  // config parsing to get better error reporting on Apache startup.
  StringPieceVector server_vector;
  SplitStringPieceToVector(servers, ",", &server_vector, true);
  bool success = true;
  for (int i = 0, n = server_vector.size(); i < n; ++i) {
    StringPieceVector host_port;
    int port = kDefaultMemcachedPort;
    SplitStringPieceToVector(server_vector[i], ":", &host_port, true);
    bool ok = false;
    if (host_port.size() == 1) {
      ok = true;
    } else if (host_port.size() == 2) {
      ok = StringToInt(host_port[1].as_string(), &port);
    }
    if (ok) {
      host_port[0].CopyToString(StringVectorAdd(&hosts_));
      ports_.push_back(port);
    } else {
      message_handler_->Message(kError, "Invalid memcached sever: %s",
                                server_vector[i].as_string().c_str());
      success = false;
    }
  }
  valid_server_spec_ = success && !server_vector.empty();
}

AprMemCache::~AprMemCache() {
  apr_pool_destroy(pool_);
}

bool AprMemCache::Connect() {
  apr_status_t status =
      apr_memcache_create(pool_, hosts_.size(), 0, &memcached_);
  bool success = false;
  if ((status == APR_SUCCESS) && !hosts_.empty()) {
    success = true;
    CHECK_EQ(hosts_.size(), ports_.size());
    for (int i = 0, n = hosts_.size(); i < n; ++i) {
      apr_memcache_server_t* server = NULL;
      status = apr_memcache_server_create(
          pool_, hosts_[i].c_str(), ports_[i],
          kDefaultServerMin, kDefaultServerSmax,
          thread_limit_, kDefaultServerTtl, &server);
      if ((status != APR_SUCCESS) ||
          ((status = apr_memcache_add_server(memcached_, server) !=
            APR_SUCCESS))) {
        char buf[kStackBufferSize];
        apr_strerror(status, buf, sizeof(buf));
        message_handler_->Message(
            kError, "Failed to attach memcached server %s:%d %s",
            hosts_[i].c_str(), ports_[i], buf);
        success = false;
      } else {
        servers_.push_back(server);
      }
    }
  }
  return success;
}

void AprMemCache::Get(const GoogleString& key, Callback* callback) {
  GoogleString hashed_key = hasher_->Hash(key);
  apr_pool_t* temp_pool;
  apr_pool_create(&temp_pool, NULL);
  char* data;
  apr_size_t data_len;
  apr_status_t status = apr_memcache_getp(
      memcached_, temp_pool, hashed_key.c_str(), &data, &data_len, NULL);
  if (status == APR_SUCCESS) {
    bool decoding_error = true;

    if (data_len >= 3) {
      size_t byte0 = static_cast<uint8>(data[0]);
      size_t byte1 = static_cast<uint8>(data[1]);
      size_t key_size = byte0 + (byte1 << CHAR_BIT);
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
    if (decoding_error) {
      message_handler_->Message(
          kError, "AprMemCache::Get decoding error on key %s",
          key.c_str());
      ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    }
  } else {
    if (status != APR_NOTFOUND) {
      char buf[kStackBufferSize];
      apr_strerror(status, buf, sizeof(buf));
      message_handler_->Message(
          kError, "AprMemCache::Get error: %s (%d) on key %s",
          buf, status, key.c_str());
    }
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
  apr_pool_destroy(temp_pool);
}

void AprMemCache::Put(const GoogleString& key, SharedString* value) {
  GoogleString* str = value->get();
  size_t encoded_size = str->size() + key.size() + kKeyLengthEncodingBytes;
  if ((encoded_size < kValueSizeThreshold) && (key.size() < kKeyMaxLength)) {
    GoogleString encoded_value;
    encoded_value.reserve(encoded_size);
    encoded_value.append(1, static_cast<char>(key.size() & 0xff));
    encoded_value.append(1, static_cast<char>((key.size() >> CHAR_BIT) & 0xff));
    encoded_value.append(key.data(), key.size());
    encoded_value.append(str->data(), str->size());
    GoogleString hashed_key = hasher_->Hash(key);

    // I believe apr_memcache_set erroneously takes a char* for the value.
    // Hence we take the address of character [0] rather than using the data()
    // pointer which is const.
    apr_status_t status = apr_memcache_set(
        memcached_, hashed_key.c_str(),
        &(encoded_value[0]), encoded_value.size(),
        0, 0);
    if (status != APR_SUCCESS) {
      char buf[kStackBufferSize];
      apr_strerror(status, buf, sizeof(buf));
      message_handler_->Message(
          kError, "AprMemCache::Put error: %s on key %s, value-size %d",
          buf, key.c_str(), static_cast<int>(str->size()));
    }
  }
}

void AprMemCache::Delete(const GoogleString& key) {
  GoogleString hashed_key = hasher_->Hash(key);
  apr_status_t status = apr_memcache_delete(memcached_, hashed_key.c_str(), 0);
  if (status != APR_SUCCESS) {
    char buf[kStackBufferSize];
    apr_strerror(status, buf, sizeof(buf));
    message_handler_->Message(
        kError, "AprMemCache::Delete error: %s on key %s", buf, key.c_str());
  }
}

bool AprMemCache::GetStatus(GoogleString* buffer) {
  apr_pool_t* temp_pool;
  apr_pool_create(&temp_pool, NULL);
  bool ret = true;
  for (int i = 0, n = servers_.size(); i < n; ++i) {
    apr_memcache_stats_t* stats;
    apr_status_t status = apr_memcache_stats(servers_[i], temp_pool, &stats);
    if (status == APR_SUCCESS) {
      StrAppend(buffer, "memcached server ", hosts_[i], ":",
                IntegerToString(ports_[i]), " version ", stats->version);
      StrAppend(buffer, " pid ", IntegerToString(stats->pid), " up ",
                IntegerToString(stats->uptime), " seconds \n");
      StrAppend(buffer, "bytes:                 ",
                Integer64ToString(stats->bytes), "\n");
      StrAppend(buffer, "bytes_read:            ",
                Integer64ToString(stats->bytes_read), "\n");
      StrAppend(buffer, "bytes_written:         ",
                Integer64ToString(stats->bytes_written), "\n");
      StrAppend(buffer, "cmd_get:               ",
                IntegerToString(stats->cmd_get), "\n");
      StrAppend(buffer, "cmd_set:               ",
                IntegerToString(stats->cmd_set), "\n");
      StrAppend(buffer, "connection_structures: ",
                IntegerToString(stats->connection_structures), "\n");
      StrAppend(buffer, "curr_connections:      ",
                IntegerToString(stats->curr_connections), "\n");
      StrAppend(buffer, "curr_items:            ",
                IntegerToString(stats->curr_items), "\n");
      StrAppend(buffer, "evictions:             ",
                Integer64ToString(stats->evictions), "\n");
      StrAppend(buffer, "get_hits:              ",
                IntegerToString(stats->get_hits), "\n");
      StrAppend(buffer, "get_misses:            ",
                IntegerToString(stats->get_misses), "\n");
      StrAppend(buffer, "limit_maxbytes:        ",
                IntegerToString(stats->limit_maxbytes), "\n");
      StrAppend(buffer, "pointer_size:          ",
                IntegerToString(stats->pointer_size), "\n");
      StrAppend(buffer, "rusage_system:         ",
                Integer64ToString(stats->rusage_system), "\n");
      StrAppend(buffer, "rusage_user:           ",
                Integer64ToString(stats->pointer_size), "\n");
      StrAppend(buffer, "threads:               ",
                IntegerToString(stats->threads), "\n");
      StrAppend(buffer, "total_connections:     ",
                IntegerToString(stats->total_connections), "\n");
      StrAppend(buffer, "total_items:           ",
                IntegerToString(stats->total_items), "\n");
      StrAppend(buffer, "\n");
      // TODO(jmarantz): add the rest of the stats from http://apr.apache.org
      // /docs/apr-util/1.4/structapr__memcache__stats__t.html
    } else {
      ret = false;
    }
  }
  apr_pool_destroy(temp_pool);
  return ret;
}

}  // namespace net_instaweb
