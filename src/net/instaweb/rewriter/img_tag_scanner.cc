/**
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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/img_tag_scanner.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {

ImgTagScanner::ImgTagScanner(HtmlParse* html_parse)
    : s_img_(html_parse->Intern("img")),
      s_input_(html_parse->Intern("input")),
      s_src_(html_parse->Intern("src")),
      s_type_(html_parse->Intern("type")) {
}

HtmlElement::Attribute*
ImgTagScanner::ParseImgElement(HtmlElement* element) const {
  // Return the src attribute of an <img> tag.
  if (element->tag() == s_img_) {
    return element->FindAttribute(s_src_);
  }
  // Return the src attribute of an <input type="image"> tag.
  // See http://code.google.com/p/modpagespeed/issues/detail?id=86
  if (element->tag() == s_input_) {
    const char* type = element->AttributeValue(s_type_);
    if (type != NULL && strcmp(type, "image") == 0) {
      return element->FindAttribute(s_src_);
    }
  }
  return NULL;
}

}  // namespace net_instaweb
