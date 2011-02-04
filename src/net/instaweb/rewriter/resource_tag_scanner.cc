/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"

namespace net_instaweb {

// Finds resource references.
ResourceTagScanner::ResourceTagScanner(HtmlParse* html_parse)
    : s_href_(html_parse->Intern("href")),
      s_img_(html_parse->Intern("img")),
      s_link_(html_parse->Intern("link")),
      s_rel_(html_parse->Intern("rel")),
      s_script_(html_parse->Intern("script")),
      s_src_(html_parse->Intern("src")) {
}

HtmlElement::Attribute* ResourceTagScanner::ScanElement(HtmlElement* element) {
  Atom tag = element->tag();
  HtmlElement::Attribute* attr = NULL;
  if (tag == s_link_) {
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage/
    // links.html#linkTypes
    HtmlElement::Attribute* rel_attr = element->FindAttribute(s_rel_);
    if ((rel_attr != NULL) &&
        (strcasecmp(rel_attr->value(), CssTagScanner::kStylesheet) == 0)) {
      attr = element->FindAttribute(s_href_);
    }
  } else if ((tag == s_script_) || (tag == s_img_)) {
    attr = element->FindAttribute(s_src_);
  }
  return attr;
}

}  // namespace net_instaweb
