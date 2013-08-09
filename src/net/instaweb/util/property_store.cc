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

#include "net/instaweb/util/public/property_store.h"

#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/property_cache.h"

namespace net_instaweb {

PropertyStore::PropertyStore()
    : enable_get_cancellation_(false) {
}

PropertyStore::~PropertyStore() {
}

PropertyStoreGetCallback::PropertyStoreGetCallback(
    AbstractMutex* mutex,
    PropertyPage* page,
    bool is_cancellable,
    BoolCallback* done)
    : mutex_(mutex),
      page_(page),
      is_cancellable_(is_cancellable),
      done_(done),
      delete_when_done_(false),
      done_called_(false) {
}

PropertyStoreGetCallback::~PropertyStoreGetCallback() {
}

void PropertyStoreGetCallback::FastFinishLookup() {
  if (!is_cancellable_) {
    // Returns early as it is in non-cancelable mode.
    return;
  }

  // Call done callback if it is not yet called.
  BoolCallback* done = NULL;
  {
    ScopedMutex lock(mutex());
    if (done_ == NULL) {
      return;
    }
    // NULL out page_ since we shouldn't be touching it any longer.
    page_ = NULL;
    done = done_;
    done_ = NULL;
  }

  // Don't run callback while holding lock.
  done->Run(false);
}

void PropertyStoreGetCallback::Done(bool success) {
  BoolCallback* done = NULL;
  bool delete_this = false;
  {
    ScopedMutex lock(mutex());
    DCHECK(!done_called_);
    // done_ will be NULL if FastFinishLookup() is called, though Done() is not
    // yet called.
    if (done_ != NULL) {
      page_ = NULL;
      done = done_;
      done_ = NULL;
    }
    delete_this = delete_when_done_;
    done_called_ = true;
  }

  // Call done callback if it is not yet called.
  if (done != NULL) {
    done->Run(success);
  }

  // If DeleteWhenDone() is already called, delete this.
  if (delete_this) {
    delete this;
  }
}

void PropertyStoreGetCallback::DeleteWhenDone() {
  {
    ScopedMutex lock(mutex());
    if (delete_when_done_) {
      LOG(DFATAL) << "PropertyStoreGetCallback::DeleteWhenDone() "
                  << "is called twice.";
    }
    delete_when_done_ = true;
    if (!done_called_) {
      // Returns if Done() is not yet called.
      return;
    }
  }
  delete this;
}

bool PropertyStoreGetCallback::AddPropertyValueProtobufToPropertyPage(
      const PropertyCache::Cohort* cohort,
      const PropertyValueProtobuf& pcache_value,
      int64 min_write_timestamp_ms) {
  ScopedMutex lock(mutex());
  if (page() == NULL || !page()->IsCacheValid(min_write_timestamp_ms)) {
    return false;
  }
  page()->AddValueFromProtobuf(cohort, pcache_value);
  return true;
}

}  // namespace net_instaweb
