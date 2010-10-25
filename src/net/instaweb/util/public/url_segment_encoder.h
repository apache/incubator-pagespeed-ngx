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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Abstract class that describes encoding of url segments by rewriters.
// Most instances of this will want to delegate to a UrlEscaper, which
// is itself an instance.
class UrlSegmentEncoder {
 public:
  virtual ~UrlSegmentEncoder();
  // *Append* encoding of url segment "in" to "url_segment".
  virtual void EncodeToUrlSegment(const StringPiece& in,
                                  std::string* url_segment) = 0;
  // Decode url segment from "url_segment", *appending* to "out"; should consume
  // entire StringPiece.  Return false on decode failure.
  virtual bool DecodeFromUrlSegment(const StringPiece& url_segment,
                                    std::string* out) = 0;
 protected:
  UrlSegmentEncoder() { }
 private:
  DISALLOW_COPY_AND_ASSIGN(UrlSegmentEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_
