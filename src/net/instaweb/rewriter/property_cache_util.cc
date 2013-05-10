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

// Author: jmaessen@google.com (Jan-Willem Maessen)
// Helper functions for the templated functionality in property_cache_util.h

#include "net/instaweb/rewriter/public/property_cache_util.h"

#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"

namespace net_instaweb {

const PropertyValue* DecodeFromPropertyCacheHelper(
    const PropertyCache* cache,
    AbstractPropertyPage* page,
    const PropertyCache::Cohort* cohort,
    StringPiece property_name,
    int64 cache_ttl_ms,
    PropertyCacheDecodeResult* status) {
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
  return property_value;
}

PropertyCacheUpdateResult UpdateInPropertyCache(
    const protobuf::MessageLite& value,
    const PropertyCache::Cohort* cohort,
    StringPiece property_name,
    bool write_cohort,
    AbstractPropertyPage* page) {
  if (cohort == NULL || page == NULL) {
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
