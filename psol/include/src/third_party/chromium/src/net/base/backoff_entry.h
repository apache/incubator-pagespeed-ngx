// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_BACKOFF_ITEM_H_
#define NET_BASE_BACKOFF_ITEM_H_
#pragma once

#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "net/base/net_api.h"

namespace net {

// Provides the core logic needed for randomized exponential back-off
// on requests to a given resource, given a back-off policy.
//
// This utility class knows nothing about network specifics; it is
// intended for reuse in various networking scenarios.
class NET_TEST BackoffEntry : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // The set of parameters that define a back-off policy.
  struct Policy {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    int num_errors_to_ignore;

    // Initial delay for exponential back-off.
    int initial_backoff_ms;

    // Factor by which the waiting time will be multiplied.
    double multiply_factor;

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    double jitter_factor;

    // Maximum amount of time we are willing to delay our request.
    int maximum_backoff_ms;

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    int entry_lifetime_ms;
  };

  // Lifetime of policy must enclose lifetime of BackoffEntry. The
  // pointer must be valid but is not dereferenced during construction.
  explicit BackoffEntry(const Policy* const policy);
  virtual ~BackoffEntry();

  // Inform this item that a request for the network resource it is
  // tracking was made, and whether it failed or succeeded.
  void InformOfRequest(bool succeeded);

  // Returns true if a request for the resource this item tracks should
  // be rejected at the present time due to exponential back-off policy.
  bool ShouldRejectRequest() const;

  // Returns the absolute time after which this entry (given its present
  // state) will no longer reject requests.
  base::TimeTicks GetReleaseTime() const;

  // Causes this object reject requests until the specified absolute time.
  // This can be used to e.g. implement support for a Retry-After header.
  void SetCustomReleaseTime(const base::TimeTicks& release_time);

  // Returns true if this object has no significant state (i.e. you could
  // just as well start with a fresh BackoffEntry object), and hasn't
  // had for Policy::entry_lifetime_ms_.
  bool CanDiscard() const;

  // Resets this entry to a fresh (as if just constructed) state.
  void Reset();

  // Returns the failure count for this entry.
  int failure_count() const { return failure_count_; }

 protected:
  // Equivalent to TimeTicks::Now(), virtual so unit tests can override.
  virtual base::TimeTicks ImplGetTimeNow() const;

 private:
  // Calculates when requests should again be allowed through.
  base::TimeTicks CalculateReleaseTime() const;

  // Timestamp calculated by the exponential back-off algorithm at which we are
  // allowed to start sending requests again.
  base::TimeTicks exponential_backoff_release_time_;

  // Counts request errors; reset on success.
  int failure_count_;

  const Policy* const policy_;

  DISALLOW_COPY_AND_ASSIGN(BackoffEntry);
};

}  // namespace net

#endif  // NET_BASE_BACKOFF_ITEM_H_
