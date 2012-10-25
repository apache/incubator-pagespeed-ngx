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

#include "net/instaweb/util/public/async_cache.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

AsyncCache::AsyncCache(CacheInterface* cache, QueuedWorkerPool* pool)
    : cache_(cache),
      name_(StrCat("AsyncCache using ", cache_->Name())) {
  CHECK(cache->IsBlocking());
  sequence_ = pool->NewSequence();
  sequence_->set_max_queue_size(kMaxQueueSize);
}

AsyncCache::~AsyncCache() {
  DCHECK_EQ(0, outstanding_operations());
}

void AsyncCache::Get(const GoogleString& key, Callback* callback) {
  if (IsHealthy()) {
    outstanding_operations_.increment(1);
    sequence_->Add(MakeFunction(this, &AsyncCache::DoGet,
                                &AsyncCache::CancelGet,
                                new GoogleString(key), callback));
  } else {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
}

void AsyncCache::MultiGet(MultiGetRequest* request) {
  outstanding_operations_.increment(1);
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
    outstanding_operations_.increment(-1);
  } else {
    CancelGet(key, callback);
  }
}

void AsyncCache::CancelGet(GoogleString* key, Callback* callback) {
  ValidateAndReportResult(*key, CacheInterface::kNotFound, callback);
  delete key;
  outstanding_operations_.increment(-1);
}

void AsyncCache::DoMultiGet(MultiGetRequest* request) {
  if (IsHealthy()) {
    cache_->MultiGet(request);
    outstanding_operations_.increment(-1);
  } else {
    CancelMultiGet(request);
  }
}

void AsyncCache::CancelMultiGet(MultiGetRequest* request) {
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback& key_callback = (*request)[i];
    ValidateAndReportResult(key_callback.key, CacheInterface::kNotFound,
                            key_callback.callback);
  }
  delete request;
  outstanding_operations_.increment(-1);
}

void AsyncCache::Put(const GoogleString& key, SharedString* value) {
  if (IsHealthy()) {
    outstanding_operations_.increment(1);
    value = new SharedString(*value);
    sequence_->Add(
        MakeFunction(this, &AsyncCache::DoPut, &AsyncCache::CancelPut,
                     new GoogleString(key), value));
  }
}

void AsyncCache::DoPut(GoogleString* key, SharedString* value) {
  if (IsHealthy()) {
    // TODO(jmarantz): Start timers at the beginning of each operation,
    // particularly this one, and use long delays as a !IsHealthy signal.
    cache_->Put(*key, value);
  }
  delete key;
  delete value;
  outstanding_operations_.increment(-1);
}

void AsyncCache::CancelPut(GoogleString* key, SharedString* value) {
  delete key;
  delete value;
  outstanding_operations_.increment(-1);
}

void AsyncCache::Delete(const GoogleString& key) {
  if (IsHealthy()) {
    outstanding_operations_.increment(1);
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
  outstanding_operations_.increment(-1);
}

void AsyncCache::CancelDelete(GoogleString* key) {
  outstanding_operations_.increment(-1);
  delete key;
}

void AsyncCache::StopCacheActivity() {
  stopped_.set_value(true);
  sequence_->CancelPendingFunctions();
}

}  // namespace net_instaweb
