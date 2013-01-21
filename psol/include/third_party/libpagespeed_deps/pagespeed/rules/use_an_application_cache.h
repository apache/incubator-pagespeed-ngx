// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_RULES_USE_AN_APPLICATION_CACHE_H_
#define PAGESPEED_RULES_USE_AN_APPLICATION_CACHE_H_

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Suggest that mobile website to use the HTML 5 application cache.
 */
class UseAnApplicationCache : public Rule {
 public:
  UseAnApplicationCache();

  // Rule interface.
  const virtual char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  // This rule is still experimental. It returns true to indicate that. When
  // this rule graduates to stable, remove this function, and the code in .cc.
  // Let the base class return false by default.
  virtual bool IsExperimental() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(UseAnApplicationCache);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_USE_AN_APPLICATION_CACHE_H_
