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

// Author: jmaessen@google.com (Jan Maessen)
//
// This contains some utilities for working with the critical_keys
// proto, and for updating support values. This is primarily used by
// CriticalSelectorFinder and CriticalImagesFinder.  These finders use the
// critical_keys proto to store a "support value" for each possible key (image
// URL or selector name) in the property cache.  When a beacon result arrives,
// the support for each critical key in the result is increased by
// support_interval.  When a new beacon is sent, existing support is decayed by
// multiplying by support_interval/(support_interval+1) and rounding down.  This
// means that a single key returned with a beacon will be considered critical
// until support_interval subsequent beacons have been injected.  Because
// support decays exponentially, repeated support for a key in multiple beacon
// results cause that key to be considered critical longer: two beacon results
// will expire after somewhat less than twice as long, three after rather less
// than three times as long, and so forth. This class also handles converting
// over old protobufs that did not use the support system.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_FINDER_SUPPORT_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_FINDER_SUPPORT_UTIL_H_

#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

// The amount of time after generating a nonce that we will accept it as valid.
// This keeps an attacker from accumulating large numbers of valid nonces to
// send many beacon responses at once.
const int64 kBeaconTimeoutIntervalMs = Timer::kMinuteMs;

// The number of valid beacons received that will switch from high frequency to
// low frequency beaconing.
const int64 kHighFreqBeaconCount = 3;

// The multiplier to apply to RewriteOptions::beacon_reinstrument_time_sec() to
// determine the low frequency beaconing interval. For example, the default
// value rebeaconing value is 5 seconds, so we will rebeacon every 5 seconds in
// high frequency mode, and every 500 seconds (~8 minutes) in low frequency
// mode.
const int64 kLowFreqBeaconMult = 100;

// The limit on the number of nonces that can expire before we stop trying to do
// high frequency beaconing. This is a signal that beacons are not configured
// correctly and so we drop into low frequency beaconing mode.
const int64 kNonceExpirationLimit = 5;

class MessageHandler;
class NonceGenerator;
class RewriteDriver;

enum BeaconStatus {
  kDoNotBeacon,
  kBeaconNoNonce,
  kBeaconWithNonce
};

struct BeaconMetadata {
  BeaconMetadata() : status(kDoNotBeacon) { }
  BeaconStatus status;
  GoogleString nonce;
};

// Check whether the given nonce is valid, invalidating any expired nonce
// entries we might encounter.  To avoid the need to copy and clear the nonce
// list, we invalidate the entry used and any expired entries by clearing the
// nonce value and timestamp.  These entries will be reused by
// AddNonceToCriticalSelectors.
bool ValidateAndExpireNonce(int64 now_ms, StringPiece nonce,
                            CriticalKeys* critical_keys);

// Generate a list of the critical keys from a proto, storing it into keys.
// Takes into account legacy keys that may have been added before.  A key is
// considered critical if its support is at least support_percentage of the
// maximum possible support value (which ramps up as beacon results arrive).
// When support_percentage = 0, any support is sufficient; when
// support_percentage = 100 all beacon results must support criticality.
void GetCriticalKeysFromProto(int64 support_percentage,
                              const CriticalKeys& critical_keys,
                              StringSet* keys);

// Add support for new_set to existing support.  The new_set should be obtained
// from a fully-validated beacon result -- this means PrepareForBeaconInsertion
// should have been called if required, and the resulting nonce should have been
// checked.  If require_prior_support then there must be an existing support
// entry (possibly 0) for new support to be registered.
void UpdateCriticalKeys(bool require_prior_support,
                        const StringSet& new_set, int support_value,
                        CriticalKeys* critical_keys);

bool ShouldBeacon(int64 next_beacon_timestamp_ms, const RewriteDriver& driver);

enum CriticalKeysWriteFlags {
  kNoRequirementsOnPriorResult = 0,  // Nice name for lack of next two flags.
  kReplacePriorResult = 1,
  kRequirePriorSupport = 2,
  kSkipNonceCheck = 4
};

// Update the property cache with a new set of keys. This will update the
// support value for the new keys. If require_prior_support is set, any keys
// that are not already present in the property cache will be ignored (to
// prevent spurious keys from being injected). Note that it only increases the
// support value for the new keys, it does not decay values that are not
// present. PrepareForBeaconInsertion should have been called previously if
// !should_replace_prior_result and nonces must be checked.
void WriteCriticalKeysToPropertyCache(
    const StringSet& new_keys, StringPiece nonce, int support_interval,
    CriticalKeysWriteFlags flags,
    StringPiece property_name, const PropertyCache* cache,
    const PropertyCache::Cohort* cohort, AbstractPropertyPage* page,
    MessageHandler* message_handler, Timer* timer);

// Given a set of candidate critical keys, decide whether beaconing should take
// place.  We should *always* beacon if there's new critical key data. Otherwise
// re-beaconing is based on a time and request interval, and 2 modes of
// beaconing frequency are supported. At first, beaconing occurs at a
// high frequency until we have collected kHighFreqBeaconCount beacons; after
// that, we transition into low frequency beaconing mode, where beaconing occurs
// less often. We also track the number of expired nonces since the last valid
// beacon was received to see if beaconing is set up correctly, and if it looks
// like it isn't, only do low frequency beaconing. Sets status and nonce
// appropriately in *result (nonce will be empty if no nonce is required).  If
// candidate keys are not required, keys may be empty (but new candidate
// detection will not occur).  If result->status != kDontBeacon, caller should
// write proto back to the property cache using UpdateInPropertyCache.
void PrepareForBeaconInsertionHelper(CriticalKeys* proto,
                                     NonceGenerator* nonce_generator,
                                     RewriteDriver* driver,
                                     bool using_candidate_key_detection,
                                     BeaconMetadata* result);

// Update the candidate key set in proto. If new candidate keys are detected,
// they are inserted into proto with a support value of 0, and true is returned.
// Otherwise returns false. If clear_rebeacon_timestamp is set, the rebeacon
// timestamp field in the proto is cleared to force rebeaconing on the next
// request.
bool UpdateCandidateKeys(const StringSet& keys, CriticalKeys* proto,
                         bool clear_rebeacon_timestamp);

// Based on the CriticalKeys data seen so far, describe whether beacon metadata
// is available.  This returns false until data is received.
inline bool IsBeaconDataAvailable(const CriticalKeys& proto) {
  return (proto.valid_beacons_received() > 0);
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_FINDER_SUPPORT_UTIL_H_
