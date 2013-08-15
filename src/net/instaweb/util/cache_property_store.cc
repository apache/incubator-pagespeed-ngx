/*
 * Copyright 2013 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/util/public/cache_property_store.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"

namespace net_instaweb {

// Property cache key prefixes.
const char CachePropertyStore::kPagePropertyCacheKeyPrefix[] = "prop_page/";

CachePropertyStore::CachePropertyStore(const GoogleString& cache_key_prefix,
                                       CacheInterface* cache,
                                       Timer* timer,
                                       Statistics* stats,
                                       ThreadSystem* thread_system)
    : cache_key_prefix_(cache_key_prefix),
      default_cache_(cache),
      timer_(timer),
      stats_(stats),
      thread_system_(thread_system) {
}

CachePropertyStore::~CachePropertyStore() {
  STLDeleteValues(&cohort_cache_map_);
}

namespace {

class CachePropertyStoreGetCallback : public PropertyStoreGetCallback {
 public:
  CachePropertyStoreGetCallback(
      AbstractMutex* mutex,
      PropertyPage* page,
      bool is_cancellable,
      BoolCallback* done,
      Timer* timer)
      : PropertyStoreGetCallback(
          mutex, page, is_cancellable, done, timer) {
  }
  virtual ~CachePropertyStoreGetCallback() {
  }

  void SetStateInPropertyPage(
      const PropertyCache::Cohort* cohort,
      CacheInterface::KeyState state,
      bool valid) {
    ScopedMutex lock(mutex());
    if (page() == NULL) {
      return;
    }
    page()->log_record()->SetCacheStatusForCohortInfo(
        page()->page_type(),
        cohort->name(),
        valid,
        state);
    page()->SetCacheState(cohort, state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CachePropertyStoreGetCallback);
};

// Tracks multiple cache lookups.  When they are all complete, page->Done() is
// called.
//
// TODO(pulkitg): Use CacheInterface::MultiGet() instead of using
// CacheInterface::Get() for each cohort.
class CachePropertyStoreCallbackCollector {
 public:
  CachePropertyStoreCallbackCollector(
      CachePropertyStoreGetCallback* property_store_callback,
      int num_pending,
      AbstractMutex* mutex)
      : property_store_callback_(property_store_callback),
        pending_(num_pending),
        success_(false),
        mutex_(mutex) {
  }

  void Done(bool success) {
    {
      ScopedMutex lock(mutex_.get());
      success_ |= success;  // Declare victory a if *any* lookups completed.
      --pending_;
      if (pending_ > 0) {
        return;
      }
    }
    property_store_callback_->Done(success_);
    delete this;
  }

 private:
  CachePropertyStoreGetCallback* property_store_callback_;
  int pending_;
  bool success_;
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(CachePropertyStoreCallbackCollector);
};

// Helper class to receive low-level cache callbacks, decode them
// as properties.
class CachePropertyStoreCacheCallback : public CacheInterface::Callback {
 public:
  CachePropertyStoreCacheCallback(
      const PropertyCache::Cohort* cohort,
      CachePropertyStoreGetCallback* property_store_callback,
      CachePropertyStoreCallbackCollector* callback_collector)
      : cohort_(cohort),
        property_store_callback_(property_store_callback),
        callback_collector_(callback_collector) {
  }
  virtual ~CachePropertyStoreCacheCallback() {}

  virtual void Done(CacheInterface::KeyState state) {
    bool valid = false;
    if (state == CacheInterface::kAvailable) {
      StringPiece value_string = value()->Value();
      ArrayInputStream input(value_string.data(), value_string.size());
      PropertyCacheValues values;
      if (values.ParseFromZeroCopyStream(&input)) {
        int64 min_write_timestamp_ms = kint64max;
        // The values in a cohort could have different write_timestamp_ms
        // values, since it is populated in UpdateValue.  But since all values
        // in a cohort are written (and read) together we need to treat either
        // all as valid or none as valid.  Hence we look at the oldest write
        // timestamp to make this decision.
        for (int i = 0; i < values.value_size(); ++i) {
          min_write_timestamp_ms = std::min(
              min_write_timestamp_ms, values.value(i).write_timestamp_ms());
        }
        // Return valid for empty cohort, and if IsCacheValid returns true for
        // Value with oldest timestamp.
        if (values.value_size() == 0) {
          valid = true;
        } else {
          for (int i = 0; i < values.value_size(); ++i) {
            const PropertyValueProtobuf& pcache_value = values.value(i);
            valid = property_store_callback_->
                AddPropertyValueProtobufToPropertyPage(
                    cohort_, pcache_value, min_write_timestamp_ms);
          }
        }
      }
    }
    property_store_callback_->SetStateInPropertyPage(cohort_, state, valid);
    callback_collector_->Done(valid);
    delete this;
  }

 private:
  const PropertyCache::Cohort* cohort_;
  CachePropertyStoreGetCallback* property_store_callback_;
  CachePropertyStoreCallbackCollector* callback_collector_;

  DISALLOW_COPY_AND_ASSIGN(CachePropertyStoreCacheCallback);
};

}  // namespace

GoogleString CachePropertyStore::CacheKey(
    const StringPiece& url,
    const StringPiece& options_signature_hash,
    const StringPiece& cache_key_suffix,
    const PropertyCache::Cohort* cohort) const {
  return StrCat(
      cache_key_prefix_,
      url, "_",
      options_signature_hash,
      cache_key_suffix, "@",
      cohort->name());
}

void CachePropertyStore::Get(
    const GoogleString& url,
    const GoogleString& options_signature_hash,
    const GoogleString& cache_key_suffix,
    const PropertyCache::CohortVector& cohort_list,
    PropertyPage* page,
    BoolCallback* done,
    AbstractPropertyStoreGetCallback** callback) {
  if (cohort_list.empty()) {
    *callback = NULL;
    done->Run(true);
    return;
  }
  CachePropertyStoreGetCallback* property_store_get_callback =
      new CachePropertyStoreGetCallback(
          thread_system_->NewMutex(),
          page,
          enable_get_cancellation(),
          done,
          timer_);
  *callback = property_store_get_callback;
  CachePropertyStoreCallbackCollector* collector =
      new CachePropertyStoreCallbackCollector(
          property_store_get_callback,
          cohort_list.size(),
          thread_system_->NewMutex());
  for (int j = 0, n = cohort_list.size(); j < n; ++j) {
    const PropertyCache::Cohort* cohort = cohort_list[j];
    CohortCacheMap::iterator cohort_itr =
        cohort_cache_map_.find(cohort->name());
    CHECK(cohort_itr != cohort_cache_map_.end());
    const GoogleString cache_key = CacheKey(
        url, options_signature_hash, cache_key_suffix, cohort);
    cohort_itr->second->Get(
        cache_key,
        new CachePropertyStoreCacheCallback(
            cohort, property_store_get_callback, collector));
  }
}

void CachePropertyStore::Put(const GoogleString& url,
                             const GoogleString& options_signature_hash,
                             const GoogleString& cache_key_suffix,
                             const PropertyCache::Cohort* cohort,
                             const PropertyCacheValues* values,
                             BoolCallback* done) {
  GoogleString value;
  StringOutputStream sstream(&value);
  values->SerializeToZeroCopyStream(&sstream);
  CohortCacheMap::iterator cohort_itr = cohort_cache_map_.find(cohort->name());
  CHECK(cohort_itr != cohort_cache_map_.end());
  const GoogleString cache_key = CacheKey(
      url, options_signature_hash, cache_key_suffix, cohort);
  cohort_itr->second->PutSwappingString(cache_key, &value);
  if (done != NULL) {
    done->Run(true);
  }
}

void CachePropertyStore::AddCohort(const GoogleString& cohort) {
  AddCohortWithCache(cohort, default_cache_);
}

void CachePropertyStore::AddCohortWithCache(
    const GoogleString& cohort, CacheInterface* cache) {
  std::pair<CohortCacheMap::iterator, bool> insertions =
      cohort_cache_map_.insert(
        make_pair(cohort, static_cast<CacheInterface*>(NULL)));
  CHECK(insertions.second) << cohort << " is added twice.";
  // Create a new CacheStats for every cohort so that we can track cache
  // statistics independently for every cohort.
  CacheInterface* cache_stats = new CacheStats(
        PropertyCache::GetStatsPrefix(cohort), cache, timer_, stats_);
  insertions.first->second = cache_stats;
}

}  // namespace net_instaweb
