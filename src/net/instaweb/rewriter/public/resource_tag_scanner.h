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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/content_type.h"

namespace net_instaweb {
class RewriteDriver;
namespace resource_tag_scanner {
// Examines an HTML element to determine if it's a link to any sort of
// resource, extracting out the HREF, SRC, or other URL attribute and
// identifying it's category.  Because, for example, a LINK tag can be an
// image, stylesheet, or nearly anything else, it's no longer sufficient for
// callers to assume they can figure out the category from the element's
// name.
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
//   OtherNonShardable:
//     <a href=...>
//     <area href=...>
//     <form action=...>
//     <frame src="">
//     <iframe src="">
//     <blockquote cite="">
//     <q cite="">
//     <ins cite="">
//     <del cite="">
//     <button formaction="">
//     <embed src="">
//       - Embeds can be flash, which can contain scripts.
//   OtherShardable:
//     <audio src="">
//     <video src="">
//     <source src="">
//     <track src="">
//     <html manifest=...>
//
// Exceptions:
//
// 1. In a small number of cases an element can have multiple url-valued
//    attributes.  We just take the most common one or the one we can do the
//    most to improve, rather than complicate the interface:
//
//      video: src but not poster
//      frame, iframe, img: src but not longdesc
//      input: src but not formaction
//      body: background but not cite
//
// 2. Because we don't parse the codebase attribute of applet or object elements
//    it's not safe for us to manipulate any of their other url-valued
//    attributes.
//
// 3. The base tag is dealt with elsewhere, but here we skip it.  It's not safe
//    to make any changes to it, so this is safe.  It's also not safe to make
//    changes to <head profile=...> because one use of a profile is as a
//    globally unique name which a browser or other agent recognizes and
//    interprets without acessing, so we skip it too.
//
// 4. While usemap, an attribute of img, input, and object, is technically a
//    url, it always has the form #name or #id, which means there's nothing we
//    can do to improve it and there's no harm in leaving it out.  Also, all
//    three cases would be covered by (1) or (2) above.
//
// These exceptions aside, we should support all url-valued attributes in
// html4 and html5:
//   http://dev.w3.org/html5/spec/section-index.html#attributes-1
//   http://www.w3.org/TR/REC-html40/index/attributes.html
//
//
HtmlElement::Attribute* ScanElement(HtmlElement* element, RewriteDriver* driver,
                                    ContentType::Category* category);
}  // namespace resource_tag_scanner
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_TAG_SCANNER_H_
