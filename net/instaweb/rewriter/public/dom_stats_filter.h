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

// Author: bharathbhushan@google.com (Bharath Bhushan)

// A filter which does not modify the DOM, but counts statistics about it.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOM_STATS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOM_STATS_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// Counts some basic statistics observed as HTML is parsed.
class DomStatsFilter : public CommonFilter {
 public:
  explicit DomStatsFilter(RewriteDriver* driver);
  virtual ~DomStatsFilter();

  // Clears all state associated with the filter.
  void Clear();

  virtual const char* Name() const { return "Dom Statistics"; }

  int num_img_tags() const { return num_img_tags_; }
  int num_inlined_img_tags() const { return num_inlined_img_tags_; }
  int num_external_css() const { return num_external_css_; }
  int num_scripts() const { return num_scripts_; }
  int num_critical_images_used() const { return num_critical_images_used_; }

 private:
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);

  int num_img_tags_;
  int num_inlined_img_tags_;
  int num_external_css_;
  int num_scripts_;
  int num_critical_images_used_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(DomStatsFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOM_STATS_FILTER_H_
