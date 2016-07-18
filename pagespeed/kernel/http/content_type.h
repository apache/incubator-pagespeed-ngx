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

// Author: sligocki@google.com (Shawn Ligocki)
//
// A collection of content-types and their attributes.

#ifndef PAGESPEED_KERNEL_HTTP_CONTENT_TYPE_H_
#define PAGESPEED_KERNEL_HTTP_CONTENT_TYPE_H_

#include <set>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct ContentType {
 public:
  // The MIME types we process.
  enum Type {
    kHtml,
    kXhtml,
    kCeHtml,  // See http://en.wikipedia.org/wiki/CE-HTML
    kJavascript,
    kCss,
    kText,
    kXml,
    kPng,
    kGif,
    kJpeg,
    kSwf,
    kWebp,
    kIco,
    kJson,
    kSourceMap,
    kPdf,
    kVideo,
    kAudio,
    kOctetStream,  // Binary resources.
    kOther,  // Used to specify a new local ContentType in one test file.
  };

  // Returns the maximum extension length of any resource types our filters
  // can create. Does not count the ".".
  // See RewriteDriver::CreateOutputResourceWithPath()
  static int MaxProducedExtensionLength();

  const char* mime_type() const { return mime_type_; }
  // TODO(sligocki): Stop returning '.' in file_extension().
  const char* file_extension() const { return file_extension_; }
  Type type() const { return type_; }

  // Return true iff this content type is CSS.
  bool IsCss() const;

  // Return true iff this content type is JS, or something similar like JSON.
  bool IsJsLike() const;

  // Return true iff this content type is HTML, or XHTML, or some other such
  // thing (e.g. CE-HTML) that we can rewrite.
  bool IsHtmlLike() const;

  // Return true iff this content type is XML of some kind (either XHTML or
  // some other XML).
  bool IsXmlLike() const;

  // Return true iff this content type is Flash.
  bool IsFlash() const;

  // Return true iff this content type is Image.
  bool IsImage() const;

  // Return true iff this content type is Video.
  bool IsVideo() const;

  // Return true iff this content type is Audio.
  bool IsAudio() const;

  // Heuristic to determine whether this should be treated as a static resource.
  bool IsLikelyStaticResource() const;

  // Heuristic to determine whether compressing the resource is beneficial.
  bool IsCompressible() const;

  // These fields should be private; we leave them public only so we can use
  // struct literals in content_type.cc.  Other code should use the above
  // accessor methods instead of accessing these fields directly.
  const char* mime_type_;
  const char* file_extension_;  // includes ".", e.g. ".ext"
  Type type_;
};

// HTML-like (i.e. rewritable) text:
extern const ContentType& kContentTypeHtml;
extern const ContentType& kContentTypeXhtml;
extern const ContentType& kContentTypeCeHtml;
// Other text:
extern const ContentType& kContentTypeJavascript;
extern const ContentType& kContentTypeCss;
extern const ContentType& kContentTypeText;
extern const ContentType& kContentTypeXml;
extern const ContentType& kContentTypeJson;
extern const ContentType& kContentTypeSourceMap;
// Images:
extern const ContentType& kContentTypePng;
extern const ContentType& kContentTypeGif;
extern const ContentType& kContentTypeJpeg;
extern const ContentType& kContentTypeSwf;
extern const ContentType& kContentTypeWebp;
extern const ContentType& kContentTypeIco;
// PDF:
extern const ContentType& kContentTypePdf;

// Binary/octet-stream.
extern const ContentType& kContentTypeBinaryOctetStream;

// Given a name (file or url), see if it has the canonical extension
// corresponding to a particular content type.
const ContentType* NameExtensionToContentType(const StringPiece& name);
const ContentType* MimeTypeToContentType(const StringPiece& mime_type);

// Extracts mime_type and charset from a string of the form
// "<mime_type>; charset=<charset>".
// If mime_type or charset is not specified, they will be populated
// with the empty string.
// Returns true if either a mime_type or a charset was extracted.
bool ParseContentType(const StringPiece& content_type_str,
                      GoogleString* mime_type,
                      GoogleString* charset);

// Splits comma-separated string to elements and tries to match each one with
// a recognized content type. The out set will be cleared first and must be
// present.
void MimeTypeListToContentTypeSet(
    const GoogleString& in,
    std::set<const ContentType*>* out);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_CONTENT_TYPE_H_
