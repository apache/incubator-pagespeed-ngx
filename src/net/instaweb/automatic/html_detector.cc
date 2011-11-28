/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich),
//         sligocki@google.com (Shawn Ligocki)
//
// This contains HtmlDetector, which tries to heuristically guess whether
// content a server claims to be HTML actually is HTML (it sometimes isn't).

#include "net/instaweb/automatic/public/html_detector.h"

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

HtmlDetector::HtmlDetector() : already_decided_(false), probable_html_(false) {
}

HtmlDetector::~HtmlDetector() {
}

bool HtmlDetector::ConsiderInput(const StringPiece& data) {
  DCHECK(!already_decided_);

  for (int i = 0, n = data.size(); i < n; ++i) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    switch (c) {
      // Ignore all leading whitespace and byte order markers.
      // See http://en.wikipedia.org/wiki/Byte_order_mark
      // Note: This test allows arbitrary orderings and combinations of the
      // byte order markers, but we do not expect many false positives.
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case 0xef:
      case 0xbb:
      case 0xbf: {
        break;
      }
      // If the first non-whitespace, non-BOM char is <, we are content that
      // this is HTML.
      case '<': {
        already_decided_ = true;
        probable_html_ = true;
        return true;
      }
      // Similarly, if it's something else, it probably isn't.
      default: {
        already_decided_ = true;
        probable_html_ = false;
        return true;
      }
    }
  }

  // Looks like we managed to get entirely whitespace --- buffer it up.
  StrAppend(&buffer_, data);

  return false;
}

void HtmlDetector::ReleaseBuffered(GoogleString* out_buffer) {
  buffer_.swap(*out_buffer);
  buffer_.clear();
}

void HtmlDetector::ForceDecision(bool is_html) {
  DCHECK(!already_decided_);
  already_decided_ = true;
  probable_html_ = is_html;
}

}  // namespace net_instaweb
