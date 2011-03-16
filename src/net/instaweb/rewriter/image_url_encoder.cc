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

#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

// decodes decimal int followed by x at start of source, removing them
// from source.  Returns true on success.  This is a utility for the
// decoding of image dimensions.
bool DecodeIntX(StringPiece* in, int *result) {
  *result = 0;
  bool ok = false;
  for (; !in->empty(); in->remove_prefix(1)) {
    // TODO(jmaessen): roll strtol-like functionality for StringPiece in util
    if (!AccumulateDecimalValue((*in)[0], result)) {
      break;
    }
    ok = true;
  }
  if (in->empty()) {
    ok = false;
  } else if ((*in)[0] != 'x') {
    ok = false;
  } else {
    in->remove_prefix(1);
  }
  return ok;
}

}  // namespace

ImageUrlEncoder::~ImageUrlEncoder() { }

void ImageUrlEncoder::Encode(const StringVector& urls,
                             const ResourceContext* data,
                             std::string* rewritten_url) const {
  DCHECK(data != NULL) << "null data passed to ImageUrlEncoder::Encode";
  DCHECK_EQ(1U, urls.size());
  if ((data != NULL) && HasDimensions(*data)) {
    const ImageDim& dims = data->image_tag_dims();
    StrAppend(rewritten_url, IntegerToString(dims.width()), "x",
              IntegerToString(dims.height()));
  }
  rewritten_url->append("x");
  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

bool ImageUrlEncoder::DecodeUrlAndDimensions(const StringPiece& encoded,
                                             ImageDim* image_dims,
                                             std::string* url,
                                             MessageHandler* handler) const {
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(encoded);
  bool ok = true;
  int width, height;
  if (remaining.empty()) {
    handler->Message(kInfo, "Empty Image URL");
    ok = false;
  } else if (remaining[0] == 'x') {
    // No dimensions.
    remaining.remove_prefix(1);
  } else if (DecodeIntX(&remaining, &width) &&
             DecodeIntX(&remaining, &height)) {
    image_dims->set_width(width);
    image_dims->set_height(height);
  } else {
    handler->Message(kInfo, "Invalid Image URL encoding: %s",
                     encoded.as_string().c_str());
    ok = false;
  }

  return (ok && UrlEscaper::DecodeFromUrlSegment(remaining, url));
}

// The generic Decode interface is supplied so that
// RewriteSingleResourceFilter and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool ImageUrlEncoder::Decode(const StringPiece& encoded,
                             StringVector* urls,
                             ResourceContext* data,
                             MessageHandler* handler) const {
  ImageDim dims;
  std::string url;
  bool ret = DecodeUrlAndDimensions(encoded, &dims, &url, handler);
  if (ret) {
    if (HasValidDimensions(dims)) {
      *data->mutable_image_tag_dims() = dims;
    }
    urls->push_back(url);
  }
  return ret;
}

}  // namespace net_instaweb
