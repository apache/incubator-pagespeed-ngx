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

#include "net/instaweb/util/public/cache_batcher.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kDroppedGets[] = "cache_batcher_dropped_gets";

}  // namespace

namespace net_instaweb {

// Used to track the progress of a MultiGet, so that we can keep track
// of how many lookups are outstanding, where a MultiGet counts as one
// lookup independent of how many keys it has.
class CacheBatcher::Group {
 public:
  Group(CacheBatcher* batcher, int group_size)
      : batcher_(batcher),
        outstanding_lookups_(group_size) {
  }

  void Done() {
    if (outstanding_lookups_.increment(-1) == 0) {
      batcher_->GroupComplete();
      delete this;
    }
  }

 private:
  CacheBatcher* batcher_;
  AtomicInt32 outstanding_lookups_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

// TODO(jmarantz): factor out a delegating Callback helper base class.
// The only purpose of this class is to call Group::Done on Done.
class CacheBatcher::BatcherCallback : public CacheInterface::Callback {
 public:
  BatcherCallback(CacheInterface::Callback* callback, Group* group)
      : callback_(callback),
        group_(group),
        validate_candidate_called_(false) {
  }

  virtual ~BatcherCallback() {}

  // Note that we have to forward validity faithfully here, since if we're
  // wrapping a 2-level cache it will need to know accurately if the value
  // is valid or not.
  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    validate_candidate_called_ = true;
    *callback_->value() = *value();
    return callback_->DelegatedValidateCandidate(key, state);
  }

  virtual void Done(CacheInterface::KeyState state) {
    DCHECK(validate_candidate_called_);
    // We don't have to do validation or value forwarding ourselves since
    // whatever we are wrapping must have already called ValidateCandidate().
    callback_->DelegatedDone(state);
    group_->Done();
    delete this;
  }

  CacheInterface::Callback* callback_;
  Group* group_;
  bool validate_candidate_called_;
};

CacheBatcher::CacheBatcher(CacheInterface* cache, AbstractMutex* mutex,
                           Statistics* statistics)
    : cache_(cache),
      mutex_(mutex),
      name_(StrCat("CacheBatcher using ", cache_->Name())),
      last_batch_size_(-1),
      pending_(0),
      max_parallel_lookups_(kDefaultMaxParallelLookups),
      max_queue_size_(kDefaultMaxQueueSize),
      dropped_gets_(statistics->GetVariable(kDroppedGets)) {
}

CacheBatcher::~CacheBatcher() {
}

void CacheBatcher::Initialize(Statistics* statistics) {
  statistics->AddVariable(kDroppedGets);
}

bool CacheBatcher::CanIssueGet() const {
  return (pending_ < max_parallel_lookups_);
}

void CacheBatcher::Get(const GoogleString& key, Callback* callback) {
  bool immediate = false;
  bool drop_get = false;
  {
    ScopedMutex mutex(mutex_.get());

    if (CanIssueGet()) {
      immediate = true;
      ++pending_;
    } else if (queue_.size() >= max_queue_size_) {
      drop_get = true;
    } else {
      queue_.push_back(KeyCallback(key, callback));
    }
  }
  if (immediate) {
    Group* group = new Group(this, 1);
    callback = new BatcherCallback(callback, group);
    cache_->Get(key, callback);
  } else if (drop_get) {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
    dropped_gets_->Add(1);
  }
}

void CacheBatcher::GroupComplete() {
  MultiGetRequest* request = NULL;

  {
    ScopedMutex mutex(mutex_.get());
    if (queue_.empty()) {
      --pending_;
      return;
    }
    request = new MultiGetRequest;
    last_batch_size_ = queue_.size();
    request->swap(queue_);
  }
  Group* group = new Group(this, request->size());
  for (int i = 0, n = request->size(); i < n; ++i) {
    (*request)[i].callback = new BatcherCallback((*request)[i].callback,
                                                 group);
  }
  cache_->MultiGet(request);
}

void CacheBatcher::Put(const GoogleString& key, SharedString* value) {
  cache_->Put(key, value);
}

void CacheBatcher::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

int CacheBatcher::Pending() {
  ScopedMutex mutex(mutex_.get());
  return pending_;
}

}  // namespace net_instaweb
