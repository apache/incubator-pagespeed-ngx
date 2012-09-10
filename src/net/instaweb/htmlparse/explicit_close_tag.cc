/*
 * Copyright 2011 Google Inc.
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

#include "net/instaweb/htmlparse/public/explicit_close_tag.h"
#include "net/instaweb/htmlparse/public/html_element.h"

namespace net_instaweb {

ExplicitCloseTag::~ExplicitCloseTag() {}

void ExplicitCloseTag::EndElement(HtmlElement* element) {
  switch (element->close_style()) {
    case HtmlElement::AUTO_CLOSE:
    case HtmlElement::UNCLOSED:
      element->set_close_style(HtmlElement::EXPLICIT_CLOSE);
      break;
    default:
      break;
  }
}

}  // namespace net_instaweb
