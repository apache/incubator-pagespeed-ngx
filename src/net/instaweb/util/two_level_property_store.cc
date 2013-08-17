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

#include "net/instaweb/util/public/two_level_property_store.h"

#include "base/logging.h"
#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_property_store_get_callback.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace {

// This class manages the lookup across two property stores. This class ensures
// following things:
// - If lookup was successful and all cohorts are available in
//   primary_property_store, then call the done_ callback without calling
//   lookup on secondary_property_store.
// - If lookup was not successful or some of the cohorts are not available in
//   primary_property_store, then issue a lookup on secondary_property_store.
// - If FastFinishLookup is called, it checks:
//   - If primary_property_store lookup is not yet finished, then wait until
//     primary lookup is finished and after it is finished, call the the done_
//     callback even if all the cohort are not available and don't issue
//     secondary lookup.
//   - If primary_property_store lookup is finished and secondary lookup is in
//     progress, then call FinishFastLookup on secondary property store, so
//     that done_ callback can be called as soon as possible.
//   - If both lookups are finished, do nothing.
// - If DeleteWhenDone() is called, it checks:
//   - If both lookups are done, delete itself.
//   - If lookup is in progress, mark a bit delete_when_done_ to true, so that
//     it deletes itself whenever lookup is finished.
class TwoLevelPropertyStoreGetCallback
    : public AbstractPropertyStoreGetCallback {
 public:
  typedef Callback1<bool> BoolCallback;
  TwoLevelPropertyStoreGetCallback(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      const GoogleString& cache_key_suffix,
      const PropertyCache::CohortVector& cohort_list,
      PropertyPage* page,
      BoolCallback* done,
      AbstractMutex* mutex,
      PropertyStore* primary_property_store,
      PropertyStore* secondary_property_store)
      : url_(url),
        options_signature_hash_(options_signature_hash),
        cache_key_suffix_(cache_key_suffix),
        page_(page),
        done_(done),
        mutex_(mutex),
        primary_property_store_(primary_property_store),
        secondary_property_store_(secondary_property_store),
        secondary_property_store_get_callback_(NULL),
        fast_finish_lookup_called_(false),
        lookup_level_(kFirstLevelLooking),
        delete_when_done_(false),
        first_level_result_(false),
        secondary_lookup_(false) {
    for (int j = 0, n = cohort_list.size(); j < n; ++j) {
      const PropertyCache::Cohort* cohort = cohort_list[j];
      cohort_list_.push_back(cohort);
    }
  }

  virtual ~TwoLevelPropertyStoreGetCallback() {
    if (secondary_property_store_get_callback_ != NULL) {
      secondary_property_store_get_callback_->DeleteWhenDone();
    }
  }

  virtual void FastFinishLookup() {
    AbstractPropertyStoreGetCallback* secondary_property_store_get_callback =
        NULL;
    {
      ScopedMutex lock(mutex_.get());
      fast_finish_lookup_called_ = true;
      if (lookup_level_ != kSecondLevelLooking) {
        // Return without calling the done_ callback if:
        // First level lookup is in progress : We always want first level lookup
        //     to be completed.
        // Both lookup are completed: done_ callback was already called by
        //     Done() function.
        return;
      }

      DCHECK(secondary_property_store_get_callback_ != NULL);
      secondary_property_store_get_callback =
          secondary_property_store_get_callback_;
    }

    // Fast finish the lookup from secondary property store.
    secondary_property_store_get_callback->FastFinishLookup();
  }

  virtual void DeleteWhenDone() {
    {
      ScopedMutex lock(mutex_.get());
      delete_when_done_ = true;
      // We should only ever delete ourselves if lookup is done.
      if (!ShouldDeleteLocked()) {
        return;
      }
    }

    delete this;
  }

  // Called after the primary lookup is done.
  void PrimaryLookupDone(bool success) {
    bool should_delete = false;
    BoolCallback* done = NULL;
    {
      ScopedMutex lock(mutex_.get());
      first_level_result_ = success;

      // Check if it is FastFinishLookup() is called or lookup for any cohort
      // ended up in a miss.
      for (int j = 0, n = cohort_list_.size(); j < n; ++j) {
        // Collect all the cohorts which is not present after lookup in
        // primary_property_store is done.
        const PropertyCache::Cohort* cohort = cohort_list_[j];
        if (!page_->IsCohortPresent(cohort)) {
          secondary_lookup_cohort_list_.push_back(cohort);
        }
      }

      if (fast_finish_lookup_called_ || secondary_lookup_cohort_list_.empty()) {
        lookup_level_ = kDone;
        done = done_;
        done_ = NULL;

        // We should only ever delete ourselves if we've been canceled or we got
        // results for all cohorts from the first level property store.
        should_delete = ShouldDeleteLocked();
      } else {
        secondary_lookup_ = true;
        // Do not issue the secondary level lookup while holding mutex as it may
        // lead to deadlock if secondary lookup finishes in the same thread and
        // call Done which also tries to get the Mutex.
      }
      DCHECK(!(should_delete && (lookup_level_ != kDone)));
    }

    if (!secondary_lookup_) {
      // Run the done_ callback if secondary lookup is not needed and the delete
      // the callback if DeleteWhenDone is already called.
      done->Run(success);
      if (should_delete) {
        delete this;
      }
      return;
    }

    // Second level lookup will be initiated only if FastFinishLookup() is not
    // called and some cohorts are not found in first level lookup.
    IssueSecondaryGet();
  }

  void SecondaryLookupDone(bool success) {
    bool should_delete = false;
    BoolCallback* done = NULL;
    {
      ScopedMutex lock(mutex_.get());
      DCHECK(done_ != NULL);

      // Second Level lookup finished.
      lookup_level_ = kDone;
      success |= first_level_result_;
      done = done_;
      done_ = NULL;

      // We should only ever delete ourselves if all internal states are
      // updated.
      should_delete = ShouldDeleteLocked();
    }
    if (success) {
      for (int j = 0, n = secondary_lookup_cohort_list_.size(); j < n; ++j) {
        const PropertyCache::Cohort* cohort = secondary_lookup_cohort_list_[j];
        PropertyCacheValues values;
        if (page_->EncodePropertyCacheValues(cohort, &values)) {
          primary_property_store_->Put(
              url_,
              options_signature_hash_,
              cache_key_suffix_,
              cohort,
              &values,
              NULL);
        }
      }
    }

    // No class level variable is safe to use beyond this point.
    done->Run(success);
    if (should_delete) {
      delete this;
    }
  }

 private:
  // Issue lookup from secondary_primary_store.
  void IssueSecondaryGet() {
    AbstractPropertyStoreGetCallback* secondary_property_store_get_callback =
        NULL;
    secondary_property_store_->Get(
        url_,
        options_signature_hash_,
        cache_key_suffix_,
        secondary_lookup_cohort_list_,
        page_,
        NewCallback(this,
                    &TwoLevelPropertyStoreGetCallback::SecondaryLookupDone),
        &secondary_property_store_get_callback);

    bool fast_finish_lookup_called = false;
    bool should_delete = false;
    {
      ScopedMutex lock(mutex_.get());
      secondary_property_store_get_callback_ =
          secondary_property_store_get_callback;
      // lookup_level_ will be kDone if secondary lookup finishes immediately.
      if (lookup_level_ != kDone) {
        lookup_level_ = kSecondLevelLooking;
        fast_finish_lookup_called = fast_finish_lookup_called_;
      } else {
        // We'll only delete ourselves if the second level lookup finished
        // inline.
        should_delete = ShouldDeleteLocked();
      }
    }

    if (fast_finish_lookup_called) {
      // FastFinishLookup() is called before
      // secondary_property_store_get_callback is assigned.
      secondary_property_store_get_callback->FastFinishLookup();
    }

    if (should_delete) {
      delete this;
    }
  }

  // Returns true if it is safe to delete this callback, false otherwise.
  bool ShouldDeleteLocked() {
    mutex_->DCheckLocked();
    if (secondary_lookup_ &&
        secondary_property_store_get_callback_ == NULL) {
      // We've decided to issue the second lookup but haven't yet updated our
      // internal state.
      return false;
    }

    if (lookup_level_ != kDone) {
      // Lookup is not yet finished.
      return false;
    }

    if (!delete_when_done_) {
      // DeleteWhenDone() is not yet called.
      return false;
    }

    // Safe to delete the callback.
    return true;
  }

  enum LookupLevel {
    kFirstLevelLooking,   // Lookup from primary_property_store is in progress.
    kSecondLevelLooking,  // Lookup from secondary_property_store is in
                          // progress.
    kDone,                // Lookup is finished.
  };

  GoogleString url_;
  GoogleString options_signature_hash_;
  const GoogleString& cache_key_suffix_;
  PropertyCache::CohortVector cohort_list_;
  PropertyPage* page_;  // page_ becomes NULL as soon as Done() is called.
  BoolCallback* done_;
  scoped_ptr<AbstractMutex> mutex_;
  PropertyStore* primary_property_store_;
  PropertyStore* secondary_property_store_;
  AbstractPropertyStoreGetCallback* secondary_property_store_get_callback_;
  bool fast_finish_lookup_called_;
  LookupLevel lookup_level_;
  bool delete_when_done_;
  bool first_level_result_;
  bool secondary_lookup_;
  PropertyCache::CohortVector secondary_lookup_cohort_list_;
  DISALLOW_COPY_AND_ASSIGN(TwoLevelPropertyStoreGetCallback);
};

}  // namespace

TwoLevelPropertyStore::TwoLevelPropertyStore(
    PropertyStore* primary_property_store,
    PropertyStore* secondary_property_store,
    ThreadSystem* thread_system)
    : primary_property_store_(primary_property_store),
      secondary_property_store_(secondary_property_store),
      thread_system_(thread_system) {
  CHECK(primary_property_store_ != NULL);
  CHECK(secondary_property_store_ != NULL);
  secondary_property_store_->set_enable_get_cancellation(true);
}

TwoLevelPropertyStore::~TwoLevelPropertyStore() {
}

void TwoLevelPropertyStore::Get(
    const GoogleString& url,
    const GoogleString& options_signature_hash,
    const GoogleString& cache_key_suffix,
    const PropertyCache::CohortVector& cohort_list,
    PropertyPage* page,
    BoolCallback* done,
    AbstractPropertyStoreGetCallback** callback) {
  TwoLevelPropertyStoreGetCallback* two_level_property_store_get_callback =
      new TwoLevelPropertyStoreGetCallback(
          url,
          options_signature_hash,
          cache_key_suffix,
          cohort_list,
          page,
          done,
          thread_system_->NewMutex(),
          primary_property_store_,
          secondary_property_store_);
  *callback = two_level_property_store_get_callback;

  AbstractPropertyStoreGetCallback* primary_property_store_get_callback = NULL;
  primary_property_store_->Get(
      url,
      options_signature_hash,
      cache_key_suffix,
      cohort_list,
      page,
      NewCallback(two_level_property_store_get_callback,
                  &TwoLevelPropertyStoreGetCallback::PrimaryLookupDone),
      &primary_property_store_get_callback);

  if (primary_property_store_get_callback != NULL) {
    // Delete the primary store get callback when it is done as it is not needed
    // any more.
    primary_property_store_get_callback->DeleteWhenDone();
  }
}

void TwoLevelPropertyStore::Put(
    const GoogleString& url,
    const GoogleString& options_signature_hash,
    const GoogleString& cache_key_suffix,
    const PropertyCache::Cohort* cohort,
    const PropertyCacheValues* values,
    BoolCallback* done) {
  // TODO(pulkitg): Pass actual callback instead of NULL.
  primary_property_store_->Put(
      url, options_signature_hash, cache_key_suffix, cohort, values, NULL);
  secondary_property_store_->Put(
      url, options_signature_hash, cache_key_suffix, cohort, values, NULL);
  if (done != NULL) {
    done->Run(true);
  }
}

}  // namespace net_instaweb
