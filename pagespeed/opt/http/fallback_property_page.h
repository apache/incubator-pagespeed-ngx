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
// This class holds property page with fallback values (i.e. without query
// params) which can be used to retrieve and update the property values in case
// if we fail to find it in actual property page. Values retrieved from property
// page without query params are named as fallback values.

#ifndef PAGESPEED_OPT_HTTP_FALLBACK_PROPERTY_PAGE_H_
#define PAGESPEED_OPT_HTTP_FALLBACK_PROPERTY_PAGE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/opt/http/property_cache.h"

namespace net_instaweb {

class GoogleUrl;

class FallbackPropertyPage : public AbstractPropertyPage {
 public:
  // FallbackPropertyPage takes the ownership of both the property pages.
  FallbackPropertyPage(PropertyPage* actual_property_page,
                       PropertyPage* property_page_with_fallback_values);
  virtual ~FallbackPropertyPage();

  // Gets a property given the property name. It returns the property
  // from fallback property cache if actual property page has no value.
  virtual PropertyValue* GetProperty(
      const PropertyCache::Cohort* cohort,
      const StringPiece& property_name);

  // Gets the property from property page with fallback values. It can return
  // NULL if property page with fallback values is NULL.
  PropertyValue* GetFallbackProperty(
        const PropertyCache::Cohort* cohort,
        const StringPiece& property_name);

  // Updates the value of a property for both actual property page and fallback
  // property page.
  virtual void UpdateValue(
      const PropertyCache::Cohort* cohort, const StringPiece& property_name,
      const StringPiece& value);

  // Updates a Cohort of properties into the cache. It will also update for
  // fallback property cache.
  virtual void WriteCohort(const PropertyCache::Cohort* cohort);

  // Gets the cache state for the actual property page.
  virtual CacheInterface::KeyState GetCacheState(
      const PropertyCache::Cohort* cohort);

  // Gets the cache state of the property page with fallback values.
  virtual CacheInterface::KeyState GetFallbackCacheState(
        const PropertyCache::Cohort* cohort);

  // Deletes a property given the property name from both the pages.
  virtual void DeleteProperty(const PropertyCache::Cohort* cohort,
                              const StringPiece& property_name);

  PropertyPage* actual_property_page() { return actual_property_page_.get(); }
  PropertyPage* property_page_with_fallback_values() {
    return property_page_with_fallback_values_.get();
  }

  // Returns the page property cache url for the page containing fallback
  // values (i.e. without query params or without leaf).
  static GoogleString GetFallbackPageUrl(const GoogleUrl& request_url);

  // Returns true if given url is for fallback properties.
  static bool IsFallbackUrl(const GoogleString& url);

 private:
  scoped_ptr<PropertyPage> actual_property_page_;
  scoped_ptr<PropertyPage> property_page_with_fallback_values_;
  DISALLOW_COPY_AND_ASSIGN(FallbackPropertyPage);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_OPT_HTTP_FALLBACK_PROPERTY_PAGE_H_
