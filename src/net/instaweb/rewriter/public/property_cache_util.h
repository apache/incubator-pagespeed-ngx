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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This contains some utilities that make it easier to work
// with the property cache.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_PROPERTY_CACHE_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_PROPERTY_CACHE_UTIL_H_

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

enum PropertyCacheDecodeResult {
  kPropertyCacheDecodeNotFound,  // includes property cache not being enabled.
  kPropertyCacheDecodeExpired,
  kPropertyCacheDecodeParseError,
  kPropertyCacheDecodeOk
};

// Decodes a protobuf of type T from the property named 'property_name'
// in the cohort 'cohort_name' in the driver's property cache, and makes
// sure it has not exceeded its TTL of 'cache_ttl_ms'. Passing in 'cache_ttl_ms'
// of -1 will disable the check.
//
// *status will denote the decoding state; if it's kPropertyCacheDecodeOk
// then a pointer to a freshly allocated decoded proto is returned; otherwise
// NULL is returned.
template<typename T>
T* DecodeFromPropertyCache(RewriteDriver* driver,
                           StringPiece cohort_name,
                           StringPiece property_name,
                           int64 cache_ttl_ms,
                           PropertyCacheDecodeResult* status) {
  scoped_ptr<T> result;
  const PropertyCache* cache = driver->server_context()->page_property_cache();
  const PropertyCache::Cohort* cohort = cache->GetCohort(cohort_name);
  PropertyPage* page = driver->property_page();
  if (cohort == NULL || page == NULL) {
    *status = kPropertyCacheDecodeNotFound;
    return NULL;
  }

  PropertyValue* property_value = page->GetProperty(cohort, property_name);
  if (property_value == NULL || !property_value->has_value()) {
    *status = kPropertyCacheDecodeNotFound;
    return NULL;
  }

  if ((cache_ttl_ms != -1) && cache->IsExpired(property_value, cache_ttl_ms)) {
    *status = kPropertyCacheDecodeExpired;
    return NULL;
  }

  result.reset(new T);
  ArrayInputStream input(property_value->value().data(),
                         property_value->value().size());
  if (!result->ParseFromZeroCopyStream(&input)) {
    *status = kPropertyCacheDecodeParseError;
    return NULL;
  }

  *status = kPropertyCacheDecodeOk;
  return result.release();
}

enum PropertyCacheUpdateResult {
  kPropertyCacheUpdateNotFound,  // can't find existing value to update
  kPropertyCacheUpdateEncodeError,
  kPropertyCacheUpdateOk
};

// Updates the property 'property_name' in cohort 'cohort_name' of the property
// cache managed by the rewrite driver with the new value of the proto T.
// If 'write_cohort' is true, will also additionally write out the cohort
// to the cache backing.
template<typename T>
PropertyCacheUpdateResult UpdateInPropertyCache(const T& value,
                                                RewriteDriver* driver,
                                                StringPiece cohort_name,
                                                StringPiece property_name,
                                                bool write_cohort) {
  const PropertyCache* cache = driver->server_context()->page_property_cache();
  PropertyPage* page = driver->property_page();
  return UpdateInPropertyCache(
      value, cache, cohort_name, property_name, write_cohort, page);
}

template<typename T>
PropertyCacheUpdateResult UpdateInPropertyCache(const T& value,
                                                const PropertyCache* cache,
                                                StringPiece cohort_name,
                                                StringPiece property_name,
                                                bool write_cohort,
                                                PropertyPage* page) {
  const PropertyCache::Cohort* cohort = cache->GetCohort(cohort_name);
  if (cohort == NULL || page == NULL) {
    return kPropertyCacheUpdateNotFound;
  }

  PropertyValue* property_value = page->GetProperty(cohort, property_name);
  if (property_value == NULL) {
    return kPropertyCacheUpdateNotFound;
  }

  GoogleString buf;
  if (!value.SerializeToString(&buf)) {
    return kPropertyCacheUpdateEncodeError;
  }

  page->UpdateValue(cohort, property_name, buf);

  if (write_cohort) {
    page->WriteCohort(cohort);
  }

  return kPropertyCacheUpdateOk;
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_PROPERTY_CACHE_UTIL_H_
