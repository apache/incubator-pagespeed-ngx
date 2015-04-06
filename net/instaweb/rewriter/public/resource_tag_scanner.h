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

// Authors: jmarantz@google.com (Joshua Marantz)
//          jefftk@google.com (Jeff Kaufman)
#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_

#include <cstddef>  // for NULL
#include <vector>

#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

class RewriteOptions;

namespace resource_tag_scanner {

struct UrlCategoryPair {
  HtmlElement::Attribute* url;
  semantic_type::Category category;

  UrlCategoryPair()
      : url(NULL),
        category(semantic_type::kUndefined) {}
};

typedef std::vector<UrlCategoryPair> UrlCategoryVector;

// If the attribute is url-valued, determine it's semantic category.  Return
// kUndefined otherwise.
//
// Supported patterns are:
//   Scripts:
//     <script src=...>
//   Stylesheets:
//     <link rel="stylesheet" href=...>
//   Images:
//     <img src=...>,
//     <link rel="icon" href=...>
//     <link rel="apple-touch-icon" href=...>
//     <link rel="apple-touch-icon-precomposed" href=...>
//     <link rel="apple-touch-startup-image" href=...>
//     <body background=...>
//     <td background=...>
//     <th background=...>
//     <table background=...>
//     <thead background=...>
//     <tbody background=...>
//     <tfoot background=...>
//     <input type="image" src=...>
//     <command icon=...>
//     <video poster=...>
//   Hyperlink:
//     <a href=...>
//     <area href=...>
//     <form action=...>
//     <blockquote cite=...>
//     <body cite=...>
//     <q cite=...>
//     <ins cite=...>
//     <del cite=...>
//     <button formaction=...>
//     <input formaction=...>
//     <frame longdesc=...>
//     <iframe longdesc=...>
//     <img longdesc=...>
//   OtherResource:
//     <audio src=...>
//     <video src=...>
//     <source src=...>
//     <track src=...>
//     <html manifest=...>
//     <embed src=...>
//     <frame src=...>
//     <iframe src=...>
//
// Exceptions:
//
// 1. Because we don't parse the codebase attribute of applet or object elements
//    it's not safe for us to manipulate any of their other url-valued
//    attributes.
//
// 2. The base tag is dealt with elsewhere, but here we skip it.  It's not safe
//    to make any changes to it, so this is safe.  It's also not safe to make
//    changes to <head profile=...> because one use of a profile is as a
//    globally unique name which a browser or other agent recognizes and
//    interprets without accessing, so we skip it too.
//
// 3. While usemap, an attribute of img, input, and object, is technically a
//    url, it always has the form #name or #id, which means there's nothing we
//    can do to improve it and there's no harm in leaving it out.
//
// These exceptions aside, we should support all url-valued attributes in
// html4 and html5:
//   http://dev.w3.org/html5/spec/section-index.html#attributes-1
//   http://www.w3.org/TR/REC-html40/index/attributes.html
//
semantic_type::Category CategorizeAttribute(
    const HtmlElement* element,
    const HtmlElement::Attribute* attribute,
    const RewriteOptions* options);

// Examines an HTML element to determine if it's a link to any sort of resource,
// extracting out the HREF, SRC, or other URL attributes and identifying their
// categories.  Because, for example, a LINK tag can be an image, stylesheet, or
// nearly anything else, it's not sufficient for callers to assume they can
// figure out the category from the element's name.
//
// See CategorizeAttribute for the meaning of "url-valued attribute".
//
// Attributes that we can't decode, such as non-ascii urls, will be skipped.
//
// Attributes are returned in left-to-right order.
void ScanElement(HtmlElement* element, const RewriteOptions* options,
                 UrlCategoryVector* attributes);

}  // namespace resource_tag_scanner
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
