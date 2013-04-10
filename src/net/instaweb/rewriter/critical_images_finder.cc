/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/rewriter/public/critical_images_finder.h"

#include <map>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kEmptyValuePlaceholder[] = "\n";

// Setup the HTML and CSS critical image sets in critical_images_info from the
// property_value. Return true if property_value had a value, and
// deserialization of it succeeded.
bool PopulateCriticalImagesFromPropertyValue(
    const PropertyValue* property_value,
    CriticalImages* critical_images) {
  DCHECK(property_value != NULL);
  DCHECK(critical_images != NULL);
  if (!property_value->has_value()) {
    return false;
  }

  // Check if we have the placeholder string value, indicating an empty value.
  // This will be stored when we have an empty set of critical images, since the
  // property cache doesn't store empty values.
  if (property_value->value() == kEmptyValuePlaceholder) {
    critical_images->Clear();
    return true;
  }

  ArrayInputStream input(property_value->value().data(),
                         property_value->value().size());
  return critical_images->ParseFromZeroCopyStream(&input);
}

// Load the value of property_value into the StringSets of critical_images_info.
bool PopulateCriticalImagesInfoFromPropertyValue(
    const PropertyValue* property_value,
    CriticalImagesInfo* critical_images_info) {
  DCHECK(property_value != NULL);
  DCHECK(critical_images_info != NULL);
  CriticalImages crit_images;
  if (PopulateCriticalImagesFromPropertyValue(property_value, &crit_images)) {
    critical_images_info->html_critical_images.clear();
    critical_images_info->html_critical_images.insert(
        crit_images.html_critical_images().begin(),
        crit_images.html_critical_images().end());
    critical_images_info->css_critical_images.clear();
    critical_images_info->css_critical_images.insert(
        crit_images.css_critical_images().begin(),
        crit_images.css_critical_images().end());
    return true;
  }
  return false;
}

void UpdateCriticalImagesSetInProto(
    const StringSet& critical_images_set,
    int max_set_size,
    int percent_needed_for_critical,
    protobuf::RepeatedPtrField<CriticalImages::CriticalImageSet>* set_field,
    protobuf::RepeatedPtrField<GoogleString>* critical_images_field) {
  DCHECK_GT(max_set_size, 0);

  // If we have a max_set_size of 1 we can just directly set the
  // critical_images_field to the new set, we don't need to store any history of
  // responses.
  if (max_set_size == 1) {
    critical_images_field->Clear();
    for (StringSet::const_iterator it = critical_images_set.begin();
         it != critical_images_set.end(); ++it) {
      *critical_images_field->Add() = *it;
    }
    return;
  }

  // Update the set field first, which contains the history of up to
  // max_set_size critical image responses. If we already have max_set_size,
  // drop the first response, and append the new response to the end.
  CriticalImages::CriticalImageSet* new_set;
  if (set_field->size() >= max_set_size) {
    DCHECK_EQ(set_field->size(), max_set_size);
    for (int i = 1; i < set_field->size(); ++i) {
      set_field->SwapElements(i - 1, i);
    }
    new_set = set_field->Mutable(set_field->size() - 1);
    new_set->Clear();
  } else {
    new_set = set_field->Add();
  }

  for (StringSet::iterator i = critical_images_set.begin();
       i != critical_images_set.end(); ++i) {
    new_set->add_critical_images(*i);
  }

  // Now recalculate the critical image set.
  std::map<GoogleString, int> image_count;
  for (int i = 0; i < set_field->size(); ++i) {
    const CriticalImages::CriticalImageSet& set = set_field->Get(i);
    for (int j = 0; j < set.critical_images_size(); ++j) {
      const GoogleString& img = set.critical_images(j);
      image_count[img]++;
    }
  }

  int num_needed_for_critical =
      set_field->size() * percent_needed_for_critical / 100;
  critical_images_field->Clear();
  for (std::map<GoogleString, int>::const_iterator it = image_count.begin();
      it != image_count.end(); ++it) {
    if (it->second >= num_needed_for_critical) {
      *critical_images_field->Add() = it->first;
    }
  }
}

}  // namespace

const char CriticalImagesFinder::kCriticalImagesPropertyName[] =
    "critical_images";

const char CriticalImagesFinder::kCriticalImagesValidCount[] =
    "critical_images_valid_count";

const char CriticalImagesFinder::kCriticalImagesExpiredCount[] =
    "critical_images_expired_count";

const char CriticalImagesFinder::kCriticalImagesNotFoundCount[] =
    "critical_images_not_found_count";

CriticalImagesFinder::CriticalImagesFinder(Statistics* statistics) {
  critical_images_valid_count_ = statistics->GetVariable(
      kCriticalImagesValidCount);
  critical_images_expired_count_ = statistics->GetVariable(
      kCriticalImagesExpiredCount);
  critical_images_not_found_count_ = statistics->GetVariable(
      kCriticalImagesNotFoundCount);
}

CriticalImagesFinder::~CriticalImagesFinder() {
}

void CriticalImagesFinder::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCriticalImagesValidCount);
  statistics->AddVariable(kCriticalImagesExpiredCount);
  statistics->AddVariable(kCriticalImagesNotFoundCount);
}

bool CriticalImagesFinder::IsHtmlCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  const StringSet& critical_images_set = GetHtmlCriticalImages(driver);
  return (critical_images_set.find(image_url) != critical_images_set.end());
}

bool CriticalImagesFinder::IsCssCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  const StringSet& critical_images_set = GetCssCriticalImages(driver);
  return (critical_images_set.find(image_url) != critical_images_set.end());
}

const StringSet& CriticalImagesFinder::GetHtmlCriticalImages(
    RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  const CriticalImagesInfo* info = driver->critical_images_info();
  CHECK(info != NULL);

  return info->html_critical_images;
}

const StringSet& CriticalImagesFinder::GetCssCriticalImages(
    RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  const CriticalImagesInfo* info = driver->critical_images_info();
  CHECK(info != NULL);

  return info->css_critical_images;
}

StringSet* CriticalImagesFinder::mutable_html_critical_images(
    RewriteDriver* driver) {
  DCHECK(driver != NULL);
  CriticalImagesInfo* driver_info = driver->critical_images_info();
  // Preserve CSS critical images if they have been updated already.
  if (driver_info == NULL) {
    driver_info = new CriticalImagesInfo;
    driver->set_critical_images_info(driver_info);
  }
  return &driver_info->html_critical_images;
}

StringSet* CriticalImagesFinder::mutable_css_critical_images(
    RewriteDriver* driver) {
  DCHECK(driver != NULL);
  CriticalImagesInfo* driver_info = driver->critical_images_info();
  // Preserve CSS critical images if they have been updated already.
  if (driver_info == NULL) {
    driver_info = new CriticalImagesInfo;
    driver->set_critical_images_info(driver_info);
  }
  return &driver_info->css_critical_images;
}


// Copy the critical images for this request from the property cache into the
// RewriteDriver. The critical images are not stored in CriticalImageFinder
// because the ServerContext holds the CriticalImageFinder and hence is shared
// between requests.
void CriticalImagesFinder::UpdateCriticalImagesSetInDriver(
    RewriteDriver* driver) {
  const CriticalImagesInfo* driver_info = driver->critical_images_info();
  // If driver_info is not NULL, then the CriticalImagesInfo has already been
  // updated, so no need to do anything here.
  if (driver_info != NULL) {
    return;
  }
  scoped_ptr<CriticalImagesInfo> info(new CriticalImagesInfo);
  PropertyCache* page_property_cache =
      driver->server_context()->page_property_cache();
  const PropertyCache::Cohort* cohort =
      page_property_cache->GetCohort(GetCriticalImagesCohort());
  // Fallback properties can be used for critical images.
  AbstractPropertyPage* page = driver->fallback_property_page();
  if (page != NULL && cohort != NULL) {
    PropertyValue* property_value = page->GetProperty(
        cohort, kCriticalImagesPropertyName);
    ExtractCriticalImagesFromCache(driver, property_value, true, info.get());
    driver->log_record()->SetNumHtmlCriticalImages(
        info->html_critical_images.size());
    driver->log_record()->SetNumCssCriticalImages(
        info->css_critical_images.size());
  }
  driver->set_critical_images_info(info.release());
}

// TODO(pulkitg): Change all instances of critical_images_set to
// html_critical_images_set.
bool CriticalImagesFinder::UpdateCriticalImagesCacheEntryFromDriver(
    RewriteDriver* driver, StringSet* critical_images_set,
    StringSet* css_critical_images_set) {
  // Update property cache if above the fold critical images are successfully
  // determined.
  // Fallback properties will be updated for critical images.
  AbstractPropertyPage* page = driver->fallback_property_page();
  PropertyCache* page_property_cache =
      driver->server_context()->page_property_cache();
  return UpdateCriticalImagesCacheEntry(
      page, page_property_cache, critical_images_set, css_critical_images_set);
}

bool CriticalImagesFinder::UpdateCriticalImagesCacheEntry(
    AbstractPropertyPage* page, PropertyCache* page_property_cache,
    StringSet* html_critical_images_set, StringSet* css_critical_images_set) {
  // Update property cache if above the fold critical images are successfully
  // determined.
  scoped_ptr<StringSet> html_critical_images(html_critical_images_set);
  scoped_ptr<StringSet> css_critical_images(css_critical_images_set);
  if (page_property_cache != NULL && page != NULL) {
    const PropertyCache::Cohort* cohort =
        page_property_cache->GetCohort(GetCriticalImagesCohort());
    if (cohort != NULL) {
      PropertyValue* property_value = page->GetProperty(
          cohort, kCriticalImagesPropertyName);
      // Read in the current critical images, and preserve the current HTML or
      // CSS critical images if they are not being updated.
      CriticalImages critical_images;
      PopulateCriticalImagesFromPropertyValue(property_value, &critical_images);
      bool updated = UpdateCriticalImages(
          html_critical_images.get(), css_critical_images.get(),
          &critical_images);
      if (updated) {
        GoogleString buf;
        if (critical_images.SerializeToString(&buf)) {
          // The property cache won't store an empty value, which is what an
          // empty CriticalImages will serialize to. If buf is an empty string,
          // repalce with a placeholder that we can then handle when decoding
          // the property_cache value in
          // PopulateCriticalImagesFromPropertyValue.
          if (buf.empty()) {
            buf = kEmptyValuePlaceholder;
          }
          page->UpdateValue(cohort, kCriticalImagesPropertyName, buf);
        } else {
          LOG(WARNING) << "Serialization of critical images protobuf failed.";
          return false;
        }
      }
      return updated;
    } else {
      LOG(WARNING) << "Critical Images Cohort is NULL.";
    }
  }
  return false;
}

bool CriticalImagesFinder::UpdateCriticalImages(
    const StringSet* html_critical_images,
    const StringSet* css_critical_images,
    CriticalImages* critical_images) const {
  DCHECK(critical_images != NULL);
  if (html_critical_images != NULL) {
    UpdateCriticalImagesSetInProto(
        *html_critical_images,
        NumSetsToKeep(),
        PercentSeenForCritical(),
        critical_images->mutable_html_critical_images_sets(),
        critical_images->mutable_html_critical_images());
  }
  if (css_critical_images != NULL) {
    UpdateCriticalImagesSetInProto(
        *css_critical_images,
        NumSetsToKeep(),
        PercentSeenForCritical(),
        critical_images->mutable_css_critical_images_sets(),
        critical_images->mutable_css_critical_images());
  }
  // We updated if either StringSet* was set.
  return (html_critical_images != NULL || css_critical_images != NULL);
}

void CriticalImagesFinder::ExtractCriticalImagesFromCache(
    RewriteDriver* driver,
    const PropertyValue* property_value,
    bool track_stats,
    CriticalImagesInfo* critical_images_info) {
  DCHECK(critical_images_info != NULL);
  // Don't track stats if we are flushing early, since we will already be
  // counting this when we are rewriting the full page.
  track_stats &= !driver->flushing_early();
  const PropertyCache* page_property_cache =
      driver->server_context()->page_property_cache();
  int64 cache_ttl_ms =
      driver->options()->finder_properties_cache_expiration_time_ms();
  // Check if the cache value exists and is not expired.
  if (property_value->has_value()) {
    const bool is_valid =
        !page_property_cache->IsExpired(property_value, cache_ttl_ms);
    if (is_valid) {
      PopulateCriticalImagesInfoFromPropertyValue(property_value,
                                                  critical_images_info);
      if (track_stats) {
        critical_images_valid_count_->Add(1);
      }
      return;
    } else if (track_stats) {
      critical_images_expired_count_->Add(1);
    }
  } else if (track_stats) {
    critical_images_not_found_count_->Add(1);
  }
}

}  // namespace net_instaweb
