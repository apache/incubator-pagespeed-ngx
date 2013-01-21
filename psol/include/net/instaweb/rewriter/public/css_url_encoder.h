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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class MessageHandler;
class ResourceContext;

// This class implements the encoding of css urls with optional additional
// dimension metadata.  It prepends characters indicating whether the
// user-agent allows for inlining or webp.  We may need to employ distinct
// CSS files to these types of browsers.  This information is conveyed in
// the ResourceContext.
//   http://..path../W.cssfile...  CSS file optimized for webp-capable browsers.
//   http://..path../I.cssfile...  CSS file optimzed for for non-webp browsers
//                                 that inline.
//   http://..path../A.cssfile...  Archaic browser (ie6+7) does neither.
//
// Note that a legacy CSS URL beginning with W., I., or A. will be
// misinterpreted and will not be fetchable since the Decode function
// will strip off the leading 2 characters.
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

 private:
  DISALLOW_COPY_AND_ASSIGN(CssUrlEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_ENCODER_H_
