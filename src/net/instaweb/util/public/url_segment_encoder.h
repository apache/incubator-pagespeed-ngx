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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class ResourceContext;

// Base class that describes encoding of url segments by rewriters.
// Most instances of this will want to delegate to UrlEscaper.
class UrlSegmentEncoder {
 public:
  UrlSegmentEncoder() { }
  virtual ~UrlSegmentEncoder();

  // Encodes arbitrary text so it can be used in a url segment.  A
  // url segment must contain only characters that are legal in URLs,
  // and exclude "/" and "." which are used for a higher level encoding
  // scheme into which this must fit.
  //
  // 'data' is optional -- it can be NULL and it is up to the encoder to
  // decide what to do.
  virtual void Encode(const StringVector& urls,  const ResourceContext* data,
                      GoogleString* url_segment) const;

  // Decode URLs from "url_segment".  Note that there may be other
  // meta-data encoded in url_segment, which this function will write
  // into out_data, if present.
  virtual bool Decode(const StringPiece& url_segment,
                      StringVector* urls, ResourceContext* out_data,
                      MessageHandler* handler) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlSegmentEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_URL_SEGMENT_ENCODER_H_
