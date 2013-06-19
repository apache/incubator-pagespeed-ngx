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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FINDER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CriticalSelectorSet;
class MessageHandler;
class NonceGenerator;
class RewriteDriver;
class Statistics;
class TimedVariable;
class Timer;

// Interface to store/retrieve critical selector information in the property
// cache.  We store a "support value" for each possible critical selector in the
// property cache.  When a beacon result arrives, the support for each critical
// selector in the result is increased by SupportInterval().  When a new beacon
// is sent, existing support is decayed by multiplying by
// SupportInterval()/(SupportInterval()+1) and rounding down.  This means that a
// single selector returned with a beacon will be considered critical until
// SupportInterval() subsequent beacons have been injected.  Because support
// decays exponentially, repeated support for a selector in multiple beacon
// results cause that selector to be considered critical longer: two beacon
// results will expire after somewhat less than twice as long, three after
// rather less than three times as long, and so forth.
class CriticalSelectorFinder {
 public:
  static const char kCriticalSelectorsValidCount[];
  static const char kCriticalSelectorsExpiredCount[];
  static const char kCriticalSelectorsNotFoundCount[];
  static const char kCriticalSelectorsPropertyName[];
  static const int64 kMinBeaconIntervalMs;
  static const int64 kBeaconTimeoutIntervalMs;
  // A nonce value that's valid, used as a placeholder when nonce generation is
  // switched off.
  static const char kValidNonce[];

  // All of the passed-in constructor arguments are owned by the caller.  If
  // critical selector data is being received from a trusted source
  // (ShouldReplacePriorResult() must return true in this case), timer and
  // nonce_generator may be NULL.
  CriticalSelectorFinder(
      const PropertyCache::Cohort* cohort, Timer* timer,
      NonceGenerator* nonce_generator, Statistics* stats);
  virtual ~CriticalSelectorFinder();

  static void InitStats(Statistics* statistics);

  // Reads the recorded selector set from the property cache, and unmarshals it
  // into *critical_selectors.  Abstracts away the handling of multiple beacon
  // results.  Call this in preference to the method below.  Returns false and
  // empties *critical_selectors if no valid critical image set is available.
  static bool GetCriticalSelectorsFromPropertyCache(
      RewriteDriver* driver, StringSet* critical_selectors);

  // DON'T CALL THIS unless you are RewriteDriver; call
  // GetCriticalSelectorsFromPropertyCache instead.  Reads the recorded selector
  // set from the property cache, and demarshals it.  Allocates a fresh object,
  // transferring ownership of it to the caller.  May return NULL if no
  // currently valid set is available.
  // TODO(jmaessen): Remove when state is contained in finder itself.
  CriticalSelectorSet* DecodeCriticalSelectorsFromPropertyCache(
      RewriteDriver* driver);

  // Updates the critical selectors in the property cache.  Support for the new
  // selector_set is added to the existing record of beacon support.  This
  // updates the value in the in-memory property page but does not write the
  // cohort.  If results are obtained from a trusted source
  // (ShouldReplacePriorResult() must return true) then nonce may be NULL.
  virtual void WriteCriticalSelectorsToPropertyCache(
      const StringSet& selector_set, StringPiece nonce,
      RewriteDriver* driver);

  // As above, but suitable for use in a beacon context where no RewriteDriver
  // is available.  If results are obtained from a trusted source
  // (ShouldReplacePriorResult() must return true) then nonce may be NULL.
  void WriteCriticalSelectorsToPropertyCache(
      const StringSet& selector_set, StringPiece nonce,
      const PropertyCache* cache, PropertyPage* page,
      MessageHandler* message_handler);

  // Given a set of candidate critical selectors, decide whether beaconing
  // should take place.  We should *always* beacon if there's new critical
  // selector data.  Otherwise re-beaconing is based on a time and request
  // interval.  Returns beacon nonce if beaconing should occur, otherwise
  // returns an empty string.  Returns kValidNonce if nonces aren't being used
  // (ShouldReplacePriorResult() returns true).
  GoogleString PrepareForBeaconInsertion(
      const StringSet& selector_set, RewriteDriver* driver);

  // Gets the SupportInterval for a new beacon result (see comment at top).
  virtual int SupportInterval() const {
    return kDefaultSupportInterval;
  }

 protected:
  // Returns true if a beacon result should replace all previous results.
  virtual bool ShouldReplacePriorResult() const { return false; }

  // TODO(morlovich): Add an API for enabling the appropriate instrumentation
  // filter; once it's clear when the configuration resolving takes place.

 private:
  // Default support interval
  static const int kDefaultSupportInterval = 10;

  const PropertyCache::Cohort* cohort_;
  Timer* timer_;
  NonceGenerator* nonce_generator_;

  TimedVariable* critical_selectors_valid_count_;
  TimedVariable* critical_selectors_expired_count_;
  TimedVariable* critical_selectors_not_found_count_;

  DISALLOW_COPY_AND_ASSIGN(CriticalSelectorFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FINDER_H_
