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

#ifndef PAGESPEED_RULES_SPECIFY_A_VARY_ACCEPT_ENCODING_HEADER_H_
#define PAGESPEED_RULES_SPECIFY_A_VARY_ACCEPT_ENCODING_HEADER_H_

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Checks for resources that need a Vary: Accept-Encoding header.
 */
class SpecifyAVaryAcceptEncodingHeader : public Rule {
 public:
  SpecifyAVaryAcceptEncodingHeader();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  virtual int ComputeScore(const InputInformation& input_info,
                           const RuleResults& results);
 protected:
  virtual double ComputeResultImpact(const InputInformation& input_info,
                                     const Result& result);

 private:
  DISALLOW_COPY_AND_ASSIGN(SpecifyAVaryAcceptEncodingHeader);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_SPECIFY_A_VARY_ACCEPT_ENCODING_HEADER_H_
