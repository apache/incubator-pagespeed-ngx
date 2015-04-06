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
// Author: jud@google.com (Jud Porter)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_IMAGES_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_IMAGES_FINDER_H_

#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class NonceGenerator;
class RenderedImages;
class RewriteDriver;
class Statistics;
class Timer;

// Support critical (above the fold) image detection through a javascript beacon
// on the client.
class BeaconCriticalImagesFinder : public CriticalImagesFinder {
 public:
  // All constructor args are owned by the caller.
  BeaconCriticalImagesFinder(
      const PropertyCache::Cohort* cohort,
      NonceGenerator* nonce_generator,
      Statistics* stats);
  virtual ~BeaconCriticalImagesFinder();

  virtual Availability Available(RewriteDriver* driver);

  virtual int PercentSeenForCritical() const {
    return kBeaconPercentSeenForCritical;
  }

  virtual int SupportInterval() const {
    return kBeaconImageSupportInterval;
  }

  virtual void ComputeCriticalImages(RewriteDriver* driver) {}

  // Update the critical image entry in the property cache. This is meant to be
  // called in the beacon handler, where there is no RewriteDriver available.
  static bool UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const RenderedImages* rendered_images_set,
      const StringPiece& nonce,
      const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page,
      Timer* timer);

  virtual bool ShouldBeacon(RewriteDriver* driver);
  // Check beacon interval and nonce state, and return appropriate
  // BeaconMetadata; result.status indicates whether beaconing should occur, and
  // result.nonce contains the nonce (if required).
  virtual BeaconMetadata PrepareForBeaconInsertion(RewriteDriver* driver);

  virtual void UpdateCandidateImagesForBeaconing(const StringSet& images,
                                                 RewriteDriver* driver,
                                                 bool beaconing);

 private:
  virtual GoogleString GetKeyForUrl(StringPiece url);

  // 80% is a guess at a reasonable value for this param.
  static const int kBeaconPercentSeenForCritical = 80;
  // This is a guess for how many samples we need to get stable data.
  static const int kBeaconImageSupportInterval = 10;

  NonceGenerator* nonce_generator_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_IMAGES_FINDER_H_
