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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)
//
// Finds critical line information from http headers/config/pcache and populates
// critical line information into the RewriteDriver.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_LINE_INFO_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_LINE_INFO_FINDER_H_

#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"

namespace net_instaweb {

class CriticalLineInfo;
class RewriteDriver;

// The instantiated CriticalLineInfoFinder is held by ServerContext, meaning
// there is only 1 per server. The RewriteDriver's critical_line_info is the
// actual information.
class CriticalLineInfoFinder {
 public:
  static const char kCriticalLineInfoPropertyName[];

  explicit CriticalLineInfoFinder(const PropertyCache::Cohort* cohort);
  virtual ~CriticalLineInfoFinder();

  // Returns the cohort in pcache which stores the critical line info.
  const PropertyCache::Cohort* cohort() const { return cohort_; }

  // Populates the critical line information in the driver and return it.
  virtual const CriticalLineInfo* GetCriticalLine(RewriteDriver* driver);

  // Check property cache state and prepare to insert beacon. Returns the
  // metadata with result.status == kDoNotBeacon if no beaconing should occur,
  // and result.nonce contains the nonce if required (default implementation
  // always beacons without a nonce).
  virtual BeaconMetadata PrepareForBeaconInsertion(RewriteDriver* driver) {
    BeaconMetadata result;
    result.status = kBeaconNoNonce;
    return result;
  }

 protected:
  // Updates the critical line information in the driver.
  virtual void UpdateInDriver(RewriteDriver* driver);

 private:
  virtual int SupportInterval() const { return 1; }

  const PropertyCache::Cohort* cohort_;

  DISALLOW_COPY_AND_ASSIGN(CriticalLineInfoFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_LINE_INFO_FINDER_H_

