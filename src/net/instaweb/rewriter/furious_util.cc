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
// Functionality for manipulating Furious exeriment state and cookies.

#include "net/instaweb/rewriter/public/furious_util.h"

#include <cstddef>
#include <cstdlib>

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"

// Separates the experiment ID from the Furious state in the cookie.
#define SEPARATOR ":"

namespace net_instaweb {
namespace furious {

// Furious cookie value strings.
const char kFuriousCookieA[] = "1";
const char kFuriousCookieB[] = "2";
const char kFuriousCookieNone[] = "0";
const char kFuriousCookieNotSet[] = "";

// TODO(nforman): Make sure the experiment id in the cookie matches the
// current experiment in RewriteOptions.
bool GetFuriousCookieState(const RequestHeaders* headers, FuriousState* value) {
  ConstStringStarVector v;
  GoogleString prefix;
  *value = kFuriousNotSet;
  StrAppend(&prefix, kFuriousCookie, "=");
  if (headers->Lookup(HttpAttributes::kCookie, &v)) {
    for (int i = 0, nv = v.size(); i < nv; ++i) {
      StringPieceVector cookies;
      SplitStringPieceToVector(*(v[i]), ";", &cookies, true);
      for (int j = 0, ncookies = cookies.size(); j < ncookies; ++j) {
        StringPiece cookie(cookies[j]);
        TrimWhitespace(&cookie);
        if (StringCaseStartsWith(cookie, prefix)) {
          cookie.remove_prefix(prefix.length());
          *value = CookieStringToState(cookie);
          // If we got a bogus value for the cookie, keep looking for another
          // one just in case.
          if (*value != kFuriousNotSet) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void RemoveFuriousCookie(RequestHeaders* headers) {
  headers->RemoveCookie(kFuriousCookie);
}

void SetFuriousCookie(ResponseHeaders* headers,
                      const StringPiece& experiment_id,
                      FuriousState state,
                      const char* url,
                      int64 now_ms) {
  GoogleUrl request_url(url);
  // If we can't parse this url, don't try to set headers on the response.
  if (!request_url.is_valid()) {
    return;
  }
  GoogleString expires;
  ConvertTimeToString(now_ms + Timer::kWeekMs, &expires);
  GoogleString value = StringPrintf(
      "%s=%s; Expires=%s; Domain=.%s; Path=/",
      kFuriousCookie, FuriousStateToCookieString(experiment_id, state).c_str(),
      expires.c_str(), request_url.Host().as_string().c_str());
  headers->Add(HttpAttributes::kSetCookie, value);
  headers->ComputeCaching();
}


// TODO(nforman): Is this a reasonable way of getting the appropriate
// percentage of the traffic?
// It might be "safer" to do this as a hash of ip so that if one person
// sent simultaneous requests, they would end up on the same side of the
// experiment for all requests.
FuriousState DetermineFuriousState(const RewriteOptions* options) {
  FuriousState ret = kFuriousNone;
  long index = random();
  // We devide by 200.0 to get half the percentage of traffic in A and
  // then the other half in B.  E.g. if furious_percent_ is 40,
  // we want 40/200 => .2 in A and .2 in B.
  double mult = static_cast<double>(options->furious_percent())/200.0;
  int middle_bound = mult * RAND_MAX;
  if (index < middle_bound) {
    ret = kFuriousA;
  } else if (index < middle_bound * 2) {
    ret = kFuriousB;
  }
  return ret;
}

FuriousState CookieStringToState(const StringPiece& cookie_str) {
  size_t index = cookie_str.find(SEPARATOR);
  if (index == StringPiece::npos) {
    return kFuriousNotSet;
  }
  StringPiece value(cookie_str.data(), cookie_str.length());
  value.remove_prefix(index + 1);
  if (value == kFuriousCookieA) {
    return kFuriousA;
  }
  if (value == kFuriousCookieB) {
    return kFuriousB;
  }
  if (value == kFuriousCookieNone) {
    return kFuriousNone;
  }
  return kFuriousNotSet;
}

GoogleString FuriousStateToCookieString(const StringPiece& experiment_id,
                                        const FuriousState state) {
  GoogleString cookie_value(experiment_id.data(), experiment_id.length());
  cookie_value.append(SEPARATOR);
  if (state == kFuriousA) {
    cookie_value.append(kFuriousCookieA);
  } else if (state == kFuriousB) {
    cookie_value.append(kFuriousCookieB);
  } else if (state == kFuriousNone) {
    cookie_value.append(kFuriousCookieNone);
  } else {
    cookie_value.append(kFuriousCookieNotSet);
  }
  return cookie_value;
}

void FuriousNoFilterDefault(RewriteOptions* options) {
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->ForceEnableFilter(RewriteOptions::kAddHead);
  options->ForceEnableFilter(RewriteOptions::kAddInstrumentation);
  options->ForceEnableFilter(RewriteOptions::kInsertGA);
  options->ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
}

}  // namespace furious

}  // namespace net_instaweb
