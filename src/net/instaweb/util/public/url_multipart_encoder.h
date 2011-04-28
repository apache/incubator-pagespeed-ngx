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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class ResourceContext;
class MessageHandler;

// Encodes a multiple strings into a single string so that it
// can be decoded.  This is not restricted to URLs but is optimized
// for them in its choice of escape characters.  '+' is used to
// separate the parts, and any parts that include '+' are prefixed
// by a '='.  '=' is converted to '==' -- it's a pretty lightweight
// encoding, and any other character restrictions will have to be
// applied to the output of this class.
//
// TODO(jmarantz): One possibly improvement is to bake this
// functionality into UrlEscaper, changing its interface to accept
// arbitrary numbers of pieces in & out.  However, that would change
// an interface that's used in multiple places, so this is left as
// a TODO.
class UrlMultipartEncoder : public UrlSegmentEncoder {
 public:
  UrlMultipartEncoder() {}
  virtual ~UrlMultipartEncoder();

  virtual void Encode(const StringVector& urls, const ResourceContext* data,
                      GoogleString* encoding) const;

  virtual bool Decode(const StringPiece& url_segment,
                      StringVector* urls,
                      ResourceContext* data,
                      MessageHandler* handler) const;

 private:
  StringVector urls_;

  DISALLOW_COPY_AND_ASSIGN(UrlMultipartEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_
