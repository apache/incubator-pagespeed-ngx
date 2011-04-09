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

#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/url_escaper.h"

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

UrlMultipartEncoder::~UrlMultipartEncoder() {
}

void UrlMultipartEncoder::Encode(const StringVector& urls,
                                 const ResourceContext* data,
                                 GoogleString* encoding) const {
  DCHECK(data == NULL)
      << "Unexpected non-null data passed to UrlMultipartEncodeer";
  GoogleString buf;

  // Encoding is a two-part process.  First we take the array of
  // URLs and concatenate them together with + signs, escaping
  // any + signs that appear in the URLs themselves.  Since the
  // escape for this encoder is '=' we must escape that too.
  for (int i = 0, n = urls.size(); i < n; ++i) {
    if (i != 0) {
      buf += kSeparator;
    }
    const GoogleString& url = urls[i];
    for (int c = 0, nc = url.size(); c < nc; ++c) {
      char ch = url[c];
      if (ch == kEscape) {
        buf += kEscapedEscape;
      } else if (ch == kSeparator) {
        buf += kEscapedSeparator;
      } else {
        buf += ch;
      }
    }
  }

  // Next we escape the whole blob with restrictions appropriate for URLs.
  UrlEscaper::EncodeToUrlSegment(buf, encoding);
}

bool UrlMultipartEncoder::Decode(const StringPiece& encoding,
                                 StringVector* urls,
                                 ResourceContext* data,
                                 MessageHandler* handler) const {
  GoogleString buf;

  // Reverse the two-step encoding process described above.
  if (!UrlEscaper::DecodeFromUrlSegment(encoding, &buf)) {
    handler->Message(kError,
                     "Invalid escaped URL segment: %s",
                     encoding.as_string().c_str());
    return false;
  }

  urls->clear();
  GoogleString url;
  bool append_last = false;
  for (int c = 0, nc = buf.size(); c < nc; ++c) {
    char ch = buf[c];
    if (ch == kSeparator) {
      urls->push_back(url);
      url.clear();
      append_last = true;
      // ensure that a "a+b+" results in 3 urls with the last one empty.
    } else {
      if (ch == kEscape) {
        ++c;
        if (c == nc) {
          handler->Message(kError,
                           "Invalid encoding: escape at end of string %s",
                           buf.c_str());
          return false;
        }
        ch = buf[c];
        if ((ch != kEscape) && (ch != kSeparator)) {
          handler->Message(kError,
                           "Invalid character `%c', after escape `%c' in %s",
                           ch, kEscape, buf.c_str());
          return false;
        }
      }
      url += ch;
    }
  }
  if (append_last || !url.empty()) {
    urls->push_back(url);
  }
  return true;
}

}  // namespace net_instaweb
