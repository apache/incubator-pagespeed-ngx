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

#ifndef PAGESPEED_RULES_SAVINGS_COMPUTER_H_
#define PAGESPEED_RULES_SAVINGS_COMPUTER_H_

#include "base/basictypes.h"

namespace pagespeed {

class Resource;
class Savings;

namespace rules {

/**
 * A SavingsComputer can be used to delegate computation of the
 * Savings for a given rule. This allows us to plug in different
 * savings computation algorithms depending on the requirements of the
 * runtime environment. Note that a SavingsComputer only supports
 * computing savings for a single resource in isolation, which makes
 * it unsuitable for rules that don't have a 1:1 mapping between
 * Resource and Result.
 */
class SavingsComputer {
 public:
  SavingsComputer();
  virtual ~SavingsComputer();

  // @param resource The resource to compute savings for.
  // @param savings The computed savings.
  // @return boolean Whether or not the computation succeeded.
  virtual bool ComputeSavings(const Resource& resource, Savings* savings) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavingsComputer);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_SAVINGS_COMPUTER_H_
