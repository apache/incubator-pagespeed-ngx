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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_USAGE_DATA_REPORTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_USAGE_DATA_REPORTER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ContentType;
class GoogleUrl;

// General interface for reporting usage data such as page load time,
// error response codes, various rewriter warnings.
// Default implementation ignores all reports, other implementations may
// do things like aggregate the top 10 most common error URls, etc.
class UsageDataReporter {
 public:
  UsageDataReporter() {}
  virtual ~UsageDataReporter();

  // Reports client-side instrumentation beacon.
  // The mod_pagespeed beacons are of the form:
  //   http://www.example.com/mod_pagespeed_beacon?ets=load:xxx
  // Implementation must parse the URL and extract interesting information.
  virtual void ReportCsiBeacon(const GoogleUrl& url) {}

  // Reports all useful response data.
  virtual void ReportResponseData(const GoogleUrl& url, int32 response_code,
                                  const ContentType* content_type,
                                  int64 time_taken) {}

  // Report a warning.
  virtual void ReportWarning(const GoogleUrl& url, int32 warning_code,
                             const StringPiece& warning_message) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UsageDataReporter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_USAGE_DATA_REPORTER_H_
