/*
 * Copyright 2012 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace net_instaweb {

class RewriteDriver;

// Add this filter into the HtmlParse chain to add a base
// tag into the head section of an HTML document.
class BaseTagFilter : public EmptyHtmlFilter {
 public:
  explicit BaseTagFilter(RewriteDriver* driver)
      : added_base_tag_(false),
        driver_(driver) {}

  virtual ~BaseTagFilter();

  virtual void StartDocument() {
    added_base_tag_ = false;
  }
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "BaseTag"; }

 private:
  bool added_base_tag_;
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(BaseTagFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_
