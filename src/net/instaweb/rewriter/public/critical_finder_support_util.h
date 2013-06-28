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

#include <map>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Default rebeaconing time.
const int64 kMinBeaconIntervalMs = 5 * Timer::kSecondMs;
const int64 kBeaconTimeoutIntervalMs = Timer::kMinuteMs;
// A nonce value that's valid, used as a placeholder when nonce generation is
// switched off.
const char kValidNonce[] = "*";

class CriticalKeys;
class MessageHandler;
class NonceGenerator;

typedef std::map<GoogleString, int> SupportMap;

// Generate and return a list of the critical keys from a proto, taking into
// account legacy keys that may have been added before.
StringSet GetCriticalKeysFromProto(const CriticalKeys& critical_keys);

// Update the property cache with a new set of keys. This will update the
// support value for the new keys, ignoring any keys that are not already
// present in the property cache (preventing spurious keys from being injected).
// Note, that it only increases the support value for the new keys, it does not
// decay values that are not present. PrepareForBeaconInsertion - which decays
// support - should have been called previously.
void WriteCriticalKeysToPropertyCache(
    const StringSet& new_keys, StringPiece nonce, int support_interval,
    bool should_replace_prior_result, StringPiece property_name,
    const PropertyCache* cache, const PropertyCache::Cohort* cohort,
    AbstractPropertyPage* page, MessageHandler* message_handler, Timer* timer);

// Given a set of candidate critical keys, decide whether beaconing
// should take place (and if so, decay existing key evidence).  We should
// *always* beacon if there's new critical key data.  Otherwise
// re-beaconing is based on a time and request interval.
GoogleString PrepareForBeaconInsertion(
    const StringSet& keys, CriticalKeys* proto, int support_interval,
    bool should_replace_prior_result, StringPiece property_name,
    const PropertyCache::Cohort* cohort, AbstractPropertyPage* page,
    NonceGenerator* nonce_generator, Timer* timer);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_FINDER_SUPPORT_UTIL_H_
