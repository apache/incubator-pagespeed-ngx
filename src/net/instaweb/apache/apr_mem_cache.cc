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
#include "base/logging.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/hostname_util.h"
#include "net/instaweb/util/public/key_value_codec.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/stack_buffer.h"
#include "third_party/aprutil/apr_memcache2.h"

namespace net_instaweb {

namespace {

// Defaults copied from Apache 2.4 src distribution:
// src/modules/cache/mod_socache_memcache.c
const int kDefaultMemcachedPort = 11211;
const int kDefaultServerMin = 0;      // minimum # client sockets to open
const int kDefaultServerSmax = 1;     // soft max # client connections to open
const char kMemCacheTimeouts[] = "memcache_timeouts";
const char kLastErrorCheckpointMs[] = "memcache_last_error_checkpoint_ms";
const char kErrorBurstSize[] = "memcache_error_burst_size";

// time-to-live of a client connection.  There is a bug in the APR
// implementation, where the TTL argument to apr_memcache2_server_create was
// being interpreted in microseconds, rather than seconds.
//
// See: http://mail-archives.apache.org/mod_mbox/apr-dev/201209.mbox/browser
// and: http://svn.apache.org/viewvc?view=revision&revision=1390530
//
// TODO(jmarantz): figure out somehow if that fix is applied, and if so,
// do not multiply by 1M.
const int kDefaultServerTtlUs = 600*1000*1000;

}  // namespace

AprMemCache::AprMemCache(const StringPiece& servers, int thread_limit,
                         Hasher* hasher, Statistics* statistics,
                         Timer* timer, MessageHandler* handler)
    : valid_server_spec_(false),
      thread_limit_(thread_limit),
      pool_(NULL),
      memcached_(NULL),
      hasher_(hasher),
      timer_(timer),
      timeouts_(statistics->GetVariable(kMemCacheTimeouts)),
      last_error_checkpoint_ms_(statistics->GetVariable(
          kLastErrorCheckpointMs)),
      error_burst_size_(statistics->GetVariable(kErrorBurstSize)),
      is_machine_local_(true),
      message_handler_(handler) {
  servers.CopyToString(&server_spec_);
  apr_pool_create(&pool_, NULL);

  // Get our hostname for the is_machine_local_ analysis below.
  GoogleString hostname(GetHostname());

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
      // If any host isn't "localhost" then the machine isn't local.
      is_machine_local_ &= IsLocalhost(host_port[0], hostname);
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

void AprMemCache::InitStats(Statistics* statistics) {
  statistics->AddVariable(kMemCacheTimeouts);
  statistics->AddVariable(kLastErrorCheckpointMs);
  statistics->AddVariable(kErrorBurstSize);
}

bool AprMemCache::Connect() {
  apr_status_t status =
      apr_memcache2_create(pool_, hosts_.size(), 0, &memcached_);
  bool success = false;
  if ((status == APR_SUCCESS) && !hosts_.empty()) {
    success = true;
    CHECK_EQ(hosts_.size(), ports_.size());
    for (int i = 0, n = hosts_.size(); i < n; ++i) {
      apr_memcache2_server_t* server = NULL;
      status = apr_memcache2_server_create(
          pool_, hosts_[i].c_str(), ports_[i],
          kDefaultServerMin, kDefaultServerSmax,
          thread_limit_, kDefaultServerTtlUs, &server);
      if ((status != APR_SUCCESS) ||
          ((status = apr_memcache2_add_server(memcached_, server) !=
            APR_SUCCESS))) {
        char buf[kStackBufferSize];
        apr_strerror(status, buf, sizeof(buf));
        message_handler_->Message(
            kError, "Failed to attach memcached server %s:%d %s (%d)",
            hosts_[i].c_str(), ports_[i], buf, status);
        success = false;
      } else {
        servers_.push_back(server);
      }
    }
  }
  return success;
}

void AprMemCache::DecodeValueMatchingKeyAndCallCallback(
    const GoogleString& key, const char* data, size_t data_len,
    const char* calling_method, Callback* callback) {
  SharedString key_value;
  key_value.Assign(data, data_len);
  GoogleString actual_key;
  if (key_value_codec::Decode(&key_value, &actual_key, callback->value())) {
    if (key == actual_key) {
      ValidateAndReportResult(actual_key, CacheInterface::kAvailable, callback);
    } else {
      message_handler_->Message(
          kError, "AprMemCache::%s key collision %s != %s",
          calling_method, key.c_str(), actual_key.c_str());
      ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    }
  } else {
    message_handler_->Message(
        kError, "AprMemCache::%s decoding error on key %s",
        calling_method, key.c_str());
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
}

void AprMemCache::Get(const GoogleString& key, Callback* callback) {
  if (!IsHealthy()) {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    return;
  }
  apr_pool_t* data_pool;
  apr_pool_create(&data_pool, NULL);
  CHECK(data_pool != NULL) << "apr_pool_t data_pool allocation failure";
  GoogleString hashed_key = hasher_->Hash(key);
  char* data;
  apr_size_t data_len;
  apr_status_t status = apr_memcache2_getp(
      memcached_, data_pool, hashed_key.c_str(), &data, &data_len, NULL);
  if (status == APR_SUCCESS) {
    DecodeValueMatchingKeyAndCallCallback(key, data, data_len, "Get", callback);
  } else {
    if (status != APR_NOTFOUND) {
      RecordError();
      char buf[kStackBufferSize];
      apr_strerror(status, buf, sizeof(buf));
      message_handler_->Message(
          kError, "AprMemCache::Get error: %s (%d) on key %s",
          buf, status, key.c_str());
      if (status == APR_TIMEUP) {
        timeouts_->Add(1);
      }
    }
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
  apr_pool_destroy(data_pool);
}

void AprMemCache::MultiGet(MultiGetRequest* request) {
  if (!IsHealthy()) {
    ReportMultiGetNotFound(request);
    return;
  }

  // apr_memcache2_multgetp documentation indicates it may clear the
  // temp_pool inside the function.  Thus it is risky to pass the same
  // pool for both temp_pool and data_pool, as we need to read the
  // data after the call.
  apr_pool_t* data_pool;
  apr_pool_create(&data_pool, NULL);
  CHECK(data_pool != NULL) << "apr_pool_t data_pool allocation failure";
  apr_pool_t* temp_pool = NULL;
  apr_pool_create(&temp_pool, NULL);
  CHECK(temp_pool != NULL) << "apr_pool_t temp_pool allocation failure";
  apr_hash_t* hash_table = apr_hash_make(data_pool);
  StringVector hashed_keys;

  for (int i = 0, n = request->size(); i < n; ++i) {
    GoogleString hashed_key = hasher_->Hash((*request)[i].key);
    hashed_keys.push_back(hashed_key);
    apr_memcache2_add_multget_key(data_pool, hashed_key.c_str(), &hash_table);
  }

  apr_status_t status = apr_memcache2_multgetp(memcached_, temp_pool, data_pool,
                                               hash_table);
  apr_pool_destroy(temp_pool);
  if (status == APR_SUCCESS) {
    for (int i = 0, n = request->size(); i < n; ++i) {
      CacheInterface::KeyCallback* key_callback = &(*request)[i];
      const GoogleString& key = key_callback->key;
      Callback* callback = key_callback->callback;
      const GoogleString& hashed_key = hashed_keys[i];
      apr_memcache2_value_t* value = static_cast<apr_memcache2_value_t*>(
          apr_hash_get(hash_table, hashed_key.data(), hashed_key.size()));
      if (value == NULL) {
        status = APR_NOTFOUND;
      } else {
        status = value->status;
      }
      if (status == APR_SUCCESS) {
        DecodeValueMatchingKeyAndCallCallback(key, value->data, value->len,
                                              "MultiGet", callback);
      } else {
        if (status != APR_NOTFOUND) {
          RecordError();
          char buf[kStackBufferSize];
          apr_strerror(status, buf, sizeof(buf));
          message_handler_->Message(
              kError, "AprMemCache::MultiGet error: %s (%d) on key %s",
              buf, status, key.c_str());
          if (status == APR_TIMEUP) {
            timeouts_->Add(1);
          }
        }
        ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
      }
    }
  }
  apr_pool_destroy(data_pool);
  delete request;
}

void AprMemCache::Put(const GoogleString& key, SharedString* encoded_value) {
  if (!IsHealthy()) {
    return;
  }

  GoogleString hashed_key = hasher_->Hash(key);
  SharedString key_value;
  if (key_value_codec::Encode(key, encoded_value, &key_value)) {
    // I believe apr_memcache2_set erroneously takes a char* for the value.
    // Hence we const_cast.
    apr_status_t status = apr_memcache2_set(
        memcached_, hashed_key.c_str(),
        const_cast<char*>(key_value.data()), key_value.size(),
        0, 0);
    if (status != APR_SUCCESS) {
      RecordError();
      char buf[kStackBufferSize];
      apr_strerror(status, buf, sizeof(buf));
      message_handler_->Message(
          kError, "AprMemCache::Put error: %s (%d) on key %s, value-size %d",
          buf, status, key.c_str(), encoded_value->size());
    }
  } else {
    message_handler_->Message(
        kError, "AprMemCache::Put error: key size %d too large, first "
        "100 bytes of key is: %s",
        static_cast<int>(key.size()), key.substr(0, 100).c_str());
  }
}

void AprMemCache::Delete(const GoogleString& key) {
  if (!IsHealthy()) {
    return;
  }

  // Note that deleting a key whose value exceeds our size threshold
  // will not actually remove it from the fallback cache.  However, it
  // will remove our sentinel indicating that it's in the fallback cache,
  // and therefore it will be functionally deleted.
  //
  // TODO(jmarantz): determine whether it's better to defensively delete
  // it from the fallback cache even though most data will not be, thus
  // incurring file system overhead for small data deleted from memcached.
  //
  // Another option would be to issue a Get before the Delete to see
  // if it's in the fallback cache, but that would send more load to
  // memcached, possibly transferring significant amounts of data that
  // will be tossed.

  GoogleString hashed_key = hasher_->Hash(key);
  apr_status_t status = apr_memcache2_delete(memcached_, hashed_key.c_str(), 0);
  if ((status != APR_SUCCESS) && (status != APR_NOTFOUND)) {
    RecordError();
    char buf[kStackBufferSize];
    apr_strerror(status, buf, sizeof(buf));
    message_handler_->Message(
        kError, "AprMemCache::Delete error: %s (%d) on key %s", buf, status,
        key.c_str());
  }
}

bool AprMemCache::GetStatus(GoogleString* buffer) {
  apr_pool_t* temp_pool = NULL;
  apr_pool_create(&temp_pool, NULL);
  CHECK(temp_pool != NULL) << "apr_pool_t allocation failure";
  bool ret = true;
  for (int i = 0, n = servers_.size(); i < n; ++i) {
    apr_memcache2_stats_t* stats;
    apr_status_t status = apr_memcache2_stats(servers_[i], temp_pool, &stats);
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

void AprMemCache::RecordError() {
  // Note that we are sharing state with other Apache child processes,
  // and we use Statistics Variables to determine our current health
  // status.  In Apache those are implemented via shared memory.
  int64 time_ms = timer_->NowMs();
  int64 last_error_checkpoint_ms = last_error_checkpoint_ms_->Get();
  int64 delta_ms = time_ms - last_error_checkpoint_ms;

  // The first time we catch an error we'll set the time of the error.
  // We'll keep counting errors for 30 seconds declaring sickness when
  // we reach 4.  That's an approximation because there will be
  // cross-process races between accesses of the time & counts.
  //
  // When we get to 30 seconds since the start of the error burst we
  // clear everything & start counting again.
  if (delta_ms > kHealthCheckpointIntervalMs) {
    last_error_checkpoint_ms_->Set(time_ms);
    error_burst_size_->Set(1);
  } else {
    error_burst_size_->Add(1);
  }
}

bool AprMemCache::IsHealthy() const {
  if (shutdown_.value()) {
    return false;
  }
  int64 time_ms = timer_->NowMs();
  int64 last_error_checkpoint_ms = last_error_checkpoint_ms_->Get();
  int64 delta_ms = time_ms - last_error_checkpoint_ms;
  int64 error_burst_size = error_burst_size_->Get();

  if (delta_ms > kHealthCheckpointIntervalMs) {
    if (error_burst_size >= kMaxErrorBurst) {
      // We were sick, but now it seems enough time has expired to
      // see whether we've recovered.
      message_handler_->Message(
          kInfo, "AprMemCache::IsHealthy error: Attempting to recover");
    }
    error_burst_size_->Set(0);
    return true;
  }
  return error_burst_size < kMaxErrorBurst;
}

void AprMemCache::ShutDown() {
  shutdown_.set_value(true);
}

}  // namespace net_instaweb
