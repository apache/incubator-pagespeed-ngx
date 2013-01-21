// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_RULES_AVOID_CSS_IMPORT_H_
#define PAGESPEED_RULES_AVOID_CSS_IMPORT_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Checks for CSS and stylesheets that use @import in a way that
 * breaks download parallelization.
 */
class AvoidCssImport : public Rule {
 public:
  AvoidCssImport();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);

  // These methods are exposed only for unittesting. They should not
  // be called by non-test code.
  static void RemoveComments(const std::string& in, std::string* out);
  static bool IsCssImportLine(const std::string& line, std::string* out_url);

 private:
  bool FindImportedResourceUrls(
      const Resource& resource, std::set<std::string>* imported_urls);

  DISALLOW_COPY_AND_ASSIGN(AvoidCssImport);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_AVOID_CSS_IMPORT_H_
