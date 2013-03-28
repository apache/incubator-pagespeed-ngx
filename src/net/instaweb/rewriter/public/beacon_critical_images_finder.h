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

#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Statistics;

// Support critical (above the fold) image detection through a javascript beacon
// on the client.
class BeaconCriticalImagesFinder : public CriticalImagesFinder {
 public:
  explicit BeaconCriticalImagesFinder(Statistics* stats);
  virtual ~BeaconCriticalImagesFinder();

  virtual bool IsMeaningful(const RewriteDriver* driver) const {
    return driver->options()->critical_images_beacon_enabled();
  }

  virtual int PercentSeenForCritical() const {
    return kBeaconPercentSeenForCritical;
  }

  virtual int NumSetsToKeep() const {
    return kBeaconNumSetsToKeep;
  }

  // Checks whether the requested image is present in the critical set or not.
  // The critical image beacon sends back hashes of the URls to save space, so
  // this computes the same hash on image_url and checks if it is stored in the
  // critical image set.
  virtual bool IsHtmlCriticalImage(const GoogleString& image_url,
                               RewriteDriver* driver);

  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver);

  virtual const char* GetCriticalImagesCohort() const {
    return RewriteDriver::kBeaconCohort;
  }

 private:
  // 80% is a guess at a reasonable value for this param.
  static const int kBeaconPercentSeenForCritical = 80;
  // This is a guess for how many samples we need to get stable data.
  static const int kBeaconNumSetsToKeep = 10;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_IMAGES_FINDER_H_
