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

// Author: morlovich@google.com (Maks Orlovich)
//
// Helpers for classifying and caching various kinds of fetch failures.

#include "net/instaweb/http/public/http_cache_failure.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

// Remember that a Fetch failed for 5 minutes by default.
//
// TODO(jmarantz): We could handle cc-private a little differently:
// in this case we could arguably remember it using the original cc-private ttl.
const int kRememberNotCacheableTtlSec = 300;
const int kRememberFetchFailedTtlSec = 300;
const int kRememberEmptyTtlSec = 300;

// We use an extremely low TTL for load-shed resources since we don't
// want this to get in the way of debugging, or letting a page with
// large numbers of refresh converge towards being fully optimized.
//
// Note if you bump this number too high, then
// RewriteContextTest.DropFetchesAndRecover cannot pass because we
// won't try fetches for dropped resources until after the rewrites
// for the successful fetches will expire.  In system terms, that means
// that you can never complete rewrites for a page with so many resources
// that the initial round of fetches gets some dropped.
const int kRememberFetchDroppedTtlSec = 10;

}  // namespace

HttpCacheFailurePolicy::HttpCacheFailurePolicy() {
  // Set up compiled-in defaults.
  for (int i = 0, n = arraysize(ttl_sec_for_status); i < n; ++i) {
    ttl_sec_for_status[i] = kRememberFetchFailedTtlSec;
  }

  ttl_sec_for_status[kFetchStatusUncacheable200] = kRememberNotCacheableTtlSec;
  ttl_sec_for_status[kFetchStatusUncacheableError] =
      kRememberNotCacheableTtlSec;
  ttl_sec_for_status[kFetchStatus4xxError] = kRememberFetchFailedTtlSec;
  ttl_sec_for_status[kFetchStatusOtherError] = kRememberFetchFailedTtlSec;
  ttl_sec_for_status[kFetchStatusDropped] = kRememberFetchDroppedTtlSec;
  ttl_sec_for_status[kFetchStatusEmpty] = kRememberEmptyTtlSec;
}

FetchResponseStatus HttpCacheFailure::ClassifyFailure(
    const ResponseHeaders& headers,
    StringPiece contents,
    bool physical_fetch_success,
    bool external_cacheable) {
  FetchResponseStatus classification = kFetchStatusNotSet;

  int status_code = headers.status_code();
  if (physical_fetch_success && !headers.IsErrorStatus()) {
    if (contents.empty() && !headers.IsRedirectStatus()) {
      // Do not cache empty 200 responses, but remember that they were empty
      // to avoid fetching too often.
      // https://github.com/pagespeed/mod_pagespeed/issues/1050
      classification = kFetchStatusEmpty;
    } else if (!external_cacheable) {
      classification = (status_code == 200 ? kFetchStatusUncacheable200
                                           : kFetchStatusUncacheableError);
    } else if (status_code == 200) {
      classification = kFetchStatusOK;
    } else {
      // It's some failure, but it's not a 4xx, 5xx, nor cacheability...
      classification = kFetchStatusOtherError;
    }
  } else {
    // 4xx, 5xx, or physical failure (which includes load-shedding drops).
    if (headers.Has(HttpAttributes::kXPsaLoadShed)) {
      classification = kFetchStatusDropped;
    } else if (status_code >= 400 && status_code < 500) {
      classification = kFetchStatus4xxError;
    } else {
      classification = kFetchStatusOtherError;
    }
  }
  DCHECK_NE(classification, kFetchStatusNotSet);
  return classification;
}

bool HttpCacheFailure::IsFailureCachingStatus(HttpStatus::Code code) {
  return (code >= HttpStatus::kRememberFailureRangeStart &&
          code < HttpStatus::kRememberFailureRangeEnd);
}

FetchResponseStatus HttpCacheFailure::DecodeFailureCachingStatus(
    HttpStatus::Code code) {
  switch (code) {
    case HttpStatus::kRememberNotCacheableAnd200StatusCode:
      return kFetchStatusUncacheable200;
    case HttpStatus::kRememberNotCacheableStatusCode:
      return kFetchStatusUncacheableError;
    case HttpStatus::kRememberFetchFailed4xxCode:
      return kFetchStatus4xxError;
    case HttpStatus::kRememberFetchFailedStatusCode:
      return kFetchStatusOtherError;
    case HttpStatus::kRememberDroppedStatusCode:
      return kFetchStatusDropped;
    case HttpStatus::kRememberEmptyStatusCode:
      return kFetchStatusEmpty;
    default:
      LOG(DFATAL) << "Decode unexpected failure status code:" << code;
      return kFetchStatusNotSet;
  }
}

HttpStatus::Code HttpCacheFailure::EncodeFailureCachingStatus(
    FetchResponseStatus status) {
  switch (status) {
    case kFetchStatusUncacheable200:
      return HttpStatus::kRememberNotCacheableAnd200StatusCode;
    case kFetchStatusUncacheableError:
      return HttpStatus::kRememberNotCacheableStatusCode;
    case kFetchStatus4xxError:
      return HttpStatus::kRememberFetchFailed4xxCode;
    case kFetchStatusOtherError:
      return HttpStatus::kRememberFetchFailedStatusCode;
    case kFetchStatusDropped:
      return HttpStatus::kRememberDroppedStatusCode;
    case kFetchStatusEmpty:
      return HttpStatus::kRememberEmptyStatusCode;
    default:
      LOG(DFATAL) << "Encoded unexpected failure status:" << status;
      return HttpStatus::kRememberFetchFailedStatusCode;
  }
}

}  // namespace net_instaweb
