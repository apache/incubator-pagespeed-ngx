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

#include "net/instaweb/http/public/content_type.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const ContentType kTypes[] = {
  // Canonical types:
  {"text/html",                     ".html",  ContentType::kHtml},  // RFC 2854
  {"application/xhtml+xml",         ".xhtml", ContentType::kXhtml},  // RFC 3236
  {"application/ce-html+xml",       ".xhtml", ContentType::kCeHtml},

  {"text/javascript",               ".js",   ContentType::kJavascript},
  {"text/css",                      ".css",  ContentType::kCss},
  {"text/plain",                    ".txt",  ContentType::kText},
  {"text/xml",                      ".xml",  ContentType::kXml},  // RFC 3023
  {"image/png",                     ".png",  ContentType::kPng},
  {"image/gif",                     ".gif",  ContentType::kGif},
  {"image/jpeg",                    ".jpg",  ContentType::kJpeg},
  {"application/x-shockwave-flash", ".swf",  ContentType::kSwf},
  {"image/webp",                    ".webp", ContentType::kWebp},

  // Synonyms; Note that the canonical types are referenced by index
  // in the named references declared below.
  {"application/x-javascript", ".js",   ContentType::kJavascript},
  {"application/javascript",   ".js",   ContentType::kJavascript},
  {"text/ecmascript",          ".js",   ContentType::kJavascript},
  {"text/x-js",                ".js",   ContentType::kJavascript},
  {"application/ecmascript",   ".js",   ContentType::kJavascript},
  {"image/jpeg",               ".jpeg", ContentType::kJpeg},
  {"text/html",                ".htm",  ContentType::kHtml},
  {"application/xml",          ".xml",  ContentType::kXml},  // RFC 3023
};
const int kNumTypes = arraysize(kTypes);

}  // namespace

const ContentType& kContentTypeHtml = kTypes[0];
const ContentType& kContentTypeXhtml = kTypes[1];
const ContentType& kContentTypeCeHtml = kTypes[2];

const ContentType& kContentTypeJavascript = kTypes[3];
const ContentType& kContentTypeCss = kTypes[4];
const ContentType& kContentTypeText = kTypes[5];
const ContentType& kContentTypeXml = kTypes[6];

const ContentType& kContentTypePng = kTypes[7];
const ContentType& kContentTypeGif = kTypes[8];
const ContentType& kContentTypeJpeg = kTypes[9];
const ContentType& kContentTypeSwf = kTypes[10];
const ContentType& kContentTypeWebp = kTypes[11];

int ContentType::MaxProducedExtensionLength() {
  return 4;  // .jpeg or .webp
}

bool ContentType::IsHtmlLike() const {
  switch (type_) {
    case kHtml:
    case kXhtml:
    case kCeHtml:
      return true;
    default:
      return false;
  }
}

bool ContentType::IsXmlLike() const {
  switch (type_) {
    case kXhtml:
    case kXml:
      return true;
    default:
      return false;
  }
}

bool ContentType::IsFlash() const {
  switch (type_) {
    case kSwf:
      return true;
    default:
      return false;
  }
}

bool ContentType::IsImage() const {
  switch (type_) {
    case kPng:
    case kGif:
    case kJpeg:
    case kWebp:
      return true;
    default:
      return false;
  }
}

const ContentType* NameExtensionToContentType(const StringPiece& name) {
  // Get the name from the extension.
  StringPiece::size_type ext_pos = name.rfind('.');
  const ContentType* res = NULL;
  if (ext_pos != StringPiece::npos) {
    StringPiece ext = name.substr(ext_pos);
    // TODO(jmarantz): convert to a map if the list gets large.
    for (int i = 0; i < kNumTypes; ++i) {
      if (StringCaseEqual(ext, kTypes[i].file_extension())) {
        res = &kTypes[i];
        break;
      }
    }
  }
  return res;
}

const ContentType* MimeTypeToContentType(const StringPiece& mime_type) {
  // TODO(jmarantz): convert to a map if the list gets large.
  const ContentType* res = NULL;

  // The content-type can have a "; charset=...".  We are not interested
  // in that, for the purpose of our ContentType object.
  //
  // TODO(jmarantz): we should be grabbing the encoding, however, and
  // saving it so that when we emit content-type headers for resources,
  // they include the proper encoding.
  StringPiece stripped_mime_type;
  StringPiece::size_type semi_colon = mime_type.find(';');
  if (semi_colon == StringPiece::npos) {
    stripped_mime_type = mime_type;
  } else {
    stripped_mime_type = mime_type.substr(0, semi_colon);
  }

  for (int i = 0; i < kNumTypes; ++i) {
    if (StringCaseEqual(stripped_mime_type, kTypes[i].mime_type())) {
      res = &kTypes[i];
      break;
    }
  }
  return res;
}

// TODO(nforman): Have some further indication of whether
// content_type_str was just empty or invalid.
bool ParseContentType(const StringPiece& content_type_str,
                      GoogleString* mime_type,
                      GoogleString* charset) {
  StringPiece content_type = content_type_str;
  // Set default values
  mime_type->clear();
  charset->clear();

  if (content_type.empty()) {
    return false;
  }

  // Mime type is in the form: "\w+/\w+ *;(.*;)* *charset *= *\w+"
  StringPieceVector semi_split;
  SplitStringPieceToVector(content_type, ";", &semi_split, false);
  if (semi_split.size() == 0) {
    return false;
  }
  semi_split[0].CopyToString(mime_type);
  for (int i = 1, n = semi_split.size(); i < n; ++i) {
    StringPieceVector eq_split;
    SplitStringPieceToVector(semi_split[i], "=", &eq_split, false);
    if (eq_split.size() == 2) {
      TrimWhitespace(&eq_split[0]);
      if (StringCaseEqual(eq_split[0], "charset")) {
        TrimWhitespace(&eq_split[1]);
        eq_split[1].CopyToString(charset);
        break;
      }
    }
  }

  return !mime_type->empty() || !charset->empty();
}

bool ParseCategory(const StringPiece& category_str,
                   ContentType::Category* category) {
  if (StringCaseEqual("Script", category_str)) {
    *category = ContentType::kScript;
  } else if (StringCaseEqual("Image", category_str)) {
    *category = ContentType::kImage;
  } else if (StringCaseEqual("Stylesheet", category_str)) {
    *category = ContentType::kStylesheet;
  } else if (StringCaseEqual("OtherResource", category_str)) {
    *category = ContentType::kOtherResource;
  } else if (StringCaseEqual("Hyperlink", category_str)) {
    *category = ContentType::kHyperlink;
  } else {
    *category = ContentType::kUndefined;
  }
  return *category != ContentType::kUndefined;
}

}  // namespace net_instaweb
