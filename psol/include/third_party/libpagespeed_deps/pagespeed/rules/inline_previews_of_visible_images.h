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

#ifndef PAGESPEED_RULES_INLINE_PREVIEWS_OF_VISIBLE_IMAGES_H_
#define PAGESPEED_RULES_INLINE_PREVIEWS_OF_VISIBLE_IMAGES_H_

#include "base/basictypes.h"
#include "pagespeed/core/rule.h"

namespace pagespeed {

namespace rules {

/**
 * Recommends serving low-quality previews of visible images.
 */
class InlinePreviewsOfVisibleImages : public Rule {
 public:
  InlinePreviewsOfVisibleImages();

  // Rule interface.
  virtual const char* name() const;
  virtual UserFacingString header() const;
  virtual bool AppendResults(const RuleInput& input, ResultProvider* provider);
  virtual void FormatResults(const ResultVector& results,
                             RuleFormatter* formatter);
  virtual bool IsExperimental() const;

  // Image size must be greater than or equal to this value in order
  // to be a candidate for inlining a preview. Part of the public
  // interface so tests can reference this value directly.
  static const size_t kMinimumInlineThresholdBytes;

 private:
  DISALLOW_COPY_AND_ASSIGN(InlinePreviewsOfVisibleImages);
};

}  // namespace rules

}  // namespace pagespeed

#endif  // PAGESPEED_RULES_INLINE_PREVIEWS_OF_VISIBLE_IMAGES_H_
