/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)
//
// A collection of content-types and their attributes.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_SEMANTIC_TYPE_H_
#define NET_INSTAWEB_HTTP_PUBLIC_SEMANTIC_TYPE_H_

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace semantic_type {

// When we see a url, we are pretty sure what kind of resource it points to from
// context.  See resource_tag_scanner.h for the definitions of the categories.
// They are broader categories than ContentType::Type because contextual
// information is limited.  <img src="URL"> may be kPng, kGif, kJpeg, kWebp, or
// another image type.  Another difference is that content type represents the
// actual type we found when we fetched the resource, while the semantic type is
// just what we expect to find when we do.  If the webmaster writes something
// like <img src=song.mp3> and song.mp3 is a css file served with content type
// text/javascript, the semantic type will be kImage, the content type will by
// kJavascript, and we'll ignore the extension (mp3) and actual contents of the
// file (which will look like css).
enum Category {
  kScript,
  kImage,
  kStylesheet,
  kOtherResource,
  kHyperlink,
  kPrefetch,
  kUndefined
};

// Determine the value of the category enum corresponding to the given string.
// Case insensitive. Valid categories are:
//   Script
//   Image
//   Stylesheet
//   OtherResource
//    - This is any other url that will be automatically loaded by the browser
//      along with the main page.  For example, the 'manifest' attribute of the
//      'html' element or the 'src' attribute of an 'iframe' element.
//   Prefetch
//    - This is to prefetch the given url or dns-prefetch for the given domain.
//   Hyperlink
//    - A link to another page or other resource that a browser wouldn't
//      normally load in connection to this page. For example the 'href'
//      attribute of an 'a' element.
bool ParseCategory(const StringPiece& category_str, Category* category);

}  // namespace semantic_type
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_SEMANTIC_TYPE_H_
