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
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteDriver;

// Support critical (above the fold) image detection through a javascript beacon
// on the client.
// TODO(jud): This class is not yet implemented.
class BeaconCriticalImagesFinder : public CriticalImagesFinder {
 public:
  static const char kBeaconCohort[];

  BeaconCriticalImagesFinder();
  virtual ~BeaconCriticalImagesFinder();

  virtual bool IsMeaningful() const {
    // TODO(jud): This class is not currently implemented yet, change this when
    // it is functional.
    return false;
  }

  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver,
                                     bool must_compute);

  virtual const char* GetCriticalImagesCohort() const {
    return kBeaconCohort;
  }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_IMAGES_FINDER_H_
