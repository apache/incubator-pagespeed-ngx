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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class ResourceTagScanner {
 public:
  explicit ResourceTagScanner(HtmlParse* html_parse) {}

  // Examines an HTML element to determine if it's a link to any sort
  // of resource, extracting out the HREF or SRC.  In this scanner,
  // we don't care about the type of resource; we are just looking for
  // anything that matches the pattern "<script src=...>", "<img src=...>",
  // or "<link rel="stylesheet" href=...>", without worrying about what
  // the other attributes are.
  HtmlElement::Attribute* ScanElement(HtmlElement* element) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceTagScanner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
