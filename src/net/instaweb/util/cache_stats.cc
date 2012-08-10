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

#include "net/instaweb/util/public/cache_stats.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace {

const char kGetCountHistogram[] = "_get_count";
const char kHitLatencyHistogram[] = "_hit_latency_us";
const char kInsertLatencyHistogram[] = "_insert_latency_us";
const char kInsertSizeHistogram[] = "_insert_size_bytes";
const char kLookupSizeHistogram[] = "_lookup_size_bytes";

const char kDeletes[] = "_deletes";
const char kHits[] = "_hits";
const char kInserts[] = "_inserts";
const char kMisses[] = "_misses";

// TODO(jmarantz): tie this to CacheBatcher::kDefaultMaxQueueSize,
// but for now I want to get discrete counts in each bucket.
const int kGetCountHistogramMaxValue = 500;

}  // namespace

namespace net_instaweb {

CacheStats::CacheStats(StringPiece prefix,
                       CacheInterface* cache,
                       Timer* timer,
                       Statistics* statistics)
    : cache_(cache),
      timer_(timer),
      get_count_histogram_(statistics->GetHistogram(
          StrCat(prefix, kGetCountHistogram))),
      hit_latency_us_histogram_(statistics->GetHistogram(
          StrCat(prefix, kHitLatencyHistogram))),
      insert_latency_us_histogram_(statistics->GetHistogram(
          StrCat(prefix, kInsertLatencyHistogram))),
      insert_size_bytes_histogram_(statistics->GetHistogram(
          StrCat(prefix, kInsertSizeHistogram))),
      lookup_size_bytes_histogram_(statistics->GetHistogram(
          StrCat(prefix, kLookupSizeHistogram))),
      deletes_(statistics->GetVariable(StrCat(prefix, kDeletes))),
      hits_(statistics->GetVariable(StrCat(prefix, kHits))),
      inserts_(statistics->GetVariable(StrCat(prefix, kInserts))),
      misses_(statistics->GetVariable(StrCat(prefix, kMisses))),
      name_(StrCat(prefix, "_stats")) {
  get_count_histogram_->SetMaxValue(kGetCountHistogramMaxValue);
}

CacheStats::~CacheStats() {
}

void CacheStats::Initialize(StringPiece prefix, Statistics* statistics) {
  Histogram* get_count_histogram =
      statistics->AddHistogram(StrCat(prefix, kGetCountHistogram));
  get_count_histogram->SetMaxValue(kGetCountHistogramMaxValue);
  statistics->AddHistogram(StrCat(prefix, kHitLatencyHistogram));
  statistics->AddHistogram(StrCat(prefix, kInsertLatencyHistogram));
  statistics->AddHistogram(StrCat(prefix, kInsertSizeHistogram));
  statistics->AddHistogram(StrCat(prefix, kLookupSizeHistogram));
  statistics->AddVariable(StrCat(prefix, kDeletes));
  statistics->AddVariable(StrCat(prefix, kHits));
  statistics->AddVariable(StrCat(prefix, kInserts));
  statistics->AddVariable(StrCat(prefix, kMisses));
}

class CacheStats::StatsCallback : public CacheInterface::Callback {
 public:
  StatsCallback(CacheStats* stats,
                Timer* timer,
                CacheInterface::Callback* callback)
      : stats_(stats),
        timer_(timer),
        callback_(callback),
        validate_candidate_called_(false) {
    start_time_us_ = timer->NowUs();
  }

  virtual ~StatsCallback() {
  }

  // Note that we have to forward validity faithfully here, as if we're
  // wrapping a 2-level cache it will need to know accurately if the value
  // is valid or not.
  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    validate_candidate_called_ = true;
    *callback_->value() = *value();
    return callback_->DelegatedValidateCandidate(key, state);
  }

  virtual void Done(CacheInterface::KeyState state) {
    if (state == CacheInterface::kAvailable) {
      int64 end_time_us = timer_->NowUs();
      stats_->hits_->Add(1);
      stats_->lookup_size_bytes_histogram_->Add((*value())->size());
      stats_->hit_latency_us_histogram_->Add(end_time_us - start_time_us_);
    } else {
      stats_->misses_->Add(1);
    }

    DCHECK(validate_candidate_called_);
    // We don't have to do validation or value forwarding ourselves since
    // whatever we are wrapping must have already called ValidateCandidate().
    callback_->DelegatedDone(state);
    delete this;
  }

 private:
  CacheStats* stats_;
  Timer* timer_;
  CacheInterface::Callback* callback_;
  bool validate_candidate_called_;
  int64 start_time_us_;

  DISALLOW_COPY_AND_ASSIGN(StatsCallback);
};

void CacheStats::Get(const GoogleString& key, Callback* callback) {
  StatsCallback* cb = new StatsCallback(this, timer_, callback);
  cache_->Get(key, cb);
  get_count_histogram_->Add(1);
}

void CacheStats::MultiGet(MultiGetRequest* request) {
  get_count_histogram_->Add(request->size());
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback* key_callback = &(*request)[i];
    key_callback->callback = new StatsCallback(this, timer_,
                                               key_callback->callback);
  }
  cache_->MultiGet(request);
}

void CacheStats::Put(const GoogleString& key, SharedString* value) {
  int64 start_time_us = timer_->NowUs();
  inserts_->Add(1);
  insert_size_bytes_histogram_->Add((*value)->size());
  cache_->Put(key, value);
  insert_latency_us_histogram_->Add(timer_->NowUs() - start_time_us);
}

void CacheStats::Delete(const GoogleString& key) {
  deletes_->Add(1);
  cache_->Delete(key);
}

}  // namespace net_instaweb
