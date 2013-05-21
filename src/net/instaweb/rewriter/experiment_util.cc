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
// Functionality for manipulating exeriment state and cookies.

#include "net/instaweb/rewriter/public/experiment_util.h"

#include <cstdlib>

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/time_util.h"

namespace net_instaweb {
namespace experiment {

bool GetExperimentCookieState(const RequestHeaders& headers, int* value) {
  ConstStringStarVector v;
  *value = kExperimentNotSet;
  if (headers.Lookup(HttpAttributes::kCookie, &v)) {
    for (int i = 0, nv = v.size(); i < nv; ++i) {
      StringPieceVector cookies;
      SplitStringPieceToVector(*(v[i]), ";", &cookies, true);
      for (int j = 0, ncookies = cookies.size(); j < ncookies; ++j) {
        StringPiece cookie(cookies[j]);
        TrimWhitespace(&cookie);
        if (StringCaseStartsWith(cookie, kExperimentCookiePrefix)) {
          cookie.remove_prefix(STATIC_STRLEN(kExperimentCookiePrefix));
          *value = CookieStringToState(cookie);
          // If we got a bogus value for the cookie, keep looking for another
          // one just in case.
          if (*value != kExperimentNotSet) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void RemoveExperimentCookie(RequestHeaders* headers) {
  headers->RemoveCookie(kExperimentCookie);
}

void SetExperimentCookie(ResponseHeaders* headers,
                      int state,
                      const StringPiece& url,
                      int64 expiration_time_ms) {
  GoogleUrl request_url(url);
  // If we can't parse this url, don't try to set headers on the response.
  if (!request_url.is_valid()) {
    return;
  }
  GoogleString expires;
  ConvertTimeToString(expiration_time_ms, &expires);
  StringPiece host = request_url.Host();
  if (host.length() == 0) {
    return;
  }
  GoogleString value = StringPrintf(
      "%s=%s; Expires=%s; Domain=.%s; Path=/",
      kExperimentCookie, ExperimentStateToCookieString(state).c_str(),
      expires.c_str(), host.as_string().c_str());
  headers->Add(HttpAttributes::kSetCookie, value);
  headers->ComputeCaching();
}

// TODO(nforman): Is this a reasonable way of getting the appropriate
// percentage of the traffic?
// It might be "safer" to do this as a hash of ip so that if one person
// sent simultaneous requests, they would end up on the same side of the
// experiment for all requests.
int DetermineExperimentState(const RewriteOptions* options) {
  int ret = kExperimentNotSet;
  int num_experiments = options->num_experiments();

  // If are no experiments, return kExperimentNotSet so RewriteOptions doesn't
  // try to change.
  if (num_experiments < 1) {
    return ret;
  }

  int64 bound = 0;
  int64 index = random();
  ret = kNoExperiment;
  // One of these should be the control.
  for (int i = 0; i < num_experiments; ++i) {
    RewriteOptions::ExperimentSpec* spec = options->experiment_spec(i);
    double mult = static_cast<double>(spec->percent())/100.0;

    // Because RewriteOptions checks to make sure the total experiment
    // percentage is not greater than 100, bound should never be greater
    // than RAND_MAX.
    bound += (mult * RAND_MAX);
    if (index < bound) {
      ret = spec->id();
      return ret;
    }
  }
  return ret;
}

int CookieStringToState(const StringPiece& cookie_str) {
  int ret;
  if (!StringToInt(cookie_str, &ret)) {
    ret = kExperimentNotSet;
  }
  return ret;
}

GoogleString ExperimentStateToCookieString(int state) {
  GoogleString cookie_value = IntegerToString(state);
  return cookie_value;
}


}  // namespace experiment

}  // namespace net_instaweb
