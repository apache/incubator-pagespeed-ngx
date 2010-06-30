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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"

namespace net_instaweb {

// Finds resource references.
ResourceTagScanner::ResourceTagScanner(HtmlParse* html_parse)
    : css_tag_scanner_(html_parse),
      img_tag_scanner_(html_parse),
      script_tag_scanner_(html_parse) {
}

HtmlElement::Attribute* ResourceTagScanner::ScanElement(HtmlElement* element) {
  HtmlElement::Attribute* attr = NULL;
  const char* media;
  if (!css_tag_scanner_.ParseCssElement(element, &attr, &media)) {
    attr = img_tag_scanner_.ParseImgElement(element);
    if (attr == NULL) {
      attr = script_tag_scanner_.ParseScriptElement(element);
    }
  }
  return attr;
}

}  // namespace net_instaweb
