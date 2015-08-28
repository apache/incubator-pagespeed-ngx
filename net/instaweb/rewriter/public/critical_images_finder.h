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

#include <map>
#include <utility>

#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class RenderedImages;
class RewriteDriver;
class RewriteOptions;
class Statistics;
class Variable;

typedef std::map<GoogleString, std::pair<int32, int32> >
    RenderedImageDimensionsMap;

// The instantiated CriticalImagesFinder is held by ServerContext, meaning
// there is only 1 per server. CriticalImagesInfo stores all of the request
// specific data needed by CriticalImagesFinder, and is held by the
// RewriteDriver.
// TODO(jud): Instead of a separate CriticalImagesInfo that gets populated from
// the CriticalImages protobuf value, we could just store the protobuf value in
// RewriteDriver and eliminate CriticalImagesInfo. Revisit this when updating
// this class to support multiple beacon responses.
struct CriticalImagesInfo {
  CriticalImagesInfo()
      : is_critical_image_info_present(false) {}
  StringSet html_critical_images;
  StringSet css_critical_images;
  CriticalImages proto;
  bool is_critical_image_info_present;
  RenderedImageDimensionsMap rendered_images_map;
};


// Finds critical images i.e. images which are above the fold for a given url.
// This information may be used by DelayImagesFilter.
class CriticalImagesFinder {
 public:
  enum Availability {
    kDisabled,   // Data will never be forthcoming
    kNoDataYet,  // Data is expected but we don't have it yet
    kAvailable,  // Data is available
  };
  static const char kCriticalImagesValidCount[];
  static const char kCriticalImagesExpiredCount[];
  static const char kCriticalImagesNotFoundCount[];
  static const char kCriticalImagesPropertyName[];
  // Property name for the rendered image dimensions retreived from webkit
  // render response for the page.
  static const char kRenderedImageDimensionsProperty[];

  CriticalImagesFinder(const PropertyCache::Cohort* cohort, Statistics* stats);
  virtual ~CriticalImagesFinder();

  static void InitStats(Statistics* statistics);

  // Checks whether IsHtmlCriticalImage will return meaningful results about
  // critical images. Users of IsHtmlCriticalImage should check this function
  // and supply default behaviors when Available != kAvailable.
  virtual Availability Available(RewriteDriver* driver);

  // In order to handle varying critical image sets returned by the beacon, we
  // store a history of the last N critical images, and only declare an image
  // critical if it appears critical in the last M out of N sets reported. This
  // function returns what percentage of the sets need to include the image for
  // it be considered critical.
  virtual int PercentSeenForCritical() const {
    return kDefaultPercentSeenForCritical;
  }

  // Minimum interval to store support for critical image results. This affects
  // how long we keep around evidence that an image might be critical; we'll
  // remember the fact for at least SupportInterval beacon insertions if it only
  // occurs once, and we'll remember it longer if multiple beacons support image
  // criticality.  By default, SupportInteval() = 1 and we only store one beacon
  // result. The beacon critical image finder should override this to store a
  // larger number of sets.
  virtual int SupportInterval() const {
    return kDefaultImageSupportInterval;
  }

  // Checks whether the requested image is present in the critical set or not.
  // Users of this function should also check Available() to see if the
  // implementation of this function returns meaningful results and provide a
  // default behavior if it does not.  If no critical set value has been
  // obtained, returns false (not critical).
  // TODO(jud): It would be simpler to modify these interfaces to take
  // HtmlElement* instead of GoogleStrings. This would move some complexity in
  // getting the correct URL from the caller into this function. For instance,
  // if an image has been modified by LazyloadImages then the actual src we want
  // to check is in the data-pagespeed-lazy-src attribute, not in src.
  bool IsHtmlCriticalImage(StringPiece image_url, RewriteDriver* driver);

  bool IsCssCriticalImage(StringPiece image_url, RewriteDriver* driver);

  // Returns true if rendered dimensions exist for the image_src_url and
  // populates dimensions in the std::pair.
  bool GetRenderedImageDimensions(
      RewriteDriver* driver,
      const GoogleUrl& image_src_gurl,
      std::pair<int32, int32>* dimensions);

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
  // TODO(jud): Make this protected. There is a lingering public usage in
  // critical_images_beacon_filter.cc.
  const PropertyCache::Cohort* cohort() const { return cohort_; }

  // Updates the critical images property cache entry. Returns whether the
  // update succeeded or not. Note that this base implementation does not call
  // WriteCohort. This should be called in the subclass if the cohort is not
  // written elsewhere. NULL is permitted for the critical image sets if only
  // one of the html or css sets is being updated, but not the other.
  bool UpdateCriticalImagesCacheEntryFromDriver(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      RewriteDriver* driver);

  // Setup the HTML and CSS critical image sets in critical_images_info from the
  // property_value. Return true if property_value had a value, and
  // deserialization of it succeeded.  Here because helper code needs access to
  // it.
  static bool PopulateCriticalImagesFromPropertyValue(
      const PropertyValue* property_value, CriticalImages* critical_images);

  // Alternative interface to update the critical images cache entry. This is
  // useful in contexts like the beacon handler where the RewriteDriver for the
  // original request no longer exists.
  static bool UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const RenderedImages* rendered_images_set,
      int support_interval,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page);

  // Returns true if the critical images are available, false otherwise. This is
  // virtual only to be overridden in tests.
  virtual bool IsCriticalImageInfoPresent(RewriteDriver* driver);

  // Extracts rendered images' dimensions from property cache.
  virtual RenderedImages* ExtractRenderedImageDimensionsFromCache(
      RewriteDriver* driver);

  // Adds the given url to the html critical image set for the driver.
  void AddHtmlCriticalImage(const GoogleString& url,
                            RewriteDriver* driver);

  // Parses Json map returned from beacon js and populates RenderedImages proto.
  // Caller takes ownership of the returned pointer.
  RenderedImages* JsonMapToRenderedImagesMap(const GoogleString& str,
                                             const RewriteOptions* options);

  // Returns true if it's time to inject a beacon onto the page. The default
  // finder doesn't use beaconing, so it always returns false.
  virtual bool ShouldBeacon(RewriteDriver* driver) { return false; }

  // Check property cache state and prepare to insert beacon.  Returns the
  // metadata where result.status == kDoNotBeacon if no beaconing should occur,
  // and result.nonce contains the nonce if required (default implementation
  // always beacons without a nonce).
  virtual BeaconMetadata PrepareForBeaconInsertion(RewriteDriver* driver) {
    BeaconMetadata result;
    result.status = kBeaconNoNonce;
    return result;
  }

  // For implementations that use beaconing, update the candidate images in the
  // property cache. New images are a signal that we should beacon more often
  // for a few requests. The beaconing argument should indicate if the current
  // request is injecting a beacon. If so, we don't need to trigger a beacon on
  // the next request even if the candidate images have changed.
  virtual void UpdateCandidateImagesForBeaconing(const StringSet& images,
                                                 RewriteDriver* driver,
                                                 bool beaconing) {}

 protected:
  // Completes a critical image set update operation and writes the data back to
  // the property cache.
  static bool UpdateAndWriteBackCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const RenderedImages* rendered_images_set,
      int support_interval,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page,
      CriticalImages* critical_images);

  // Gets critical images if present in the property cache and updates the
  // critical_images set in RewriteDriver with the obtained set.  If you
  // override this method, driver->critical_images_info() must not return NULL
  // after this function has been called.
  virtual void UpdateCriticalImagesSetInDriver(RewriteDriver* driver);

  virtual GoogleString GetKeyForUrl(StringPiece url) { return url.as_string(); }

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
                                   int support_interval,
                                   CriticalImages* critical_images);

  // By default, store 1 critical image set and require an image to be in that
  // set for it to be critical.
  static const int kDefaultPercentSeenForCritical = 100;
  static const int kDefaultImageSupportInterval = 1;

  const PropertyCache::Cohort* cohort_;

  Variable* critical_images_valid_count_;
  Variable* critical_images_expired_count_;
  Variable* critical_images_not_found_count_;

  DISALLOW_COPY_AND_ASSIGN(CriticalImagesFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_FINDER_H_
