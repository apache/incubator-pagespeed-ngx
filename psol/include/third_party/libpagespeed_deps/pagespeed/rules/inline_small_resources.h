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

#ifndef PAGESPEED_RULES_INLINE_SMALL_RESOURCES_H_
#define PAGESPEED_RULES_INLINE_SMALL_RESOURCES_H_

#include "base/basictypes.h"
#include "pagespeed/core/resource.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Checks for small external CSS and JS resources that can be inlined
 * in HTML.
 */
class InlineSmallResources : public Rule {
 public:
  explicit InlineSmallResources(ResourceType resource_type);

  // Rule interface.
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  virtual int ComputeScore(const InputInformation& input_info,
                           const RuleResults& results);

 protected:
  virtual bool ComputeMinifiedSize(
      const std::string& body, int* out_minified_size) const = 0;

  virtual int GetTotalResourcesOfSameType(
      const InputInformation& input_info) const = 0;

 private:
  bool IsInlineCandidate(const Resource* resource,
                         const std::string& html_domain);

  const ResourceType resource_type_;
  DISALLOW_COPY_AND_ASSIGN(InlineSmallResources);
};

class InlineSmallJavaScript : public InlineSmallResources {
 public:
  InlineSmallJavaScript();
  virtual const char* name() const;
  virtual UserFacingString header() const;

 protected:
  virtual bool ComputeMinifiedSize(
      const std::string& body, int* out_minified_size) const;

  virtual int GetTotalResourcesOfSameType(
      const InputInformation& input_info) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(InlineSmallJavaScript);
};

class InlineSmallCss : public InlineSmallResources {
 public:
  InlineSmallCss();
  virtual const char* name() const;
  virtual UserFacingString header() const;

 protected:
  virtual bool ComputeMinifiedSize(
      const std::string& body, int* out_minified_size) const;

  virtual int GetTotalResourcesOfSameType(
      const InputInformation& input_info) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(InlineSmallCss);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_INLINE_SMALL_RESOURCES_H_
