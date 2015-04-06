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

#include "pagespeed/kernel/http/content_type.h"

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

const ContentType kTypes[] = {
  // Canonical types:
  {"text/html",                     ".html",  ContentType::kHtml},  // RFC 2854
  {"application/xhtml+xml",         ".xhtml", ContentType::kXhtml},  // RFC 3236
  {"application/ce-html+xml",       ".xhtml", ContentType::kCeHtml},

  // RFC 4329 defines application/javascript as canonical for JavaScript.
  // text/javascript can break firewall gzipping.
  {"application/javascript",        ".js",   ContentType::kJavascript},
  {"text/css",                      ".css",  ContentType::kCss},
  {"text/plain",                    ".txt",  ContentType::kText},
  {"text/xml",                      ".xml",  ContentType::kXml},  // RFC 3023
  {"image/png",                     ".png",  ContentType::kPng},
  {"image/gif",                     ".gif",  ContentType::kGif},
  {"image/jpeg",                    ".jpg",  ContentType::kJpeg},
  {"application/x-shockwave-flash", ".swf",  ContentType::kSwf},
  {"image/webp",                    ".webp", ContentType::kWebp},
  // While the official MIME type is image/vnd.microsoft.icon, old IE browsers
  // will not accept that type, so we use portable image/x-icon as canonical.
  {"image/x-icon",                  ".ico",  ContentType::kIco},
  {"application/javascript",        ".json", ContentType::kJson},
  {"application/javascript",        ".map",  ContentType::kSourceMap},
  {"application/pdf",               ".pdf",  ContentType::kPdf},  // RFC 3778
  {"application/octet-stream",      ".bin",  ContentType::kOctetStream },

  // Synonyms; Note that the canonical types above are referenced by index
  // in the named references declared below.  The synonyms below are not
  // index-sensitive.
  {"application/x-javascript", ".js",   ContentType::kJavascript},
  {"text/javascript",          ".js",   ContentType::kJavascript},
  {"text/x-javascript",        ".js",   ContentType::kJavascript},
  {"text/ecmascript",          ".js",   ContentType::kJavascript},
  {"text/js",                  ".js",   ContentType::kJavascript},
  {"text/jscript",             ".js",   ContentType::kJavascript},
  {"text/x-js",                ".js",   ContentType::kJavascript},
  {"application/ecmascript",   ".js",   ContentType::kJavascript},
  {"application/json",         ".json", ContentType::kJson},
  {"application/x-json",       ".json", ContentType::kJson},
  {"image/jpeg",               ".jpeg", ContentType::kJpeg},
  {"image/jpg",                ".jpg",  ContentType::kJpeg},
  {"image/vnd.microsoft.icon", ".ico",  ContentType::kIco},
  {"text/html",                ".htm",  ContentType::kHtml},
  {"application/xml",          ".xml",  ContentType::kXml},  // RFC 3023

  {"video/mpeg",                ".mpg",  ContentType::kVideo},  // RFC 2045
  {"video/mp4",                 ".mp4",  ContentType::kVideo},  // RFC 4337
  {"video/3gp",                 ".3gp",  ContentType::kVideo},
  {"video/x-flv",               ".flv",  ContentType::kVideo},
  {"video/ogg",                 ".ogg",  ContentType::kVideo},  // RFC 5334
  {"video/webm",                ".webm", ContentType::kVideo},
  {"video/x-ms-asf",            ".asf",  ContentType::kVideo},
  {"video/x-ms-wmv",            ".wmv",  ContentType::kVideo},
  {"video/quicktime",           ".mov",  ContentType::kVideo},
  {"video/mpeg4",               ".mp4",  ContentType::kVideo},

  {"audio/mpeg",               ".mp3",  ContentType::kAudio},
  {"audio/ogg",                ".ogg",  ContentType::kAudio},
  {"audio/webm",               ".webm", ContentType::kAudio},
  {"audio/mp4",                ".mp4",  ContentType::kAudio},
  {"audio/x-mpeg",             ".mp3",  ContentType::kAudio},
  {"audio/x-wav",              ".wav",  ContentType::kAudio},
  {"audio/mp3",                ".mp3",  ContentType::kAudio},
  {"audio/wav",                ".wav",  ContentType::kAudio},

  {"binary/octet-stream",       ".bin", ContentType::kOctetStream },
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
const ContentType& kContentTypeIco = kTypes[12];

const ContentType& kContentTypeJson = kTypes[13];
const ContentType& kContentTypeSourceMap = kTypes[14];

const ContentType& kContentTypePdf = kTypes[15];

const ContentType& kContentTypeBinaryOctetStream = kTypes[16];

int ContentType::MaxProducedExtensionLength() {
  return 4;  // .jpeg or .webp
}

bool ContentType::IsCss() const {
  return type_ == kCss;
}

bool ContentType::IsJs() const {
  switch (type_) {
    case kJavascript:
    case kJson:
      return true;
    default:
      return false;
  }
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

bool ContentType::IsVideo() const {
  return type_ == kVideo;
}

bool ContentType::IsAudio() const {
  return type_ == kAudio;
}

const ContentType* NameExtensionToContentType(const StringPiece& name) {
  // Get the name from the extension.
  StringPiece::size_type ext_pos = name.rfind('.');
  if (ext_pos != StringPiece::npos) {
    StringPiece ext = name.substr(ext_pos);
    // TODO(jmarantz): convert to a map if the list gets large.
    for (int i = 0; i < kNumTypes; ++i) {
      if (StringCaseEqual(ext, kTypes[i].file_extension())) {
        return &kTypes[i];
      }
    }
  }
  return NULL;
}

const ContentType* MimeTypeToContentType(const StringPiece& mime_type) {
  // TODO(jmarantz): convert to a map if the list gets large.

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
      return &kTypes[i];
    }
  }
  return NULL;
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

void MimeTypeListToContentTypeSet(
    const GoogleString& in,
    std::set<const ContentType*>* out) {
  CHECK(out != NULL) << "'out' is a required parameter.";
  out->clear();
  if (in.empty()) {
    return;
  }
  StringPieceVector strings;
  SplitStringPieceToVector(in, ",", &strings, true /* omit_empty */);
  for (StringPieceVector::const_iterator i = strings.begin(), e = strings.end();
           i != e; ++i) {
    const ContentType* ct = MimeTypeToContentType(*i);
    if (ct == NULL) {
      LOG(WARNING) << "'" << *i << "' is not a recognized mime-type.";
    } else {
      VLOG(1) << "Adding '" << *i << "' to the content-type set.";
      out->insert(ct);
    }
  }
}

bool ContentType::IsLikelyStaticResource() const {
  switch (type_) {
    case kCeHtml:
    case kHtml:
    case kJson:
    case kSourceMap:
    case kOctetStream:
    case kOther:
    case kText:
    case kXhtml:
    case kXml:
      return false;
    case kCss:
    case kGif:
    case kIco:
    case kJavascript:
    case kJpeg:
    case kPdf:
    case kPng:
    case kSwf:
    case kVideo:
    case kAudio:
    case kWebp:
      return true;
  };
  LOG(DFATAL) << "Unexpected content type: " << type_;
  return false;
}

}  // namespace net_instaweb
