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
// Functionality and constants for handling Furious experiments and
// measurement.
//
// Furious is the A/B experiment framework that uses cookies
// and Google Analytics to track page speed statistics and correlate
// them with different sets of rewriters.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_UTIL_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;
class RewriteOptions;

namespace furious {

// Name of the Furious cookie we set when running experiments.
const char kFuriousCookie[] = "_GFURIOUS";

// Various states these options could be in regarding
// the Furious Experiment.
enum FuriousState {
  kFuriousNotSet,
  kFuriousNone,
  kFuriousA,
  kFuriousB
};

// Populates value with the state indicated by the FuriousCookie, if found.
// Returns true if a cookie was found, false if it was not.
bool GetFuriousCookieState(const RequestHeaders* headers, FuriousState* value);

// Removes the Furious cookie from the request headers so we don't
// send it to the origin.
void RemoveFuriousCookie(RequestHeaders *headers);

// Add a Set-Cookie header for Furious on the domain of url,
// one week from now_ms, putting it on the side of the experiment
// indicated by state.
void SetFuriousCookie(ResponseHeaders* headers,
                      const StringPiece& experiment_id,
                      FuriousState state, const char* url, int64 now_ms);

// Determines which side of the experiment this request should end up on.
FuriousState DetermineFuriousState(const RewriteOptions* options);

// Disables all the filters except for add_head, insert_ga, add_instrumentation,
// and HtmlWriter.  This is considered a "no-filter" base for
// furious experiments.
void FuriousNoFilterDefault(RewriteOptions* options);

// The string value of a Furious State.  We don't want to use "ToString"
// in case we change how we want the cookies to look.
GoogleString FuriousStateToCookieString(const StringPiece& experiment_id,
                                        const FuriousState state);

// Converts a Furious Cookie string, e.g. "2", into a FuriousState.
FuriousState CookieStringToState(const StringPiece& cookie_str);

}  // namespace furious

}  // namespace net_instaweb

#endif // NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_UTIL_H_
