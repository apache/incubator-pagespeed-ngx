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

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace {

const char kRenderedImageJsonWidthKey[] = "rw";
const char kRenderedImageJsonHeightKey[] = "rh";
const char kOriginalImageJsonWidthKey[] = "ow";
const char kOriginalImageJsonHeightKey[] = "oh";
const char kEmptyValuePlaceholder[] = "\n";

// Create CriticalImagesInfo object from the value of property_value.  NULL if
// no value is found, or if the property value reflects that no results are
// available.  Result is owned by caller.
CriticalImagesInfo* CriticalImagesInfoFromPropertyValue(
    int percent_seen_for_critical,
    const PropertyValue* property_value) {
  DCHECK(property_value != NULL);
  scoped_ptr<CriticalImagesInfo> info(new CriticalImagesInfo());
  if (!CriticalImagesFinder::PopulateCriticalImagesFromPropertyValue(
          property_value, &info->proto)) {
    return NULL;
  }
  // Fill in map fields based on proto value so that image lookups are O(lg n).
  GetCriticalKeysFromProto(percent_seen_for_critical,
                           info->proto.html_critical_image_support(),
                           &info->html_critical_images);
  GetCriticalKeysFromProto(percent_seen_for_critical,
                           info->proto.css_critical_image_support(),
                           &info->css_critical_images);
  return info.release();
}

// Setup a map for RenderedImages and their dimensions.
void SetupRenderedImageDimensionsMap(
    const RenderedImages& rendered_images,
    RenderedImageDimensionsMap* map) {
  for (int i = 0; i < rendered_images.image_size(); ++i) {
    const RenderedImages_Image& images = rendered_images.image(i);
    // In case of beacons returning these rendered dimensions, images.src()
    // will be a hash of the image url. Hence when we do a lookup in
    // rendered_images_map we need to hash the url.
    (*map)[images.src()] = std::make_pair(
        images.rendered_width(), images.rendered_height());
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

CriticalImagesFinder::CriticalImagesFinder(const PropertyCache::Cohort* cohort,
                                           Statistics* statistics)
    : cohort_(cohort) {
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
  return IsCriticalImage(GetKeyForUrl(image_url),
                         GetHtmlCriticalImages(driver));
}

bool CriticalImagesFinder::IsCssCriticalImage(
    const GoogleString& image_url, RewriteDriver* driver) {
  return IsCriticalImage(GetKeyForUrl(image_url),
                         GetCssCriticalImages(driver));
}

bool CriticalImagesFinder::GetRenderedImageDimensions(
    RewriteDriver* driver,
    const GoogleUrl& image_src_gurl,
    std::pair<int32, int32>* dimensions) {
  UpdateCriticalImagesSetInDriver(driver);
  const CriticalImagesInfo* info = driver->critical_images_info();
  CHECK(info != NULL);
  RenderedImageDimensionsMap::const_iterator iterator =
      info->rendered_images_map.find(
          GetKeyForUrl(image_src_gurl.spec_c_str()));
  if (iterator != info->rendered_images_map.end()) {
    *dimensions = iterator->second;
    return true;
  }
  return false;
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
  // Fallback properties can be used for critical images.
  AbstractPropertyPage* page = driver->fallback_property_page();
  if (page != NULL && cohort() != NULL) {
    PropertyValue* property_value = page->GetProperty(
        cohort(), kCriticalImagesPropertyName);
    info = ExtractCriticalImagesFromCache(driver, property_value);
    if (info != NULL) {
      info->is_critical_image_info_present = true;
      if (driver->request_context().get() != NULL) {
        driver->log_record()->SetNumHtmlCriticalImages(
            info->html_critical_images.size());
        driver->log_record()->SetNumCssCriticalImages(
            info->css_critical_images.size());
      }
    }
  }

  // Store an empty CriticalImagesInfo back into the driver if we don't have any
  // beacon results yet.
  if (info == NULL) {
    info = new CriticalImagesInfo;
  }

  if (driver->options()->Enabled(
      RewriteOptions::kResizeToRenderedImageDimensions)) {
    scoped_ptr<RenderedImages> rendered_images(
        ExtractRenderedImageDimensionsFromCache(driver));
    if (rendered_images != NULL) {
      SetupRenderedImageDimensionsMap(*rendered_images,
                                      &info->rendered_images_map);
    }
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
      NULL /* RenderedImages Proto */,
      SupportInterval(), cohort(), page);
}

// Setup the HTML and CSS critical image sets in *critical_images using
// *property_value.  Return true if property_value had a value, and
// deserialization of it succeeded.
bool CriticalImagesFinder::PopulateCriticalImagesFromPropertyValue(
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
  // Having dealt with the unusual cases, parse the proto.
  ArrayInputStream input(property_value->value().data(),
                         property_value->value().size());
  return critical_images->ParseFromZeroCopyStream(&input);
}

bool CriticalImagesFinder::UpdateCriticalImagesCacheEntry(
    const StringSet* html_critical_images_set,
    const StringSet* css_critical_images_set,
    const RenderedImages* rendered_images_set,
    int support_interval,
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
  return UpdateAndWriteBackCriticalImagesCacheEntry(
      html_critical_images_set, css_critical_images_set, rendered_images_set,
      support_interval, cohort, page, &critical_images);
}

bool CriticalImagesFinder::UpdateAndWriteBackCriticalImagesCacheEntry(
    const StringSet* html_critical_images_set,
    const StringSet* css_critical_images_set,
    const RenderedImages* rendered_images_set,
    int support_interval,
    const PropertyCache::Cohort* cohort,
    AbstractPropertyPage* page,
    CriticalImages* critical_images) {
  // Update RenderedImages proto in property Cache.
  if (rendered_images_set != NULL) {
    UpdateInPropertyCache(
        *rendered_images_set, cohort, kRenderedImageDimensionsProperty,
        false /* don't write cohort */, page);
  }
  if (!UpdateCriticalImages(
      html_critical_images_set, css_critical_images_set,
      support_interval, critical_images)) {
    return false;
  }

  GoogleString buf;
  if (!critical_images->SerializeToString(&buf)) {
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
    int support_interval,
    CriticalImages* critical_images) {
  DCHECK(critical_images != NULL);
  if (html_critical_images != NULL) {
    UpdateCriticalKeys(
        false /* require_prior_support */,
        *html_critical_images,
        support_interval,
        critical_images->mutable_html_critical_image_support());
  }
  if (css_critical_images != NULL) {
    UpdateCriticalKeys(
        false /* require_prior_support */,
        *css_critical_images,
        support_interval,
        critical_images->mutable_css_critical_image_support());
  }
  // We updated if either StringSet* was set.
  return (html_critical_images != NULL || css_critical_images != NULL);
}

RenderedImages* CriticalImagesFinder::ExtractRenderedImageDimensionsFromCache(
    RewriteDriver* driver) {
  PropertyCacheDecodeResult pcache_status;
  scoped_ptr<RenderedImages> dimensions(
      DecodeFromPropertyCache<RenderedImages>(
          driver,
          cohort(),
          kRenderedImageDimensionsProperty,
          driver->options()->finder_properties_cache_expiration_time_ms(),
          &pcache_status));
  if (pcache_status == kPropertyCacheDecodeParseError) {
    driver->message_handler()->Message(
        kWarning, "Unable to parse Critical RenderedImage PropertyValue for %s",
        driver->url());
  }
  return dimensions.release();
}

RenderedImages* CriticalImagesFinder::JsonMapToRenderedImagesMap(
    const GoogleString& str,
    const RewriteOptions* options) {
  Json::Reader reader;
  Json::Value json_rendered_image_map;
  if (!reader.parse(str, json_rendered_image_map)) {
    LOG(WARNING) << "Unable to parse Json data for rendered images";
    return NULL;
  }
  // Parse json data into a map.
  if (json_rendered_image_map.isNull() || !json_rendered_image_map.isObject()) {
    LOG(WARNING) << "Bad Json rendered image dimensions map";
    return NULL;
  }
  // Put the extracted map into RenderedImages proto data.
  RenderedImages* rendered_images = new RenderedImages();
  Json::Value::Members imgs = json_rendered_image_map.getMemberNames();
  for (int i = 0, n = imgs.size(); i < n; ++i) {
    const GoogleString& img_src = imgs[i];
    int original_width = json_rendered_image_map[img_src].get(
        kOriginalImageJsonWidthKey, 0).asInt();
    int original_height = json_rendered_image_map[img_src].get(
        kOriginalImageJsonHeightKey, 0).asInt();
    int rendered_width = json_rendered_image_map[img_src].get(
        kRenderedImageJsonWidthKey, 0).asInt();
    int rendered_height = json_rendered_image_map[img_src].get(
        kRenderedImageJsonHeightKey, 0).asInt();
    int original_area = (original_width * original_height);
    int rendered_area = (rendered_width * rendered_height);
    // Store renderedWidth and renderedHeight for the image only if
    // the rendered sizes are lower than the original sizes by at least the
    // percentage threshold set.
    if (100 * rendered_area < original_area *
        options->image_limit_rendered_area_percent()) {
      RenderedImages_Image* images = rendered_images->add_image();
      images->set_src(img_src);
      images->set_rendered_width(rendered_width);
      images->set_rendered_height(rendered_height);
    }
  }
  return rendered_images;
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
          CriticalImagesInfoFromPropertyValue(PercentSeenForCritical(),
                                              property_value);
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

CriticalImagesFinder::Availability CriticalImagesFinder::Available(
    RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  CriticalImagesInfo* info = driver->critical_images_info();
  if (info != NULL && info->is_critical_image_info_present &&
      info->proto.has_html_critical_image_support() &&
      IsBeaconDataAvailable(info->proto.html_critical_image_support())) {
    return kAvailable;
  } else {
    return kNoDataYet;
  }
}

bool CriticalImagesFinder::IsCriticalImageInfoPresent(RewriteDriver* driver) {
  UpdateCriticalImagesSetInDriver(driver);
  return driver->critical_images_info()->is_critical_image_info_present;
}

void CriticalImagesFinder::AddHtmlCriticalImage(
    const GoogleString& url,
    RewriteDriver* driver) {
  mutable_html_critical_images(driver)->insert(GetKeyForUrl(url));
}

}  // namespace net_instaweb
