// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RULE_H_
#define PAGESPEED_CORE_RULE_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "pagespeed/core/input_capabilities.h"
#include "pagespeed/l10n/user_facing_string.h"

namespace pagespeed {

class InputInformation;
class Resource;
class Result;
class ResultProvider;
class RuleFormatter;
class RuleInput;
class RuleResults;

typedef std::vector<const Result*> ResultVector;

/**
 * Lint rule checker interface.
 */
class Rule {
 public:
  explicit Rule(const InputCapabilities& capability_requirements);
  virtual ~Rule();

  // String that should be used to identify this rule during result
  // serialization.
  virtual const char* name() const = 0;

  // Human readable rule name.
  virtual UserFacingString header() const = 0;

  // Required InputCapabilities for this Rule.
  const InputCapabilities& capability_requirements() const {
    return capability_requirements_;
  }

  // Compute results and append it to the results set.
  //
  // @param input Input to process.
  // @param result_provider
  // @return true iff the computation was completed without errors.
  virtual bool AppendResults(const RuleInput& input,
                             ResultProvider* result_provider) = 0;

  // Interpret the results structure and produce a formatted representation.
  //
  // @param results Results to interpret
  // @param formatter Output formatter
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter) = 0;

  // Compute the impact of the rule suggestions.  The result should be a
  // nonnegative number, where zero means there is no room for improvement.
  // The relative scaling of this number should depend upon the
  // ClientCharacteristics object in the provided InputInformation.
  //
  // @param input_info Information about resources that are part of the page.
  // @param results Result vector that contains savings information.
  // @returns nonnegative impact rating
  double ComputeRuleImpact(const InputInformation& input_info,
                           const RuleResults& results);

  // Compute the Rule score from InputInformation and ResultVector.
  //
  // @param input_info Information about resources that are part of the page.
  // @param results Result vector that contains savings information.
  // @returns 0-100 score.
  virtual int ComputeScore(const InputInformation& input_info,
                           const RuleResults& results);

  // Sort the results in their presentation order.
  virtual void SortResultsInPresentationOrder(ResultVector* rule_results) const;

  // Show if the rule is experimental. Return false by default. Any experimental
  // rule must override this method to return true. Experimental rules are new
  // rules that we are trying out, which do not impact the overall score, and
  // are expected to either graduate to non-experimental rules or else be
  // deleted.
  virtual bool IsExperimental() const;

 protected:
  // Compute the impact of a single rule suggestion.  The result should be a
  // nonnegative number, where zero means there is no room for improvement.
  // The relative scaling of this number should depend upon the
  // ClientCharacteristics object in the provided InputInformation.
  //
  // @param input_info Information about resources that are part of the page.
  // @param result Result object that contains savings information.
  // @returns nonnegative impact rating
  virtual double ComputeResultImpact(const InputInformation& input_info,
                                     const Result& result);

 private:
  const InputCapabilities capability_requirements_;

  DISALLOW_COPY_AND_ASSIGN(Rule);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RULE_H_
