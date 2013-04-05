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
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CriticalSelectorSet;
class MessageHandler;
class PropertyCache;
class PropertyPage;
class RewriteDriver;
class Statistics;
class TimedVariable;

// Interface to store/retrieve critical selector information in the property
// cache.
class CriticalSelectorFinder {
 public:
  static const char kCriticalSelectorsValidCount[];
  static const char kCriticalSelectorsExpiredCount[];
  static const char kCriticalSelectorsNotFoundCount[];
  static const char kCriticalSelectorsPropertyName[];

  CriticalSelectorFinder(StringPiece cohort, Statistics* stats);
  virtual ~CriticalSelectorFinder();

  static void InitStats(Statistics* statistics);

  // Reads the recorded selector set from the property cache, and demarshals it.
  // Allocates a fresh object, transferring ownership of it to the caller.
  // May return NULL if no currently valid set is available.
  CriticalSelectorSet* DecodeCriticalSelectorsFromPropertyCache(
      RewriteDriver* driver);

  // Updates the critical selectors in the property cache. The new selector_set
  // is added to the list of previous beacon responses, and the
  // critical_selectors field is recomputed by taking the union of the selectors
  // in the previous NumSetsToKeep() sets. This updates the value in the
  // in-memory property page but does not write the cohort.
  void WriteCriticalSelectorsToPropertyCache(
      const StringSet& selector_set, RewriteDriver* driver);

  void WriteCriticalSelectorsToPropertyCache(
      const StringSet& selector_set, const PropertyCache* cache,
      PropertyPage* page, MessageHandler* message_handler);

  // Get the number of critical selector beacon results to keep.
  int NumSetsToKeep() const {
    return kDefaultNumSetsToKeep;
  }

  // TODO(morlovich): Add an API for enabling the appropriate instrumentation
  // filter; once it's clear when the configuration resolving takes place.

 private:
  // The number of beacon results to keep. This is an estimate for how many
  // samples it will take to get stable results.
  static const int kDefaultNumSetsToKeep = 10;

  // Merge the given set into the existing critical selector sets by adding the
  // new set to the history of sets, discarding the oldest if we exceed
  // NumSetsToKeep(), then recalculating the critical selectors as the union of
  // all the selectors in all the sets in the history.
  void UpdateCriticalSelectorSet(
    const StringSet& new_set, CriticalSelectorSet* set);

  GoogleString cohort_;

  TimedVariable* critical_selectors_valid_count_;
  TimedVariable* critical_selectors_expired_count_;
  TimedVariable* critical_selectors_not_found_count_;

  DISALLOW_COPY_AND_ASSIGN(CriticalSelectorFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FINDER_H_
