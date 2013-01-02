// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_RULES_AVOID_LANDING_PAGE_REDIRECTS_H_
#define PAGESPEED_RULES_AVOID_LANDING_PAGE_REDIRECTS_H_

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/** Checks for HTTP redirections at landing page . Each redirection adds an RTT
 * to the page load, and before the final landing page is requested, nothing is
 * shown for the client. In many cases, redirections can be eliminated without
 * changing the function of a page.
 */
class AvoidLandingPageRedirects: public Rule {
 public:
  AvoidLandingPageRedirects();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  // Sort the result in redirection chain order.
  virtual void SortResultsInPresentationOrder(ResultVector* results) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AvoidLandingPageRedirects);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_AVOID_LANDING_PAGE_REDIRECTS_H_
