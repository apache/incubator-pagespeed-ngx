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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/message_handler.h"

namespace {

// Ultimately these will be encoded by the URL Escaper so we want to
// stay within legal URL space so we don't blow up.  We'll have to
// see how we like this aesthetically.  We want to stay within legal
// filename space as well so the filenames don't look too ugly.

const char kEscape = '=';            // Nice if this is filename-legal
const char kEscapedEscape[] = "==";
const char kSeparator = '+';
const char kEscapedSeparator[] = "=+";

}  // namespace

namespace net_instaweb {

std::string UrlMultipartEncoder::Encode() {
  std::string encoding;
  for (int i = 0, n = urls_.size(); i <n; ++i) {
    if (i != 0) {
      encoding += kSeparator;
    }
    const std::string& url = urls_[i];
    for (int c = 0, nc = url.size(); c < nc; ++c) {
      char ch = url[c];
      if (ch == kEscape) {
        encoding += kEscapedEscape;
      } else if (ch == kSeparator) {
        encoding += kEscapedSeparator;
      } else {
        encoding += ch;
      }
    }
  }
  return encoding;
}

bool UrlMultipartEncoder::Decode(const StringPiece& encoding,
                                 MessageHandler* handler) {
  urls_.clear();
  std::string url;
  bool append_last = false;
  for (int c = 0, nc = encoding.size(); c < nc; ++c) {
    char ch = encoding[c];
    if (ch == kSeparator) {
      urls_.push_back(url);
      url.clear();
      append_last = true;
      // ensure that a "a+b+" results in 3 urls with the last one empty.
    } else {
      if (ch == kEscape) {
        ++c;
        if (c == nc) {
          handler->Message(kError,
                           "Invalid encoding: escape at end of string %s",
                           encoding.as_string().c_str());
          return false;
        }
        ch = encoding[c];
        if ((ch != kEscape) && (ch != kSeparator)) {
          handler->Message(kError,
                           "Invalid character `%c', after escape `%c' in %s",
                           ch, kEscape, encoding.as_string().c_str());
          return false;
        }
      }
      url += ch;
    }
  }
  if (append_last || !url.empty()) {
    urls_.push_back(url);
  }
  return true;
}

}  // namespace net_instaweb
