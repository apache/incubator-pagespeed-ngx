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
// This contains HtmlDetector, which tries to heuristically detect whether
// content a server claims to be HTML actually is HTML (it sometimes isn't).

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_HTML_DETECTOR_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_HTML_DETECTOR_H_

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// This class tries to heuristically detect whether something that claims to
// HTML is likely to be. For now, it merely looks at whether the first
// non-whitespace/non-BOM character is <.
//
// Typical usage:
// HtmlDetector detect_html_;
//
// if (!detect_html_.already_decided() &&
//     detect_html_.ConsiderInput(data)) {
//   GoogleString buffered;
//   detect_html_.ReleaseBuffered(&buffered);
//   if (detect_html_.probable_html()) {
//      do html-specific bits with buffered
//   } else {
//      do non-html things with buffered
//   }
// }
//
// if (detect_html_.already_decided()) {
//   do appropriate things with data based on detect_html_.probable_html()
// }
class HtmlDetector {
 public:
  HtmlDetector();
  ~HtmlDetector();

  // Processes the data, trying to determine if it's HTML or not. If there is
  // enough evidence to make a decision, returns true.
  //
  // If true is returned, already_decided() will be true as well, and hence
  // probable_html() will be accessible. buffered() will not be changed.
  //
  // If false is returned, data will be accumulated inside buffered().
  //
  // Precondition: !already_decided()
  bool ConsiderInput(const StringPiece& data);

  // Returns true if we have seen enough input to make a guess as to whether
  // it's HTML or not.
  bool already_decided() const { return already_decided_; }

  // Precondition: already_decided() true (or ConsiderInput returning true).
  bool probable_html() const {
    DCHECK(already_decided_);
    return probable_html_;
  }

  // Transfers any data that was buffered by ConsiderInput calls that returned
  // false into *out_buffer. The old value of out_buffer is overwritten, and
  // HtmlDetector's internal buffers are cleared.
  void ReleaseBuffered(GoogleString* out_buffer);

 private:
  GoogleString buffer_;
  bool already_decided_;
  bool probable_html_;  // valid only if already_decided_.

  DISALLOW_COPY_AND_ASSIGN(HtmlDetector);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_HTML_DETECTOR_H_
