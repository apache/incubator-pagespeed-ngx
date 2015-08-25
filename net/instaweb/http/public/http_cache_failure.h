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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_FAILURE_H_
#define NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_FAILURE_H_

#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

// This enumerates different states of the fetched response. This module
// is mostly concerned with specific failure statuses, but it's convenient
// to have non-failure ones in the same enum.
enum FetchResponseStatus {
  kFetchStatusNotSet = 0,
  kFetchStatusOK = 1,
  kFetchStatusUncacheable200 = 2,
  kFetchStatusUncacheableError = 3,
  kFetchStatus4xxError = 4,
  kFetchStatusOtherError = 5,
  kFetchStatusDropped = 6,
  kFetchStatusEmpty = 7
  // Make sure to expand ttl_sec_for_status in HttpCacheFailurePolicy below
  // if adding more values.
};

struct HttpCacheFailurePolicy {
  HttpCacheFailurePolicy();

  // FetchResponseStatus's range from 0 to 7, so we need 8 slots.
  // Adding a length enum would mess with switch exhaustiveness checking
  int ttl_sec_for_status[8];
};

class HttpCacheFailure {
 public:
  // Classify results of a fetch as success, or as particular kind of failure,
  // based on fetch results (including headers and contents).
  //
  // The "is this cacheable" bit is injected from external computation so this
  // code doesn't have to worry about all the various knobs we have for
  // overriding TTL.
  static FetchResponseStatus ClassifyFailure(
      const ResponseHeaders& headers,
      StringPiece contents,
      bool physical_fetch_success, /* e.g. what Done() said */
      bool external_cacheable);

  // Returns true if the given status code is used to remember failure.
  static bool IsFailureCachingStatus(HttpStatus::Code code);

  // These two methods are used to convert between failure status classification
  // and the magic status codes we use to remember such failures in HTTP cache.
  static FetchResponseStatus DecodeFailureCachingStatus(HttpStatus::Code code);
  static HttpStatus::Code EncodeFailureCachingStatus(
      FetchResponseStatus status);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_CACHE_FAILURE_H_
