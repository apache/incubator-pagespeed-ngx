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
// Reads the properties stored in the CacheInterface and populates them in
// PropertyPage.
// There is a CacheInterface object for every cohort which is stored in
// CohortCacheMap and read/write for a cohort happens on its respective
// CacheInterface object.

#ifndef PAGESPEED_OPT_HTTP_CACHE_PROPERTY_STORE_H_
#define PAGESPEED_OPT_HTTP_CACHE_PROPERTY_STORE_H_

#include <map>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/opt/http/abstract_property_store_get_callback.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/property_store.h"

namespace net_instaweb {

class PropertyCacheValues;
class Statistics;
class ThreadSystem;
class Timer;

class CachePropertyStore : public PropertyStore {
 public:
  // Property cache key prefixes.
  static const char kPagePropertyCacheKeyPrefix[];

  // Does not take the ownership of cache, timer and stats object.
  // L2-only caches should be used for CachePropertyStore.  We cannot use the L1
  // cache because this data can get stale quickly.
  CachePropertyStore(const GoogleString& cache_key_prefix,
                     CacheInterface* cache,
                     Timer* timer,
                     Statistics* stats,
                     ThreadSystem* thread_system);
  virtual ~CachePropertyStore();

  // Cache lookup is initiated for the given cohort and results are populated
  // in PropertyPage if it is valid.
  // callback parameter can be set to NULL if cohort_list is empty.
  virtual void Get(const GoogleString& url,
                   const GoogleString& options_signature_hash,
                   const GoogleString& cache_key_suffix,
                   const PropertyCache::CohortVector& cohort_list,
                   PropertyPage* page,
                   BoolCallback* done,
                   AbstractPropertyStoreGetCallback** callback);

  // Write to cache.
  virtual void Put(const GoogleString& url,
                   const GoogleString& options_signature_hash,
                   const GoogleString& cache_key_suffix,
                   const PropertyCache::Cohort* cohort,
                   const PropertyCacheValues* values,
                   BoolCallback* done);

  // Establishes a Cohort backed by the CacheInteface passed to the constructor.
  void AddCohort(const GoogleString& cohort);
  // Establishes a Cohort to be backed by the specified CacheInterface.
  // Does not take the ownership of cache.
  void AddCohortWithCache(const GoogleString& cohort,
                          CacheInterface* cache);

  // Gets the underlying key associated with cache_key and a Cohort.
  // This is the key used for the CacheInterface provided to the
  // constructor.  This is made visible for testing, to make it
  // possible to inject delays into the cache via DelayCache::DelayKey.
  GoogleString CacheKey(const StringPiece& url,
                        const StringPiece& options_signature_hash,
                        const StringPiece& cache_key_suffix,
                        const PropertyCache::Cohort* cohort) const;

  // Returns default cache backend associated with CachePropertyStore.
  const CacheInterface* cache_backend() { return default_cache_; }

  virtual GoogleString Name() const;

  static GoogleString FormatName3(StringPiece cohort_name1,
                                  StringPiece cohort_cache1,
                                  StringPiece cohort_name2,
                                  StringPiece cohort_cache2,
                                  StringPiece cohort_name3,
                                  StringPiece cohort_cache3);

 private:
  GoogleString cache_key_prefix_;
  typedef std::map<GoogleString, CacheInterface*> CohortCacheMap;
  CohortCacheMap cohort_cache_map_;
  CacheInterface* default_cache_;
  Timer* timer_;
  Statistics* stats_;
  ThreadSystem* thread_system_;
  DISALLOW_COPY_AND_ASSIGN(CachePropertyStore);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_OPT_HTTP_CACHE_PROPERTY_STORE_H_
