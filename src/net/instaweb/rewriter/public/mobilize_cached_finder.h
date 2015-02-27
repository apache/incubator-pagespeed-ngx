/*
 * Copyright 2015 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_CACHED_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_CACHED_FINDER_H_

#include "net/instaweb/rewriter/mobilize_cached.pb.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class RewriteDriver;
class Statistics;
class TimedVariable;

// Helpers to store/retrieve cached mobilizer information in the property
// cache.
class MobilizeCachedFinder {
 public:
  static const char kMobilizeCachedPropertyName[];

  static const char kMobilizeCachedValidCount[];
  static const char kMobilizeCachedExpiredCount[];
  static const char kMobilizeCachedNotFoundCount[];
  static const char kMobilizeCachedNoConsensusCount[];

  // All of the passed-in constructor arguments are owned by the caller.
  MobilizeCachedFinder(const PropertyCache::Cohort* cohort, Statistics* stats);
  ~MobilizeCachedFinder();

  static void InitStats(Statistics* statistics);

  // Returns whether successful.
  bool GetMobilizeCachedFromPropertyCache(RewriteDriver* driver,
                                          MobilizeCached* out);

  void UpdateMobilizeCachedInPropertyCache(const MobilizeCached& new_sample,
                                           RewriteDriver* driver);


 private:
  const PropertyCache::Cohort* cohort_;

  TimedVariable* mobilize_cached_valid_count_;
  TimedVariable* mobilize_cached_expired_count_;
  TimedVariable* mobilize_cached_not_found_count_;
  TimedVariable* mobilize_cached_no_consensus_count_;

  DISALLOW_COPY_AND_ASSIGN(MobilizeCachedFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_CACHED_FINDER_H_
