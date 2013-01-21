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

#ifndef PAGESPEED_RULES_MOBILE_VIEWPORT_H_
#define PAGESPEED_RULES_MOBILE_VIEWPORT_H_

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Check for a <meta name="viewport"> tag for proper sizing on mobile browsers.
 * Does not check the contents of the viewport meta tag, just that it is set.
 *
 * TODO(dbathgate): Check for @viewport CSS selectors when the CSS Device
 *  Adaptation Specification (http://dev.w3.org/csswg/css-device-adapt/)
 *  becomes supported by major mobile browsers.
 */
class MobileViewport : public Rule {
 public:
  MobileViewport();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  // This rule is listed as "experimental" because it represents a best
  // practice for UI usability and speed, rather than a slowing of page
  // loading or rendering.
  virtual bool IsExperimental() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileViewport);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_MOBILE_VIEWPORT_H_
