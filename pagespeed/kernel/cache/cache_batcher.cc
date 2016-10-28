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

#include "pagespeed/kernel/cache/cache_batcher.h"

#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/atomic_int32.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace {

const char kDroppedGets[] = "cache_batcher_dropped_gets";
const char kCoalescedGets[] = "cache_batcher_coalesced_gets";
const char kQueuedGets[] = "cache_batcher_queued_gets";

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
    if (outstanding_lookups_.BarrierIncrement(-1) == 0) {
      batcher_->GroupComplete();
      delete this;
    }
  }

 private:
  CacheBatcher* batcher_;
  AtomicInt32 outstanding_lookups_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

class CacheBatcher::MultiCallback : public CacheInterface::Callback {
 public:
  MultiCallback(CacheBatcher* batcher, Group* group)
      : batcher_(batcher),
        group_(group) {
  }

  ~MultiCallback() override {
  }

  bool ValidateCandidate(
      const GoogleString& key, CacheInterface::KeyState state) override {
    if (saved_.empty()) {
      std::vector<CacheInterface::Callback*> callbacks;
      batcher_->ExtractInFlightKeys(key, &callbacks);
      DCHECK(!callbacks.empty());
      saved_.reserve(callbacks.size());
      for (Callback* callback : callbacks) {
        saved_.emplace_back(callback, false /* available */, state);
      }
    }

    bool all_succeed = true;
    for (auto& record : saved_) {
      if (!record.available) {
        Callback* callback = record.callback;
        CacheInterface::KeyState tmp_state = state;
        callback->set_value(value());
        if (!callback->DelegatedValidateCandidate(key, state)) {
          all_succeed = false;
          tmp_state = CacheInterface::kNotFound;
        }
        record.available = tmp_state == CacheInterface::kAvailable;
        record.state = tmp_state;
      }
    }
    return all_succeed;
  }

  void Done(CacheInterface::KeyState state) override {
    Group* group = group_;
    batcher_->DecrementInFlightGets(saved_.size());
    for (const auto& record : saved_) {
      record.callback->DelegatedDone(record.state);
    }
    delete this;
    group->Done();
  }

 private:
  CacheBatcher* batcher_;
  Group* group_;
  struct CallbackRecord {
    CallbackRecord(Callback* callback, bool available, KeyState state)
        : callback(callback),
          available(available),
          state(state) {
    }
    Callback* callback;
    bool available;
    KeyState state;
  };
  std::vector<CallbackRecord> saved_;

  DISALLOW_COPY_AND_ASSIGN(MultiCallback);
};

CacheBatcher::CacheBatcher(const Options& options, CacheInterface* cache,
                           AbstractMutex* mutex, Statistics* statistics)
    : cache_(cache),
      dropped_gets_(statistics->GetVariable(kDroppedGets)),
      coalesced_gets_(statistics->GetVariable(kCoalescedGets)),
      queued_gets_(statistics->GetVariable(kQueuedGets)),
      last_batch_size_(-1),
      mutex_(mutex),
      num_in_flight_groups_(0),
      num_in_flight_keys_(0),
      num_pending_gets_(0),
      options_(options),
      shutdown_(false) {
}

CacheBatcher::~CacheBatcher() {
}

GoogleString CacheBatcher::FormatName(StringPiece cache, int parallelism,
                                      int max) {
  return StrCat("Batcher(cache=", cache,
                ",parallelism=", IntegerToString(parallelism),
                ",max=", IntegerToString(max), ")");
}

GoogleString CacheBatcher::Name() const {
  return FormatName(cache_->Name(), options_.max_parallel_lookups,
                    options_.max_pending_gets);
}

void CacheBatcher::InitStats(Statistics* statistics) {
  statistics->AddVariable(kDroppedGets);
  statistics->AddVariable(kCoalescedGets);
  statistics->AddVariable(kQueuedGets);
}

bool CacheBatcher::CanIssueGet() const {
  return !shutdown_ && num_in_flight_groups_ < options_.max_parallel_lookups;
}

bool CacheBatcher::CanQueueCallback() const {
  return !shutdown_ && num_pending_gets_ < options_.max_pending_gets;
}

void CacheBatcher::Get(const GoogleString& key, Callback* callback) {
  bool immediate = false;
  bool drop_get = false;
  {
    ScopedMutex mutex(mutex_.get());

    // Determine if a lookup of this key is already in flight (and this callback
    // should be added to the list of in-flight callbacks under that key), can
    // be issued immediately, should be "queued", or should be dropped.
    bool can_queue = CanQueueCallback();
    if (can_queue) {
      auto iter = in_flight_.find(key);
      if (iter != in_flight_.end()) {
        iter->second.push_back(callback);
        ++num_pending_gets_;
        coalesced_gets_->Add(1);
        return;
      }
    }
    if (CanIssueGet()) {
      immediate = true;
      ++num_in_flight_groups_;
      ++num_pending_gets_;
      ++num_in_flight_keys_;
      in_flight_[key].push_back(callback);
    } else if (can_queue) {
      queued_[key].push_back(callback);
      queued_gets_->Add(1);
      ++num_pending_gets_;
    } else {
      drop_get = true;
    }
  }
  if (immediate) {
    Group* group = new Group(this, 1);
    callback = new MultiCallback(this, group);
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
    if (queued_.empty()) {
      --num_in_flight_groups_;
      return;
    }
    last_batch_size_ = queued_.size();
    request = CreateRequestForQueuedKeys();
  }
  cache_->MultiGet(request);
}

CacheBatcher::MultiGetRequest* CacheBatcher::CreateRequestForQueuedKeys() {
  MultiGetRequest* request = ConvertMapToRequest(queued_);
  MoveQueuedKeys();
  return request;
}

void CacheBatcher::MoveQueuedKeys() {
  num_in_flight_keys_ += queued_.size();
  for (const auto& pair : queued_) {
    const GoogleString& key = pair.first;
    const std::vector<Callback*>& src_callbacks = pair.second;
    bool inserted;

    std::tie(std::ignore, inserted) = in_flight_.emplace(key, src_callbacks);
    // It should be impossible to have both in_flight_[key] and queued_[key]
    // exist and be nonempty simultaneously.
    DCHECK(inserted);
  }
  queued_.clear();
}

CacheBatcher::MultiGetRequest* CacheBatcher::ConvertMapToRequest(
    const CallbackMap &map) {
  Group* group = new Group(this, map.size());
  MultiGetRequest* request = new MultiGetRequest();
  for (const auto& pair : map) {
    const GoogleString& key = pair.first;
    request->emplace_back(key, new MultiCallback(this, group));
  }

  return request;
}

void CacheBatcher::ExtractInFlightKeys(
    const GoogleString& key,
    std::vector<CacheInterface::Callback*>* callbacks) {
  ScopedMutex mutex(mutex_.get());
  auto iter = in_flight_.find(key);
  CHECK(iter != in_flight_.end());
  iter->second.swap(*callbacks);
  in_flight_.erase(iter);
}

void CacheBatcher::Put(const GoogleString& key, const SharedString& value) {
  cache_->Put(key, value);
}

void CacheBatcher::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

void CacheBatcher::DecrementInFlightGets(int n) {
  ScopedMutex mutex(mutex_.get());
  num_pending_gets_ -= n;
  --num_in_flight_keys_;
}

int CacheBatcher::last_batch_size() const {
  ScopedMutex mutex(mutex_.get());
  return last_batch_size_;
}

int CacheBatcher::num_in_flight_keys() {
  ScopedMutex mutex(mutex_.get());
  return num_in_flight_keys_;
}

void CacheBatcher::ShutDown() {
  MultiGetRequest* request = nullptr;
  {
    ScopedMutex mutex(mutex_.get());
    shutdown_ = true;
    if (!queued_.empty()) {
      request = ConvertMapToRequest(queued_);
      queued_.clear();
    }
  }

  if (request != nullptr) {
    ReportMultiGetNotFound(request);
  }
  cache_->ShutDown();
}

}  // namespace net_instaweb
