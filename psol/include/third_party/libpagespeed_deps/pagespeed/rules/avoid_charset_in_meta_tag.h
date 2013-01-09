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

#ifndef PAGESPEED_RULES_AVOID_CHARSET_IN_META_TAG_H_
#define PAGESPEED_RULES_AVOID_CHARSET_IN_META_TAG_H_

#include <string>
#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Checks for requests that return an HTML response with a CharSet
 * specified in the meta tag and recommends moving the charset to the
 * Content-Type response header.
 */
class AvoidCharsetInMetaTag : public Rule {
 public:
  AvoidCharsetInMetaTag();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  virtual bool IsExperimental() const;

  // Exposed only for testing.
  static bool HasMetaCharsetTag(
      const std::string& url,
      const std::string& html_body,
      std::string* out_meta_charset_content,
      int* out_meta_charset_begin_line_number);

 private:
  DISALLOW_COPY_AND_ASSIGN(AvoidCharsetInMetaTag);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_AVOID_CHARSET_IN_META_TAG_H_
