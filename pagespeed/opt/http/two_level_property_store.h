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
// Composes two property stores to form a two level property cache storage
// system.
// TwoLevelPropertyStore::Get() also has capability to fast return the results
// (i.e. results of the primary property store lookup) if cancel is called.
#ifndef PAGESPEED_OPT_HTTP_TWO_LEVEL_PROPERTY_STORE_H_
#define PAGESPEED_OPT_HTTP_TWO_LEVEL_PROPERTY_STORE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/opt/http/abstract_property_store_get_callback.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/property_store.h"

namespace net_instaweb {

class PropertyCacheValues;
class ThreadSystem;

class TwoLevelPropertyStore : public PropertyStore {
 public:
  TwoLevelPropertyStore(PropertyStore* primary_property_store,
                        PropertyStore* secondary_propery_store,
                        ThreadSystem* thread_system);
  virtual ~TwoLevelPropertyStore();

  // It issues a lookup on the primary_property_store and lookup on
  // secondary_property_store will only be issued if properties are not
  // available in primary_property_store and lookup is not yet cancelled.
  virtual void Get(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      const GoogleString& cache_key_suffix,
      const PropertyCache::CohortVector& cohort_list,
      PropertyPage* page,
      BoolCallback* done,
      AbstractPropertyStoreGetCallback** callback);

  // Write to both the storage system for the given key.
  virtual void Put(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      const GoogleString& cache_key_suffix,
      const PropertyCache::Cohort* cohort,
      const PropertyCacheValues* values,
      BoolCallback* done);

  virtual GoogleString Name() const;

 private:
  PropertyStore* primary_property_store_;
  PropertyStore* secondary_property_store_;
  ThreadSystem* thread_system_;
  DISALLOW_COPY_AND_ASSIGN(TwoLevelPropertyStore);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_OPT_HTTP_TWO_LEVEL_PROPERTY_STORE_H_
