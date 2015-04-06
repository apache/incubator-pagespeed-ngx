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
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_ENCODER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_ENCODER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/url_segment_encoder.h"

namespace net_instaweb {

class RequestProperties;
class MessageHandler;
class ResourceContext;

// This class implements the encoding of css urls with optional additional
// dimension metadata. For the legacy encoding, it used to prepend characters
// indicating whether the user-agent allows for inlining or webp. We may need
// to employ distinct CSS files for these types of browsers.  This information
// is conveyed in the ResourceContext.
//   http://..path../W.cssfile...  CSS file optimized for webp-capable browsers.
//   http://..path../I.cssfile...  CSS file optimzed for for non-webp browsers
//                                 that inline.
//   http://..path../A.cssfile...  Archaic browser (ie6+7) does neither.
//
// Note that a legacy CSS URL beginning with W., I., or A. will be
// misinterpreted and will not be fetchable since the Decode function
// will strip off the leading 2 characters.
//
// Note that a lot of this is legacy encoding now, and that we just
// unconditionally use the "A." encoding and rely on content hash and
// metadata cache + user-agent sniffing to keep things consistent.
class CssUrlEncoder : public UrlSegmentEncoder {
 public:
  CssUrlEncoder() {}
  virtual ~CssUrlEncoder();

  virtual void Encode(const StringVector& urls,
                      const ResourceContext* encoding,
                      GoogleString* rewritten_url) const;

  virtual bool Decode(const StringPiece& url_segment,
                      StringVector* urls,
                      ResourceContext* dim,
                      MessageHandler* handler) const;

  // Sets Inlining of image according to the user agent.
  static void SetInliningImages(const RequestProperties& request_properties,
                                ResourceContext* resource_context);

 private:
  DISALLOW_COPY_AND_ASSIGN(CssUrlEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_ENCODER_H_
