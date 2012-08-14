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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

AsyncCache::~AsyncCache() {
}

bool AsyncCache::CanIssueGet() {
  ScopedMutex lock(mutex_.get());
  return CanIssueGetMutexHeld();
}

bool AsyncCache::CanIssueGetMutexHeld() const {
  return (active_threads_ < num_threads_);
}

bool AsyncCache::InitiateLookup() {
  ScopedMutex mutex(mutex_.get());
  if (CanIssueGetMutexHeld()) {
    ++active_threads_;
    return true;
  }
  return false;
}

void AsyncCache::Get(const GoogleString& key, Callback* callback) {
  if (InitiateLookup()) {
    QueuedWorkerPool::Sequence* sequence = pool_->NewSequence();
    if (sequence == NULL) {
      CancelGet(key, callback, sequence);
    } else {
      sequence->Add(MakeFunction(this, &AsyncCache::DoGet,
                                 &AsyncCache::CancelGet,
                                 key, callback, sequence));
    }
  } else {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  }
}

void AsyncCache::MultiGet(MultiGetRequest* request) {
  if (InitiateLookup()) {
    QueuedWorkerPool::Sequence* sequence = pool_->NewSequence();
    if (sequence == NULL) {
      CancelMultiGet(request, sequence);
    } else {
      sequence->Add(MakeFunction(this, &AsyncCache::DoMultiGet,
                                 &AsyncCache::CancelMultiGet, request,
                                 sequence));
    }
  } else {
    MultiGetReportNotFound(request);
  }
}

void AsyncCache::DoGet(GoogleString key, Callback* callback,
                       QueuedWorkerPool::Sequence* sequence) {
  pool_->FreeSequence(sequence);
  cache_->Get(key, callback);

  {
    ScopedMutex mutex(mutex_.get());
    --active_threads_;
  }
}

void AsyncCache::CancelGet(GoogleString key, Callback* callback,
                           QueuedWorkerPool::Sequence* sequence) {
  // It is not necessary to free the sequence in this call.  In fact the
  // sequence is already being freed, which is why this method is called.

  ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  {
    ScopedMutex mutex(mutex_.get());
    --active_threads_;
  }
}

void AsyncCache::DoMultiGet(MultiGetRequest* request,
                            QueuedWorkerPool::Sequence* sequence) {
  pool_->FreeSequence(sequence);
  cache_->MultiGet(request);

  {
    ScopedMutex mutex(mutex_.get());
    --active_threads_;
  }
}

void AsyncCache::CancelMultiGet(MultiGetRequest* request,
                                QueuedWorkerPool::Sequence* sequence) {
  // It is not necessary to free the sequence in this call.  In fact the
  // sequence is already being freed, which is why this method is called.

  MultiGetReportNotFound(request);
  {
    ScopedMutex mutex(mutex_.get());
    --active_threads_;
  }
}

void AsyncCache::MultiGetReportNotFound(MultiGetRequest* request) {
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback& key_callback = (*request)[i];
    ValidateAndReportResult(key_callback.key, CacheInterface::kNotFound,
                            key_callback.callback);
  }
  delete request;
}

void AsyncCache::Put(const GoogleString& key, SharedString* value) {
  cache_->Put(key, value);
}

void AsyncCache::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

void AsyncCache::StopCacheGets() {
  ScopedMutex mutex(mutex_.get());
  num_threads_ = 0;
}

}  // namespace net_instaweb
