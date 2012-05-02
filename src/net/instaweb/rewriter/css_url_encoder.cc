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

#include "net/instaweb/rewriter/public/css_url_encoder.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

CssUrlEncoder::~CssUrlEncoder() { }

void CssUrlEncoder::Encode(const StringVector& urls,
                           const ResourceContext* data,
                           GoogleString* rewritten_url) const {
  DCHECK(data != NULL) << "null data passed to CssUrlEncoder::Encode";
  DCHECK_EQ(1U, urls.size());
  if (data != NULL) {
    if (data->attempt_webp()) {
      rewritten_url->append("W.");
    } else if (data->inline_images()) {
      rewritten_url->append("I.");
    } else {
      rewritten_url->append("A.");
    }
  }
  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

// The generic Decode interface is supplied so that
// RewriteSingleResourceFilter and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool CssUrlEncoder::Decode(const StringPiece& encoded,
                           StringVector* urls,
                           ResourceContext* data,
                           MessageHandler* handler) const {
  CHECK(data != NULL);
  if ((encoded.size() < 2) || (encoded[1] != '.')) {
    handler->Message(kError, "Invalid CSS Encoding: %s",
                     encoded.as_string().c_str());
    return false;
  }
  switch (encoded[0]) {
    case 'A':
      data->set_attempt_webp(false);
      data->set_inline_images(false);
      break;
    case 'W':
      data->set_attempt_webp(true);
      data->set_inline_images(true);
      break;
    case 'I':
      data->set_attempt_webp(false);
      data->set_inline_images(true);
      break;
  }

  GoogleString* url = StringVectorAdd(urls);
  StringPiece remaining = encoded.substr(2);
  if (UrlEscaper::DecodeFromUrlSegment(remaining, url)) {
    return true;
  } else {
    urls->pop_back();
    return false;
  }
}

}  // namespace net_instaweb
