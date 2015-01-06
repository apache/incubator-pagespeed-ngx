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
//
// Retrieves property values stored in the storage system and populate them
// in PropertyPage after validation of the properties.
#ifndef PAGESPEED_OPT_HTTP_PROPERTY_STORE_H_
#define PAGESPEED_OPT_HTTP_PROPERTY_STORE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/opt/http/abstract_property_store_get_callback.h"
#include "pagespeed/opt/http/property_cache.h"

namespace net_instaweb {

class AbstractMutex;
class PropertyCacheValues;
class PropertyValueProtobuf;
class Statistics;
class Timer;

// Abstract interface for implementing PropertyStore which helps to
// retrieve and put properties into the storage system.
class PropertyStore {
 public:
  typedef Callback1<bool> BoolCallback;
  PropertyStore();
  virtual ~PropertyStore();

  // Populates the values field for all the cohorts present in the cohort_list
  // and call the BoolCallback after lookup of all the cohorts are done.
  // BoolCallback is called with true if at least one of the cohorts lookup is
  // succeeded.
  // PropertyPage object is used to validate the entries looked up from cache.
  // AbstractPropertyStoreGetCallback is set in callback parameter and can be
  // used to fast finish the lookup. Client must call DeleteWhenDone() on this
  // callback after that it is no more usable. This parameter can be set to
  // NULL.
  virtual void Get(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      const GoogleString& cache_key_suffix,
      const PropertyCache::CohortVector& cohort_list,
      PropertyPage* page,
      BoolCallback* done,
      AbstractPropertyStoreGetCallback** callback) = 0;

  // Write to storage system for the given key.
  // Callback done can be NULL. BoolCallback done will be called with true if
  // Insert operation is successful.
  // TODO(pulkitg): Remove UserAgentMatcher dependency.
  virtual void Put(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      const GoogleString& cache_key_suffix,
      const PropertyCache::Cohort* cohort,
      const PropertyCacheValues* values,
      BoolCallback* done) = 0;

  // PropertyStore::Get can be cancelled if enable_get_cancellation is true
  // i.e. input done callback will be called as soon as FastFinishLookup() is
  // called on the AbstractPropertyStoreGetCallback callback.
  bool enable_get_cancellation() { return enable_get_cancellation_; }
  void set_enable_get_cancellation(bool x) { enable_get_cancellation_ = x; }

  // The name of this PropertyStore -- used for logging and debugging.
  //
  // It is strongly recommended that you provide a static GoogleString
  // FormatName(...) method for use in formatting the Name() return,
  // and in testing, e.g. in third_party/pagespeed/system/system_caches_test.cc.
  virtual GoogleString Name() const = 0;

 private:
  bool enable_get_cancellation_;
  DISALLOW_COPY_AND_ASSIGN(PropertyStore);
};

// This class manages the lookup for the properties in PropertyStore. It works
// in two mode: Cancellable mode and Non-Cancellable mode.
// Non-Cancellable Mode:
//   - FastFinishLookup() has no-op in this mode.
//   - Done() will be called whenever lookup finishes and calls the
//     done callback based on sucess of the lookup.
//   - DeleteWhenDone() will delete the callback if Done() is already called or
//     set the bit so that callback delete itself after executing Done().
// Cancellable Mode:
//   - FastFinishLookup() will call the done callback if its not yet called.
//   - Done() is same as that in non-cancellable mode but if FastFinishLookup()
//     is called then it will not call the done callback.
//   - DeleteWhenDone() works same as it works in non-cancellable mode.
class PropertyStoreGetCallback : public AbstractPropertyStoreGetCallback {
 public:
  typedef Callback1<bool> BoolCallback;
  PropertyStoreGetCallback(
      AbstractMutex* mutex,
      PropertyPage* page,
      bool is_cancellable,
      BoolCallback* done,
      Timer* timer);
  virtual ~PropertyStoreGetCallback();

  static void InitStats(Statistics* statistics);

  // Try to finish all the pending lookups if possible and call Done as soon as
  // possible.
  virtual void FastFinishLookup();
  // Deletes the callback after done finishes.
  virtual void DeleteWhenDone();
  // Add the given property cache value to the PropertyPage if PropertyPage is
  // not NULL.
  // Returns true if PropertyValueProtobuf is successfully added to
  // PropertyPage.
  bool AddPropertyValueProtobufToPropertyPage(
      const PropertyCache::Cohort* cohort,
      const PropertyValueProtobuf& pcache_value,
      int64 min_write_timestamp_ms);

  // Done is called when lookup is finished. This method is made public so that
  // PropertyStore implementations may call it.
  void Done(bool success);

 protected:
  AbstractMutex* mutex() { return mutex_.get(); }
  PropertyPage* page() { return page_; }

 private:
  scoped_ptr<AbstractMutex> mutex_;
  PropertyPage* page_;
  const bool is_cancellable_;
  BoolCallback* done_;
  bool delete_when_done_;
  bool done_called_;
  Timer* timer_;
  int64 fast_finish_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(PropertyStoreGetCallback);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_OPT_HTTP_PROPERTY_STORE_H_
