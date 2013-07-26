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

#include "pagespeed/kernel/cache/async_cache.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/atomic_bool.h"
#include "pagespeed/kernel/base/atomic_int32.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/key_value_codec.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

AsyncCache::AsyncCache(CacheInterface* cache, QueuedWorkerPool* pool)
    : cache_(cache) {
  CHECK(cache->IsBlocking());
  sequence_ = pool->NewSequence();
  sequence_->set_max_queue_size(kMaxQueueSize);
}

AsyncCache::~AsyncCache() {
  DCHECK_EQ(0, outstanding_operations());
}

GoogleString AsyncCache::FormatName(StringPiece cache) {
  return StrCat("Async(", cache, ")");
}

void AsyncCache::Get(const GoogleString& key, Callback* callback) {
  if (IsHealthy()) {
    outstanding_operations_.NoBarrierIncrement(1);
    sequence_->Add(MakeFunction(this, &AsyncCache::DoGet,
                                &AsyncCache::CancelGet,
                                new GoogleString(key), callback));
  } else {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
}

void AsyncCache::MultiGet(MultiGetRequest* request) {
  outstanding_operations_.NoBarrierIncrement(1);
  if (IsHealthy()) {
    sequence_->Add(MakeFunction(this, &AsyncCache::DoMultiGet,
                                &AsyncCache::CancelMultiGet, request));
  } else {
    CancelMultiGet(request);
  }
}

void AsyncCache::DoGet(GoogleString* key, Callback* callback) {
  if (IsHealthy()) {
    cache_->Get(*key, callback);
    delete key;
    outstanding_operations_.NoBarrierIncrement(-1);
  } else {
    CancelGet(key, callback);
  }
}

void AsyncCache::CancelGet(GoogleString* key, Callback* callback) {
  ValidateAndReportResult(*key, CacheInterface::kNotFound, callback);
  delete key;
  outstanding_operations_.NoBarrierIncrement(-1);
}

void AsyncCache::DoMultiGet(MultiGetRequest* request) {
  if (IsHealthy()) {
    cache_->MultiGet(request);
    outstanding_operations_.NoBarrierIncrement(-1);
  } else {
    CancelMultiGet(request);
  }
}

void AsyncCache::CancelMultiGet(MultiGetRequest* request) {
  ReportMultiGetNotFound(request);
  outstanding_operations_.NoBarrierIncrement(-1);
}

void AsyncCache::Put(const GoogleString& key, SharedString* value) {
  if (IsHealthy()) {
    // If the cache will encode the key into the value during Put,
    // then instead do it inline now, not in sequence_, as
    // SharedString::Append can mutate the shared value storage in a
    // thread-unsafe way.
    if (cache_->MustEncodeKeyInValueOnPut()) {
      SharedString* encoded_value = new SharedString;
      if (!key_value_codec::Encode(key, value, encoded_value)) {
        delete encoded_value;
        return;
      }
      value = encoded_value;
    } else {
      value = new SharedString(*value);
    }

    outstanding_operations_.NoBarrierIncrement(1);
    sequence_->Add(
        MakeFunction(this, &AsyncCache::DoPut, &AsyncCache::CancelPut,
                     new GoogleString(key), value));
  }
}

void AsyncCache::DoPut(GoogleString* key, SharedString* value) {
  if (IsHealthy()) {
    // TODO(jmarantz): Start timers at the beginning of each operation,
    // particularly this one, and use long delays as a !IsHealthy signal.
    if (cache_->MustEncodeKeyInValueOnPut()) {
      cache_->PutWithKeyInValue(*key, value);
    } else {
      cache_->Put(*key, value);
    }
  }
  delete key;
  delete value;
  outstanding_operations_.NoBarrierIncrement(-1);
}

void AsyncCache::CancelPut(GoogleString* key, SharedString* value) {
  delete key;
  delete value;
  outstanding_operations_.NoBarrierIncrement(-1);
}

void AsyncCache::Delete(const GoogleString& key) {
  if (IsHealthy()) {
    outstanding_operations_.NoBarrierIncrement(1);
    sequence_->Add(MakeFunction(this, &AsyncCache::DoDelete,
                                &AsyncCache::CancelDelete,
                                new GoogleString(key)));
  }
}

void AsyncCache::DoDelete(GoogleString* key) {
  if (IsHealthy()) {
    cache_->Delete(*key);
  }
  delete key;
  outstanding_operations_.NoBarrierIncrement(-1);
}

void AsyncCache::CancelDelete(GoogleString* key) {
  outstanding_operations_.NoBarrierIncrement(-1);
  delete key;
}

void AsyncCache::ShutDown() {
  stopped_.set_value(true);
  sequence_->CancelPendingFunctions();

  // Note that though we've canceled pending functions, the cache might be
  // still be busy with a function -- say if it's blocked on a wedged memcached.
  //
  // So we can't Disable it until it quiesces.  The only way out from a
  // completely wedged system is kill -9.  Other solutions likely cause
  // core dumps.
  sequence_->Add(MakeFunction(cache_, &CacheInterface::ShutDown));
}

}  // namespace net_instaweb
