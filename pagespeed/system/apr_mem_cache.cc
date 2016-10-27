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

#include "pagespeed/system/apr_mem_cache.h"

#include <memory>

#include "apr_pools.h"  // NOLINT

#include "base/logging.h"
#include "pagespeed/system/apr_thread_compatible_pool.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/key_value_codec.h"
#include "third_party/aprutil/apr_memcache2.h"

namespace net_instaweb {

namespace {

// Defaults copied from Apache 2.4 src distribution:
// src/modules/cache/mod_socache_memcache.c
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

const int kTimeoutUnset = -1;

}  // namespace

AprMemCache::AprMemCache(const ExternalClusterSpec& cluster, int thread_limit,
                         Hasher* hasher, Statistics* statistics, Timer* timer,
                         MessageHandler* handler)
    : cluster_spec_(cluster),
      thread_limit_(thread_limit),
      timeout_us_(kTimeoutUnset),
      pool_(NULL),
      memcached_(NULL),
      hasher_(hasher),
      timer_(timer),
      timeouts_(statistics->GetVariable(kMemCacheTimeouts)),
      last_error_checkpoint_ms_(
          statistics->GetUpDownCounter(kLastErrorCheckpointMs)),
      error_burst_size_(statistics->GetUpDownCounter(kErrorBurstSize)),
      message_handler_(handler) {
  pool_ = AprCreateThreadCompatiblePool(NULL);

  // Don't try to connect on construction; we don't want to bother creating
  // connections to the memcached servers in the root process.
  //
  // TODO(jmarantz): consider doing an initial connect/disconnect during
  // config parsing to get better error reporting on Apache startup.
}

AprMemCache::~AprMemCache() {
  apr_pool_destroy(pool_);
}

void AprMemCache::InitStats(Statistics* statistics) {
  statistics->AddVariable(kMemCacheTimeouts);
  statistics->AddUpDownCounter(kLastErrorCheckpointMs);
  statistics->AddUpDownCounter(kErrorBurstSize);
}

bool AprMemCache::Connect() {
  DCHECK(servers_.empty());
  servers_.clear();
  apr_status_t status =
      apr_memcache2_create(pool_, cluster_spec_.servers.size(), 0, &memcached_);
  bool success = false;
  if ((status == APR_SUCCESS) && !cluster_spec_.empty()) {
    success = true;
    for (const ExternalServerSpec& spec : cluster_spec_.servers) {
      apr_memcache2_server_t* server = NULL;
      status = apr_memcache2_server_create(
          pool_, spec.host.c_str(), spec.port,
          kDefaultServerMin, kDefaultServerSmax,
          thread_limit_, kDefaultServerTtlUs, &server);
      if ((status != APR_SUCCESS) ||
          ((status = apr_memcache2_add_server(memcached_, server) !=
            APR_SUCCESS))) {
        char buf[kStackBufferSize];
        apr_strerror(status, buf, sizeof(buf));
        message_handler_->Message(
            kError, "Failed to attach memcached server %s:%d %s (%d)",
            spec.host.c_str(), spec.port, buf, status);
        success = false;
      } else {
        if (timeout_us_ != kTimeoutUnset) {
          apr_memcache2_set_timeout_microseconds(memcached_, timeout_us_);
        }
        servers_.push_back(server);
      }
    }
  }
  return success;
}

void AprMemCache::DecodeValueMatchingKeyAndCallCallback(
    const GoogleString& key, const char* data, size_t data_len,
    const char* calling_method, Callback* callback) {
  SharedString key_and_value;
  key_and_value.Assign(data, data_len);
  GoogleString actual_key;
  SharedString tmp_value;
  if (key_value_codec::Decode(&key_and_value, &actual_key, &tmp_value)) {
    callback->set_value(tmp_value);
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
  apr_pool_create(&data_pool, pool_);
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
  apr_pool_create(&data_pool, pool_);
  CHECK(data_pool != NULL) << "apr_pool_t data_pool allocation failure";
  apr_pool_t* temp_pool = NULL;
  apr_pool_create(&temp_pool, pool_);
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
  bool error_recorded = false;
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
          if (!error_recorded) {
            // Only count 1 error towards threshold on MultiGet failure.
            error_recorded = true;
            RecordError();
          }
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
    delete request;
  } else {
    RecordError();
    char buf[kStackBufferSize];
    apr_strerror(status, buf, sizeof(buf));
    message_handler_->Message(
        kError, "AprMemCache::MultiGet error: %s (%d) on %d keys",
        buf, status, static_cast<int>(request->size()));
    ReportMultiGetNotFound(request);
  }
  apr_pool_destroy(data_pool);
}

void AprMemCache::PutHelper(const GoogleString& key,
                            const SharedString& key_and_value) {
  // I believe apr_memcache2_set erroneously takes a char* for the value.
  // Hence we const_cast.
  GoogleString hashed_key = hasher_->Hash(key);
  apr_status_t status = apr_memcache2_set(
      memcached_, hashed_key.c_str(),
      const_cast<char*>(key_and_value.data()), key_and_value.size(),
      0, 0);
  if (status != APR_SUCCESS) {
    RecordError();
    char buf[kStackBufferSize];
    apr_strerror(status, buf, sizeof(buf));

    int value_size = key_value_codec::GetValueSizeFromKeyAndKeyValue(
        key, key_and_value);
    message_handler_->Message(
        kError, "AprMemCache::Put error: %s (%d) on key %s, value-size %d",
        buf, status, key.c_str(), value_size);
    if (status == APR_TIMEUP) {
      timeouts_->Add(1);
    }
  }
}

void AprMemCache::PutWithKeyInValue(const GoogleString& key,
                                    const SharedString& key_and_value) {
  if (!IsHealthy()) {
    return;
  }
  PutHelper(key, key_and_value);
}

void AprMemCache::Put(const GoogleString& key, const SharedString& value) {
  if (!IsHealthy()) {
    return;
  }

  SharedString key_and_value;
  if (key_value_codec::Encode(key, value, &key_and_value)) {
    PutHelper(key, key_and_value);
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
    if (status == APR_TIMEUP) {
      timeouts_->Add(1);
    }
  }
}

bool AprMemCache::GetStatus(GoogleString* buffer) {
  apr_pool_t* temp_pool = NULL;
  apr_pool_create(&temp_pool, pool_);
  CHECK(temp_pool != NULL) << "apr_pool_t allocation failure";
  bool ret = true;
  for (int i = 0, n = servers_.size(); i < n; ++i) {
    apr_memcache2_stats_t* stats;
    apr_status_t status = apr_memcache2_stats(servers_[i], temp_pool, &stats);
    if (status == APR_SUCCESS) {
      StrAppend(buffer, "memcached server ",
                cluster_spec_.servers[i].ToString(), " version ",
                stats->version);
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

void AprMemCache::set_timeout_us(int timeout_us) {
  timeout_us_ = timeout_us;
  if ((memcached_ != NULL) && (timeout_us != kTimeoutUnset)) {
    apr_memcache2_set_timeout_microseconds(memcached_, timeout_us);
  }
}

}  // namespace net_instaweb
