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

// Author: sligocki@google.com (Shawn Ligocki)
//
// A collection of content-types and their attributes.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CONTENT_TYPE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CONTENT_TYPE_H_

#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

struct ContentType {
 public:
  const char* mime_type() const { return mime_type_; }
  const char* file_extension() const { return file_extension_; }

  const char* mime_type_;
  const char* file_extension_;
};

extern const ContentType& kContentTypeJavascript;
extern const ContentType& kContentTypeCss;
extern const ContentType& kContentTypeText;

extern const ContentType& kContentTypePng;
extern const ContentType& kContentTypeGif;
extern const ContentType& kContentTypeJpeg;

// Given a name (file or url), see if it has the canonical extension
// corresponding to a particular content type.
const ContentType* NameExtensionToContentType(const StringPiece& name);
const ContentType* MimeTypeToContentType(const StringPiece& mime_type);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CONTENT_TYPE_H_
