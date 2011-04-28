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

#include "net/instaweb/util/public/url_segment_encoder.h"

#include "base/logging.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {
class MessageHandler;
class ResourceContext;

UrlSegmentEncoder::~UrlSegmentEncoder() {
}

void UrlSegmentEncoder::Encode(const StringVector& urls,
                               const ResourceContext* data,
                               GoogleString* url_segment) const {
  DCHECK(data == NULL) << "non-null data passed to default SegmentEncoder";
  DCHECK_EQ(1U, urls.size());
  UrlEscaper::EncodeToUrlSegment(urls[0], url_segment);
}

bool UrlSegmentEncoder::Decode(const StringPiece& url_segment,
                               StringVector* urls,
                               ResourceContext* out_data,
                               MessageHandler* handler) const {
  urls->clear();
  urls->push_back(GoogleString());
  GoogleString& url = urls->back();
  return UrlEscaper::DecodeFromUrlSegment(url_segment, &url);
}

}  // namespace net_instaweb
