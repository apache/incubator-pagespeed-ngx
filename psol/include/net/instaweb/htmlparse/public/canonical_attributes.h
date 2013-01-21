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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_CANONICAL_ATTRIBUTES_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_CANONICAL_ATTRIBUTES_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class HtmlParse;

// Rewrites every attribute-value that can be safely decoded.  This helps us
// determine whether our attribute value parsing is problematic.
class CanonicalAttributes : public EmptyHtmlFilter {
 public:
  explicit CanonicalAttributes(HtmlParse* html_parse);
  virtual ~CanonicalAttributes();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "CanonicalAttributes"; }
  int num_changes() const { return num_changes_; }
  int num_errors() const { return num_errors_; }

 private:
  HtmlParse* html_parse_;
  int num_changes_;
  int num_errors_;

  DISALLOW_COPY_AND_ASSIGN(CanonicalAttributes);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_CANONICAL_ATTRIBUTES_H_
