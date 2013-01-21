// Copyright 2012 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_FILTERS_LANDING_PAGE_REDIRECTION_FILTER_H_
#define PAGESPEED_FILTERS_LANDING_PAGE_REDIRECTION_FILTER_H_

#include "pagespeed/core/engine.h"

namespace pagespeed {

class Result;

// This result filter removes the landing page redirection result in a
// redirection chain less than kDefaultThresholdRedirectionCount and matching
// any of the following conditions:
//  * cacheable and redirects URL from one host to another, or
//  * login URL, or
//  * callback URL (i.e., captcha).
// We expect this filter to be used in those cases where we accept that a
// redirection is a better choice than alternatives (e.g. should be used when
// analyzing pages where the URL was provided by a user and thus may redirect
// from foo.com -> www.foo.com).
class LandingPageRedirectionFilter : public ResultFilter {
 public:
  // The default allowed redirection count.
  static const int kDefaultThresholdRedirectionCount = 2;

  // Construct a LandingPageRedirectionFilter with the given
  // threshold. Results that have a cacheable redirection count less than
  // the specified threshold will not be accepted.
  explicit LandingPageRedirectionFilter(int threshold);

  // Construct a LandingPageRedirectionFilter with the default threshold.
  LandingPageRedirectionFilter();
  virtual ~LandingPageRedirectionFilter();

  virtual bool IsAccepted(const Result& result) const;

 private:
  int redirection_count_threshold_;

  DISALLOW_COPY_AND_ASSIGN(LandingPageRedirectionFilter);
};
}  // namesapce pagespeed

#endif  // PAGESPEED_FILTERS_LANDING_PAGE_REDIRECTION_FILTER_H_
