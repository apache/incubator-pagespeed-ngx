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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/image_tag_scanner.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlParse;

ImageTagScanner::ImageTagScanner(HtmlParse* html_parse) {
}

HtmlElement::Attribute*
ImageTagScanner::ParseImageElement(HtmlElement* element) const {
  // Return the src attribute of an <img> tag.
  if (element->keyword() == HtmlName::kImg) {
    return element->FindAttribute(HtmlName::kSrc);
  }
  // Return the src attribute of an <input type="image"> tag.
  // See http://code.google.com/p/modpagespeed/issues/detail?id=86
  if (element->keyword() == HtmlName::kInput) {
    const char* type = element->AttributeValue(HtmlName::kType);
    if (type != NULL && strcmp(type, "image") == 0) {
      return element->FindAttribute(HtmlName::kSrc);
    }
  }
  return NULL;
}

}  // namespace net_instaweb
