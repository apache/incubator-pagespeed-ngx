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

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

// Decodes decimal int followed by x or w at start of source, removing them
// from source.  Returns true on success.  This is a utility for the
// decoding of image dimensions.
// The separator character (x or w) is stored in sep if the parse succeeds.
bool DecodeIntXW(StringPiece* in, int* result, char* sep) {
  *result = 0;
  bool ok = false;
  char curr_char = '\0';
  for (; !in->empty(); in->remove_prefix(1)) {
    // TODO(jmaessen): roll strtol-like functionality for StringPiece in util
    curr_char = (*in)[0];
    if (!AccumulateDecimalValue(curr_char, result)) {
      break;
    }
    ok = true;
  }
  // If we get here, either curr_char is a non-digit, or in->empty().  In the
  // latter case, curr_char is the last char of in (a digit) or '\0', and we
  // fall through the next test and fail.
  if (curr_char != 'x' && curr_char != 'w') {
    ok = false;
  } else {
    *sep = curr_char;
    in->remove_prefix(1);
  }
  return ok;
}

}  // namespace

ImageUrlEncoder::~ImageUrlEncoder() { }

void ImageUrlEncoder::Encode(const StringVector& urls,
                             const ResourceContext* data,
                             GoogleString* rewritten_url) const {
  DCHECK(data != NULL) << "null data passed to ImageUrlEncoder::Encode";
  DCHECK_EQ(1U, urls.size());
  if (data != NULL) {
    if (HasDimensions(*data)) {
      const ImageDim& dims = data->image_tag_dims();
      StrAppend(rewritten_url, IntegerToString(dims.width()), "x",
                IntegerToString(dims.height()));
    }
    if (data->attempt_webp()) {
      rewritten_url->append("w");
    } else {
      rewritten_url->append("x");
    }
  }
  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

// The generic Decode interface is supplied so that
// RewriteSingleResourceFilter and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool ImageUrlEncoder::Decode(const StringPiece& encoded,
                             StringVector* urls,
                             ResourceContext* data,
                             MessageHandler* handler) const {
  ImageDim* dims = data->mutable_image_tag_dims();
  DCHECK(dims != NULL);  // TODO(jmaessen): sanity check on my api understanding
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(encoded);
  int width, height;
  char sep = 'x';
  if (remaining.empty()) {
    handler->Message(kInfo, "Empty Image URL");
    return false;
  } else if (remaining[0] == 'x' || remaining[0] == 'w') {
    // No dimensions.
    sep = remaining[0];
    remaining.remove_prefix(1);
  } else if (DecodeIntXW(&remaining, &width, &sep) && (sep == 'x') &&
             DecodeIntXW(&remaining, &height, &sep)) {
    dims->set_width(width);
    dims->set_height(height);
  } else {
    handler->Message(kInfo, "Invalid Image URL encoding: %s",
                     encoded.as_string().c_str());
    return false;
  }
  DCHECK(sep == 'w' || sep == 'x');
  if (sep == 'w') {
    data->set_attempt_webp(true);
  }

  // Avoid even more string copies by decoding url directly into urls->back().
  GoogleString empty;
  urls->push_back(empty);
  GoogleString& url = urls->back();
  if (UrlEscaper::DecodeFromUrlSegment(remaining, &url)) {
    return true;
  } else {
    urls->pop_back();
    return false;
  }
}

}  // namespace net_instaweb
