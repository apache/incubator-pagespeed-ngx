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
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
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

// Create CriticalImagesInfo object from the value of property_value.  NULL if
// no value is found, or if the property value reflects that no results are
// available.  Result is owned by caller.
CriticalImagesInfo* CriticalImagesInfoFromPropertyValue(
    const PropertyValue* property_value) {
  DCHECK(property_value != NULL);
  CriticalImages crit_images;
  if (!PopulateCriticalImagesFromPropertyValue(property_value, &crit_images)) {
    return NULL;
  }
  // The existence of kEmptyValuePlaceholder should mean that "no data" will be
  // distinguished from "no critical images" by the above call.
  CriticalImagesInfo* critical_images_info = new CriticalImagesInfo();
  critical_images_info->html_critical_images.insert(
      crit_images.html_critical_images().begin(),
      crit_images.html_critical_images().end());
  critical_images_info->css_critical_images.insert(
      crit_images.css_critical_images().begin(),
      crit_images.css_critical_images().end());
  return critical_images_info;
}

void UpdateCriticalImagesSetInProto(
    const StringSet& html_critical_images_set,
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
    for (StringSet::const_iterator it = html_critical_images_set.begin();
         it != html_critical_images_set.end(); ++it) {
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

  for (StringSet::iterator i = html_critical_images_set.begin();
       i != html_critical_images_set.end(); ++i) {
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

const char CriticalImagesFinder::kRenderedImageDimensionsProperty[] =
    "rendered_image_dimensions";

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

namespace {

bool IsCriticalImage(const GoogleString& image_url,
                     const StringSet& critical_images_set) {
  return (critical_images_set.find(image_url) != critical_images_set.end());
}

}  // namespace

bool CriticalImagesFinder::IsHtmlCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  return IsCriticalImage(image_url, GetHtmlCriticalImages(driver));
}

bool CriticalImagesFinder::IsCssCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  return IsCriticalImage(image_url, GetCssCriticalImages(driver));
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
  // Don't update critical_images_info if it's already been set.
  if (driver->critical_images_info() != NULL) {
    return;
  }
  CriticalImagesInfo* info = NULL;
  const PropertyCache::Cohort* cohort = GetCriticalImagesCohort();
  // Fallback properties can be used for critical images.
  AbstractPropertyPage* page = driver->fallback_property_page();
  if (page != NULL && cohort != NULL) {
    PropertyValue* property_value = page->GetProperty(
        cohort, kCriticalImagesPropertyName);
    info = ExtractCriticalImagesFromCache(driver, property_value);
    if (info != NULL) {
      info->is_set_from_pcache = true;
      driver->log_record()->SetNumHtmlCriticalImages(
          info->html_critical_images.size());
      driver->log_record()->SetNumCssCriticalImages(
          info->css_critical_images.size());
    }
  }
  // Store an empty CriticalImagesInfo back into the driver if we don't have any
  // beacon results yet.
  if (info == NULL) {
    info = new CriticalImagesInfo;
  }
  driver->set_critical_images_info(info);
}

bool CriticalImagesFinder::UpdateCriticalImagesCacheEntryFromDriver(
    const StringSet* html_critical_images_set,
    const StringSet* css_critical_images_set,
    RewriteDriver* driver) {
  // Update property cache if above the fold critical images are successfully
  // determined.
  // Fallback properties will be updated for critical images.
  AbstractPropertyPage* page = driver->fallback_property_page();
  return UpdateCriticalImagesCacheEntry(
      html_critical_images_set, css_critical_images_set,
      NumSetsToKeep(), PercentSeenForCritical(),
      GetCriticalImagesCohort(), page);
}

bool CriticalImagesFinder::UpdateCriticalImagesCacheEntry(
    const StringSet* html_critical_images_set,
    const StringSet* css_critical_images_set,
    int num_sets_to_keep,
    int percent_seen_for_critical,
    const PropertyCache::Cohort* cohort,
    AbstractPropertyPage* page) {
  // Update property cache if above the fold critical images are successfully
  // determined.
  if (page == NULL) {
    return false;
  }
  if (cohort == NULL) {
    LOG(WARNING) << "Critical Images Cohort is NULL.";
    return false;
  }

  PropertyValue* property_value = page->GetProperty(
      cohort, kCriticalImagesPropertyName);
  // Read in the current critical images, and preserve the current HTML or
  // CSS critical images if they are not being updated.
  CriticalImages critical_images;
  PopulateCriticalImagesFromPropertyValue(property_value, &critical_images);
  if (!UpdateCriticalImages(
      html_critical_images_set, css_critical_images_set,
      num_sets_to_keep, percent_seen_for_critical,
      &critical_images)) {
    return false;
  }

  GoogleString buf;
  if (!critical_images.SerializeToString(&buf)) {
    LOG(WARNING) << "Serialization of critical images protobuf failed.";
    return false;
  }
  // The property cache won't store an empty value, which is what an
  // empty CriticalImages will serialize to. If buf is an empty string,
  // repalce with a placeholder that we can then handle when decoding
  // the property_cache value in
  // PopulateCriticalImagesFromPropertyValue.
  if (buf.empty()) {
    buf = kEmptyValuePlaceholder;
  }
  page->UpdateValue(cohort, kCriticalImagesPropertyName, buf);
  return true;
}

bool CriticalImagesFinder::UpdateCriticalImages(
    const StringSet* html_critical_images,
    const StringSet* css_critical_images,
    int num_sets_to_keep,
    int percent_seen_for_critical,
    CriticalImages* critical_images) {
  DCHECK(critical_images != NULL);
  if (html_critical_images != NULL) {
    UpdateCriticalImagesSetInProto(
        *html_critical_images,
        num_sets_to_keep,
        percent_seen_for_critical,
        critical_images->mutable_html_critical_images_sets(),
        critical_images->mutable_html_critical_images());
  }
  if (css_critical_images != NULL) {
    UpdateCriticalImagesSetInProto(
        *css_critical_images,
        num_sets_to_keep,
        percent_seen_for_critical,
        critical_images->mutable_css_critical_images_sets(),
        critical_images->mutable_css_critical_images());
  }
  // We updated if either StringSet* was set.
  return (html_critical_images != NULL || css_critical_images != NULL);
}

RenderedImages* CriticalImagesFinder::ExtractRenderedImageDimensionsFromCache(
    RewriteDriver* driver) {
  PropertyCacheDecodeResult pcache_status;
  scoped_ptr<RenderedImages> result(
      DecodeFromPropertyCache<RenderedImages>(
          driver,
          GetCriticalImagesCohort(),
          kRenderedImageDimensionsProperty,
          driver->options()->finder_properties_cache_expiration_time_ms(),
          &pcache_status));
  switch (pcache_status) {
    case kPropertyCacheDecodeNotFound:
      driver->message_handler()->Message(
          kInfo, "RenderedImage not found in cache");
      break;
    case kPropertyCacheDecodeExpired:
      driver->message_handler()->Message(
          kInfo, "RenderedImage cache entry expired");
      break;
    case kPropertyCacheDecodeParseError:
      driver->message_handler()->Message(
          kWarning, "Unable to parse Critical RenderedImage PropertyValue");
      break;
    case kPropertyCacheDecodeOk:
      break;
  }
  return result.release();
}

CriticalImagesInfo* CriticalImagesFinder::ExtractCriticalImagesFromCache(
    RewriteDriver* driver,
    const PropertyValue* property_value) {
  CriticalImagesInfo* critical_images_info = NULL;
  // Don't track stats if we are flushing early, since we will already be
  // counting this when we are rewriting the full page.
  bool track_stats = !driver->flushing_early();
  const PropertyCache* page_property_cache =
      driver->server_context()->page_property_cache();
  int64 cache_ttl_ms =
      driver->options()->finder_properties_cache_expiration_time_ms();
  // Check if the cache value exists and is not expired.
  if (property_value->has_value()) {
    const bool is_valid =
        !page_property_cache->IsExpired(property_value, cache_ttl_ms);
    if (is_valid) {
      critical_images_info =
          CriticalImagesInfoFromPropertyValue(property_value);
      if (track_stats) {
        if (critical_images_info == NULL) {
          critical_images_not_found_count_->Add(1);
        } else {
          critical_images_valid_count_->Add(1);
        }
      }
    } else if (track_stats) {
      critical_images_expired_count_->Add(1);
    }
  } else if (track_stats) {
    critical_images_not_found_count_->Add(1);
  }
  return critical_images_info;
}

bool CriticalImagesFinder::IsSetFromPcache(RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  return driver->critical_images_info()->is_set_from_pcache;
}

}  // namespace net_instaweb
