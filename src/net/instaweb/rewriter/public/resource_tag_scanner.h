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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {
class HtmlParse;

class ResourceTagScanner {
 public:
  explicit ResourceTagScanner(HtmlParse* html_parse)
      : find_a_tags_(false),
        find_form_tags_(false) {
  }

  // Examines an HTML element to determine if it's a link to any sort
  // of resource, extracting out the HREF or SRC.  In this scanner,
  // we don't care about the type of resource; we are just looking for
  // anything that matches the pattern "<script src=...>", "<img src=...>",
  // or "<link rel="stylesheet" href=...>", without worrying about what
  // the other attributes are.
  //
  // Sets *is_hyperlink to true for <a>, <form>, and <area> tags, false
  // otherwise.  These are of interest because we generally want to
  // apply domain-sharding for resources, but not hyperlinks.
  HtmlElement::Attribute* ScanElement(HtmlElement* element,
                                      bool* is_hyperlink) const;

  // Note: set_find_a_tags also finds "area" tags, as those are a different
  // visualization for an anchor.  If we need to split this functionality
  // in the future we add a new bool.
  //
  // TODO(jmarantz): merge these two flags (note url_left_trim_filter.cc only
  // sets one currently) and rename to set_find_hyperlinks.
  void set_find_a_tags(bool val) { find_a_tags_ = val; }
  void set_find_form_tags(bool val) { find_form_tags_ = val; }

 private:
  bool find_a_tags_;
  bool find_form_tags_;
  DISALLOW_COPY_AND_ASSIGN(ResourceTagScanner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
