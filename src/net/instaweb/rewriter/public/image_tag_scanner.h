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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_TAG_SCANNER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {
class HtmlParse;

class ImageTagScanner {
 public:
  explicit ImageTagScanner(HtmlParse* html_parse);

  // Examine HTML element and determine if it is an img with a src.  If so
  // extract the src attribute and return it, otherwise return NULL.
  HtmlElement::Attribute* ParseImageElement(HtmlElement* element) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTagScanner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_TAG_SCANNER_H_
