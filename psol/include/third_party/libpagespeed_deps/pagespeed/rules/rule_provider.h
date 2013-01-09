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

#ifndef PAGESPEED_RULES_RULE_PROVIDER_H_
#define PAGESPEED_RULES_RULE_PROVIDER_H_

#include <string>
#include <vector>
#include "base/basictypes.h"

namespace pagespeed {

class InputCapabilities;
class Rule;

namespace rule_provider {

/**
 * A RuleSet is a collection of rules for a specific purpose (e.g. only useful
 * for older browsers, or experimental).  Every rule should be in exactly one
 * RuleSet.
 */
enum RuleSet {
  CORE_RULES = 0,
  OLD_BROWSER_RULES = 1,
  NEW_BROWSER_RULES = 2,
  MOBILE_BROWSER_RULES = 3,
};

// Special values that allow for iteration over the entire RuleSet
// enum. These values should be kept in sync with the actual first
// and last values in the enum, above.
static const RuleSet kFirstRuleSet = CORE_RULES;
static const RuleSet kLastRuleSet = MOBILE_BROWSER_RULES;

/**
 * Append all the rules in a given RuleSet to the given vector of Rules.
 * Return true if all the rules were instantiated and added.
 */
bool AppendRuleSet(bool save_optimized_content, RuleSet ruleset,
                   std::vector<Rule*>* rules);

/**
 * Create a new Rule object from a rule name (case-insensitive).  If no rule is
 * found with the given name, then return NULL.
 */
Rule* CreateRuleWithName(bool save_optimized_content, const std::string& name);

/**
 * Append the rules with the given names to the given vector of Rules.
 * Return true if all rules were able to be instantiated. Rule names
 * that were unable to be instantiated will be added to
 * nonexistent_rule_names.
 */
bool AppendRulesWithNames(bool save_optimized_content,
                          const std::vector<std::string>& rule_names,
                          std::vector<Rule*>* rules,
                          std::vector<std::string>* nonexistent_rule_names);

/**
 * Remove the rule with the given name from the given vector of Rules (mutating
 * it). Return true and set removed_rule to point to the removed Rule object if
 * the rule was found in the vector.
 * Note: RemoveRuleWithName does *not* delete the removed Rule --- the caller
 * is responsible for deleting the removed rule (returned in removed_rule).
 */
bool RemoveRuleWithName(const std::string& name, std::vector<Rule*>* rules,
                        Rule** removed_rule);

/**
 * Append the canonical set of Page Speed rules, used to generate a
 * Page Speed Score.
 */
void AppendPageSpeedRules(bool save_optimized_content,
                          std::vector<Rule*>* rules);

/**
 * Remove all rules that aren't compatible with the given
 * InputCapabilities.
 */
void RemoveIncompatibleRules(std::vector<Rule*>* rules,
                             std::vector<std::string>* incompatible_rule_names,
                             const pagespeed::InputCapabilities& capabilities);

/**
 * NOTE: Most clients should call AppendPageSpeedRules instead of
 * AppendAllRules. This method may be removed in a future release.
 *
 * Append all Page Speed rules to the given vector of Rule
 * instances. This includes all of the rules returned from
 * AppendPageSpeedRules as well as some rules that have been
 * deprecated from the "Page Speed Score" set of rules.
 */
void AppendAllRules(bool save_optimized_content, std::vector<Rule*>* rules);

/**
 * NOTE: This method will be removed in a future release. Callers
 * should instead build a vector of rules, and then call
 * RemoveIncompatibleRules.
 *
 * Append the Page Speed rules that are compatible with the given
 * InputCapabilities bitfield.
 */
void AppendCompatibleRules(bool save_optimized_content,
                           std::vector<Rule*>* rules,
                           std::vector<std::string>* incompatible_rule_names,
                           const pagespeed::InputCapabilities& capabilities);

}  // namespace rule_provider

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_RULE_PROVIDER_H_
