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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CriticalImages;
class RenderedImages;
class RewriteDriver;
class Statistics;
class Variable;


// The instantiated CriticalImagesFinder is held by ServerContext, meaning
// there is only 1 per server. CriticalImagesInfo stores all of the request
// specific data needed by CriticalImagesFinder, and is held by the
// RewriteDriver.
// TODO(jud): Instead of a separate CriticalImagesInfo that gets populated from
// the CriticalImages protobuf value, we could just store the protobuf value in
// RewriteDriver and eliminate CriticalImagesInfo. Revisit this when updating
// this class to support multiple beacon response.
struct CriticalImagesInfo {
  CriticalImagesInfo() : is_set_from_pcache(false) {}
  StringSet html_critical_images;
  StringSet css_critical_images;
  bool is_set_from_pcache;
};


// Finds critical images i.e. images which are above the fold for a given url.
// This information may be used by DelayImagesFilter.
class CriticalImagesFinder {
 public:
  static const char kCriticalImagesValidCount[];
  static const char kCriticalImagesExpiredCount[];
  static const char kCriticalImagesNotFoundCount[];
  static const char kCriticalImagesPropertyName[];
  // Property name for the rendered image dimensions retreived from webkit
  // render response for the page.
  static const char kRenderedImageDimensionsProperty[];

  explicit CriticalImagesFinder(Statistics* stats);
  virtual ~CriticalImagesFinder();

  static void InitStats(Statistics* statistics);

  // Checks whether IsHtmlCriticalImage will return meaningful results about
  // critical images. Users of IsHtmlCriticalImage should check this function
  // and supply a default behavior if IsMeaningful returns false.
  virtual bool IsMeaningful(const RewriteDriver* driver) const = 0;

  // In order to handle varying critical image sets returned by the beacon, we
  // store a history of the last N critical images, and only declare an image
  // critical if it appears critical in the last M out of N sets reported. This
  // function returns what percentage of the sets need to include the image for
  // it be considered critical.
  virtual int PercentSeenForCritical() const {
    return kDefaultPercentSeenForCritial;
  }

  // The number of past critical image sets to keep. By default, we only keep
  // the most recent one. The beacon critical image finder should override this
  // to store a larger number of sets.
  virtual int NumSetsToKeep() const {
    return kDefaultNumSetsToKeep;
  }

  // Checks whether the requested image is present in the critical set or not.
  // Users of this function should also check IsMeaningful() to see if the
  // implementation of this function returns meaningful results and provide a
  // default behavior if it does not.  If no critical set value has been
  // obtained, returns false (not critical).
  virtual bool IsHtmlCriticalImage(const GoogleString& image_url,
                                   RewriteDriver* driver);

  virtual bool IsCssCriticalImage(const GoogleString& image_url,
                                  RewriteDriver* driver);

  // Get the critical image sets. Returns an empty set if there is no critical
  // image information.
  const StringSet& GetHtmlCriticalImages(RewriteDriver* driver);
  const StringSet& GetCssCriticalImages(RewriteDriver* driver);

  // Utility functions for manually setting the critical image sets. These
  // should only be used by unit tests that need to setup a specific set of
  // critical images. For normal users of CriticalImagesFinder, the critical
  // images will be populated from entries in the property cache.  Note that
  // these always return a non-NULL StringSet value (implying "beacon result
  // received").
  StringSet* mutable_html_critical_images(RewriteDriver* driver);
  StringSet* mutable_css_critical_images(RewriteDriver* driver);

  // Compute the critical images for the driver's url.
  virtual void ComputeCriticalImages(RewriteDriver* driver) = 0;

  // Identifies which cohort in the PropertyCache the critical image information
  // is located in.
  virtual const PropertyCache::Cohort* GetCriticalImagesCohort() const = 0;

  // Updates the critical images property cache entry. Returns whether the
  // update succeeded or not. Note that this base implementation does not call
  // WriteCohort. This should be called in the subclass if the cohort is not
  // written elsewhere. NULL is permitted for the critical image sets if only
  // one of the html or css sets is being updated, but not the other.
  bool UpdateCriticalImagesCacheEntryFromDriver(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      RewriteDriver* driver);

  // Alternative interface to update the critical images cache entry. This is
  // useful in contexts like the beacon handler where the RewriteDriver for the
  // original request no longer exists.
  static bool UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      int num_sets_to_keep,
      int percent_seen_for_critical,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page);

  // Returns true if the critical images have been extracted from pcache,
  // false otherwise. This is virtual only to be overridden in tests.
  virtual bool IsSetFromPcache(RewriteDriver* driver);

  // Extracts rendered dimensions from property cache.
  virtual RenderedImages* ExtractRenderedImageDimensionsFromCache(
      RewriteDriver* driver);

 protected:
  // Gets critical images if present in the property cache and updates the
  // critical_images set in RewriteDriver with the obtained set.  If you
  // override this method, driver->critical_images_info() must not return NULL
  // after this function has been called.
  virtual void UpdateCriticalImagesSetInDriver(RewriteDriver* driver);

  // Extracts the critical images from the given property_value into
  // critical_images_info, after checking if the property value is still valid
  // using the provided TTL.  It also updates stats variables.
  CriticalImagesInfo* ExtractCriticalImagesFromCache(
      RewriteDriver* driver,
      const PropertyValue* property_value);

 private:
  friend class CriticalImagesFinderTestBase;

  // Update a CriticalImages protobuf value with new critical image sets. If
  // either of the html or css sets are NULL, those fields in critical_images
  // will not be updated. Returns true if either of the critical image sets in
  // critical_images was updated.
  static bool UpdateCriticalImages(const StringSet* html_critical_images,
                                   const StringSet* css_critical_images,
                                   int num_sets_to_keep,
                                   int percent_seen_for_critical,
                                   CriticalImages* critical_images);

  // By default, store 1 critical image set and require an image to be in that
  // set for it to be critical.
  static const int kDefaultPercentSeenForCritial = 100;
  static const int kDefaultNumSetsToKeep = 1;

  Variable* critical_images_valid_count_;
  Variable* critical_images_expired_count_;
  Variable* critical_images_not_found_count_;

  DISALLOW_COPY_AND_ASSIGN(CriticalImagesFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_H_
