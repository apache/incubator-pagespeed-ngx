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
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

enum PropertyCacheDecodeResult {
  kPropertyCacheDecodeNotFound,  // includes property cache not being enabled.
  kPropertyCacheDecodeExpired,
  kPropertyCacheDecodeParseError,
  kPropertyCacheDecodeOk
};

// Returns PropertyValue object for given cohort and property name,
// setting *status and returning NULL if any errors were found.
const PropertyValue* DecodeFromPropertyCacheHelper(
    const PropertyCache* cache,
    AbstractPropertyPage* page,
    const PropertyCache::Cohort* cohort,
    StringPiece property_name,
    int64 cache_ttl_ms,
    PropertyCacheDecodeResult* status);

// Decodes a protobuf of type T from the property named 'property_name' in the
// cohort 'cohort_name' in the given property cache, and makes sure it has not
// exceeded its TTL of 'cache_ttl_ms' (a value of -1 will disable this check).
//
// *status will denote the decoding state; if it's kPropertyCacheDecodeOk
// then a pointer to a freshly allocated decoded proto is returned; otherwise
// NULL is returned.
template<typename T>
T* DecodeFromPropertyCache(const PropertyCache* cache,
                           AbstractPropertyPage* page,
                           const PropertyCache::Cohort* cohort,
                           StringPiece property_name,
                           int64 cache_ttl_ms,
                           PropertyCacheDecodeResult* status) {
  const PropertyValue* property_value =
      DecodeFromPropertyCacheHelper(cache, page, cohort, property_name,
                                    cache_ttl_ms, status);
  if (property_value == NULL) {
    // *status set by helper
    return NULL;
  }

  scoped_ptr<T> result(new T);
  ArrayInputStream input(property_value->value().data(),
                         property_value->value().size());
  if (!result->ParseFromZeroCopyStream(&input)) {
    *status = kPropertyCacheDecodeParseError;
    return NULL;
  }

  *status = kPropertyCacheDecodeOk;
  return result.release();
}

// Wrapper version of the above function that gets the property cache and the
// property page from the given driver.
template<typename T>
T* DecodeFromPropertyCache(RewriteDriver* driver,
                           const PropertyCache::Cohort* cohort,
                           StringPiece property_name,
                           int64 cache_ttl_ms,
                           PropertyCacheDecodeResult* status) {
  return DecodeFromPropertyCache<T>(
      driver->server_context()->page_property_cache(),
      driver->property_page(),
      cohort,
      property_name,
      cache_ttl_ms,
      status);
}

enum PropertyCacheUpdateResult {
  kPropertyCacheUpdateNotFound,  // can't find existing value to update
  kPropertyCacheUpdateEncodeError,
  kPropertyCacheUpdateOk
};

PropertyCacheUpdateResult UpdateInPropertyCache(
    const protobuf::MessageLite& value,
    const PropertyCache::Cohort* cohort,
    StringPiece property_name,
    bool write_cohort,
    AbstractPropertyPage* page);

// Updates the property 'property_name' in cohort 'cohort_name' of the property
// cache managed by the rewrite driver with the new value of the proto T.
// If 'write_cohort' is true, will also additionally write out the cohort
// to the cache backing.
inline PropertyCacheUpdateResult UpdateInPropertyCache(
    const protobuf::MessageLite& value, RewriteDriver* driver,
    const PropertyCache::Cohort* cohort, StringPiece property_name,
    bool write_cohort) {
  AbstractPropertyPage* page = driver->property_page();
  return UpdateInPropertyCache(
      value, cohort, property_name, write_cohort, page);
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_PROPERTY_CACHE_UTIL_H_
