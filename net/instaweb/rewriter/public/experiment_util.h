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

// Author: nforman@google.com (Naomi Forman)
//
// Functionality and constants for handling experiments and measurement.
//
// The experiment framework uses cookies and Google Analytics to track page
// speed statistics and correlate them with different sets of rewriters.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_UTIL_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;
class RewriteOptions;
class UserAgentMatcher;

namespace experiment {

enum ExperimentState {
  // You can force enrollment in any experiment setting including
  // kExperimentNotSet and kNoExperiment, which means we need a value to
  // indicate "don't force enrollment in any experiment".  This is the default,
  // and shouldn't appear outside of enroll_experiment_id in the options.
  //
  // Someone with enroll_experiment_id=<ForceNoExperiment> will still be
  // enrolled in an experiment if appropriate, but one will be randomly assigned
  // instead of explicitly specified.
  kForceNoExperiment = -2,
  // No experiment cookie was set, or if one was (by EnrollExperiment) then
  // assign them to an experiment category as if they had no cookie.
  kExperimentNotSet = -1,
  // There is a cookie set, but the cookie says: don't run experiments
  // on this user.  E.g. if you're running an A/B experiment on 40% of the
  // traffic, 20% is in A, 20% is in B, and 60% is in NoExperiment.
  kNoExperiment = 0,
};

// Name of the cookie we set when running experiments.
const char kExperimentCookie[] = "PageSpeedExperiment";
const char kExperimentCookiePrefix[] = "PageSpeedExperiment=";

// Populates value with the state indicated by the experiment cookie, if found.
// Returns true if a cookie was found, false if it was not.
bool GetExperimentCookieState(const RequestHeaders& headers, int* value);

// Removes the experiment cookie from the request headers so we don't
// send it to the origin.
void RemoveExperimentCookie(RequestHeaders *headers);

// Add a Set-Cookie header for the experiment on the domain of url, expiring at
// expiration_time_ms (specified as ms since the epoch), putting it on the side
// of the experiment indicated by state.
void SetExperimentCookie(ResponseHeaders* headers, int state,
                         const StringPiece& url, int64 expiration_time_ms);

// Determines which side of the experiment this request should end up on.
int DetermineExperimentState(const RewriteOptions* options,
                             const RequestHeaders& headers,
                             const UserAgentMatcher& matcher);

// Are there any experiments defined with percent > 0?
bool AnyActiveExperiments(const RewriteOptions* options);

// The string value of a Experiment State.  We don't want to use "ToString"
// in case we change how we want the cookies to look.
GoogleString ExperimentStateToCookieString(int state);

// Converts a Experiment Cookie string, e.g. "2", into a ExperimentState.
int CookieStringToState(const StringPiece& cookie_str);

}  // namespace experiment

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_UTIL_H_
