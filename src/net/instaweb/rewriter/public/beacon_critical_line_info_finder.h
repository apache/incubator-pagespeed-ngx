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

// Author: jud@google.com (Jud Porter)
//
// Manage critical line info from client side beacons used by the split_html
// filter.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_LINE_INFO_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_LINE_INFO_FINDER_H_

#include "net/instaweb/rewriter/public/critical_line_info_finder.h"

#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MessageHandler;
class NonceGenerator;
class RewriteDriver;
class Timer;

// This class provides beacon support in mod_pagespeed for the xpaths used by
// split_html. It does this by using the CriticalKey infrastructure also used
// by critical images and critical CSS selectors to populate the
// critical_line_info member in RewriteDriver and used by the base
// implementation.
// TODO(jud): Currently, this implementation just looks at the support value for
// an individual node to decide if it is below-the-fold or not. It should also
// combine the support values of a node's parent elements to decide if it's
// critical. The impact of missing this feature is that some nodes on or near
// the fold may not be properly considered at BTF, depending on the layout of
// the page.
// For example, consider if there is div[1] with a child node
// div[1]/span[a]. These nodes are close to the fold - clients with a larger
// screen consider just span[a] below-the-fold, while clients with smaller
// screens have both div[1] and span[a] below-the-fold. Both screen sizes
// however have span[a] as below-the-fold. The current implementation won't
// consider either node to be below-the-fold, since neither will receive enough
// support. When this TODO is fixed though, span[a] will be considered
// below-the-fold, since the support value for div[1] will be added to the
// support value for span[a].
class BeaconCriticalLineInfoFinder : public CriticalLineInfoFinder {
 public:
  static const char kBeaconCriticalLineInfoPropertyName[];

  BeaconCriticalLineInfoFinder(const PropertyCache::Cohort* cohort,
                               NonceGenerator* nonce_generator);
  virtual ~BeaconCriticalLineInfoFinder();

  virtual BeaconMetadata PrepareForBeaconInsertion(RewriteDriver* driver);

  // Write the xpaths sent from the split_html_beacon to the property cache.
  // This is a static method, because when the beacon is handled in
  // ServerContext, the RewriteDriver for the original request is long gone.
  static void WriteXPathsToPropertyCacheFromBeacon(
      const StringSet& xpaths_set, StringPiece nonce,
      const PropertyCache* cache, const PropertyCache::Cohort* cohort,
      AbstractPropertyPage* page, MessageHandler* message_handler,
      Timer* timer);

 protected:
  // Updates the critical line information in the driver.
  virtual void UpdateInDriver(RewriteDriver* driver);

 private:
  static const int kDefaultSupportInterval = 10;

  virtual int SupportInterval() const { return kDefaultSupportInterval; }

  NonceGenerator* nonce_generator_;
  DISALLOW_COPY_AND_ASSIGN(BeaconCriticalLineInfoFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BEACON_CRITICAL_LINE_INFO_FINDER_H_
