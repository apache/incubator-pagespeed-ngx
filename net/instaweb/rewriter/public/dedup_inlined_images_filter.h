/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DEDUP_INLINED_IMAGES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DEDUP_INLINED_IMAGES_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class Statistics;
class Variable;

/*
 * The Dedup Inlined Images filter replaces the second & subsequent occurences
 * a repeated inlined image (data:image/... URL) with JavaScript that loads
 * the image from the first occurence. This requires the first occurence to be
 * tagged with a unique id; if it already has an id then that's used instead.
 */
class DedupInlinedImagesFilter : public CommonFilter {
 public:
  static const unsigned int kMinimumImageCutoff;  // Dont dedup if smaller.

  static const char kDiiInitializer[];  // public for the test harness only.

  // Statistics' names.
  static const char kCandidatesFound[];     // No. of unique inlined images.
  static const char kCandidatesReplaced[];  // No. of those replaced with JS.

  explicit DedupInlinedImagesFilter(RewriteDriver* driver);
  virtual ~DedupInlinedImagesFilter();

  // May be called multiple times, if there are multiple statistics objects.
  static void InitStats(Statistics* statistics);

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  virtual const char* Name() const { return "DedupInlinedImages"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  bool IsDedupCandidate(HtmlElement* element, StringPiece* src_iff_true);
  void InsertOurScriptElement(HtmlElement* before);

  bool script_inserted_;  // Have we inserted the script of utility functions?
  StringStringMap hash_to_id_map_;  // The map from data URL content hash to id.
  int snippet_id_;  // Monotonically increasing id for JS snippets we insert.

  // # of times an inlined image was found.
  Variable* num_dedup_inlined_images_candidates_found_;
  // # of times an inlined image was replaced with JS.
  Variable* num_dedup_inlined_images_candidates_replaced_;

  DISALLOW_COPY_AND_ASSIGN(DedupInlinedImagesFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DEDUP_INLINED_IMAGES_FILTER_H_
