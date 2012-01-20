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
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

const char kCodeSeparator = 'x';
const char kCodeWebp = 'w';
const char kCodeMobileUserAgent = 'm';
const char kMissingDimension = 'N';

const int kNoDimension = -1;

bool IsValidCode(char code) {
  return (code == kCodeSeparator) || (code == kCodeWebp) ||
      (code == kCodeMobileUserAgent);
}

// Decodes a single dimension (either N or an integer), removing it from *in and
// ensuring at least one character remains.  Returns true on success.  When N is
// seen, result is set to kNoDimension, otherwise it's set to the decoded
// dimension.  If decoding fails, result may be written.
// Ensures that *in contains at least one character on exit.
bool DecodeDimension(StringPiece* in, int* result) {
  DCHECK(in->size() >= 2);
  if ((*in)[0] == kMissingDimension) {
    // Dimension is absent.
    in->remove_prefix(1);
    *result = kNoDimension;
    return true;
  }
  bool ok = false;
  *result = 0;
  while (in->size() >= 2 && AccumulateDecimalValue((*in)[0], result)) {
    in->remove_prefix(1);
    ok = true;
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
    if (HasDimension(*data)) {
      const ImageDim& dims = data->image_tag_dims();
      if (dims.has_width()) {
        rewritten_url->append(IntegerToString(dims.width()));
      } else {
        rewritten_url->append(1, kMissingDimension);
      }
      if (dims.has_height()) {
        StrAppend(rewritten_url,
                  StringPiece(&kCodeSeparator, 1),
                  IntegerToString(dims.height()));
      } else {
        StrAppend(rewritten_url,
                  StringPiece(&kCodeSeparator, 1),
                  StringPiece(&kMissingDimension, 1));
      }
    }
    if (data->mobile_user_agent()) {
      rewritten_url->append(1, kCodeMobileUserAgent);
    }
    if (data->attempt_webp()) {
      rewritten_url->append(1, kCodeWebp);
    } else {
      rewritten_url->append(1, kCodeSeparator);
    }
  }
  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

namespace {

// Stateless helper function for ImageUrlEncoder::Decode.
// Removes read dimensions from remaining, sets dims and returns true if
// dimensions are correctly parsed, returns false and leaves dims untouched on
// parse failure.
bool DecodeImageDimensions(StringPiece* remaining, ImageDim* dims) {
  if (remaining->size() < 4) {
    // url too short to hold dimensions.
    return false;
  }
  int width, height;
  if (!DecodeDimension(remaining, &width) ||  // Parse the width
      (*remaining)[0] != kCodeSeparator) {        // And check the separator
    return false;
  }
  // Consume the separator
  remaining->remove_prefix(1);
  if (remaining->size() < 2 ||
      !DecodeDimension(remaining, &height)) {  // Parse the height
    return false;
  }
  if (!IsValidCode((*remaining)[0])) {  // And check the terminator
    return false;
  }
  // Parsed successfully.
  // Now store the dimensions that were present.
  if (width != kNoDimension) {
    dims->set_width(width);
  }
  if (height != kNoDimension) {
    dims->set_height(height);
  } else if (width == kNoDimension) {
    // Both dimensions are missing!  NxN[xw] is not allowed, as it's ambiguous
    // with the shorter encoding.  We should never get here in real life.
    return false;
  }
  return true;
}

}  // namespace

// The generic Decode interface is supplied so that
// RewriteContext and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool ImageUrlEncoder::Decode(const StringPiece& encoded,
                             StringVector* urls,
                             ResourceContext* data,
                             MessageHandler* handler) const {
  if (encoded.empty()) {
    return false;
  }
  ImageDim* dims = data->mutable_image_tag_dims();
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(encoded);
  char terminator = remaining[0];
  if (IsValidCode(terminator)) {
    // No dimensions.  x... or w... or mx... or mw...
    // Do nothing.
  } else if (DecodeImageDimensions(&remaining, dims)) {
    // We've parsed the dimensions and they've been stripped from remaining.
    // Now set terminator properly.
    terminator = remaining[0];
  } else {
    return false;
  }
  // Remove the terminator
  remaining.remove_prefix(1);
  if (terminator == kCodeMobileUserAgent) {
    data->set_mobile_user_agent(true);
    // There must be a final kCodeWebp or kCodeSeparator. Otherwise, invalid.
    // Check and strip it.
    if (remaining.empty()) {
      return false;
    }
    terminator = remaining[0];
    if (terminator != kCodeWebp && terminator != kCodeSeparator) {
      return false;
    }
    remaining.remove_prefix(1);
  }
  data->set_attempt_webp(terminator == kCodeWebp);

  GoogleString* url = StringVectorAdd(urls);
  if (UrlEscaper::DecodeFromUrlSegment(remaining, url)) {
    return true;
  } else {
    urls->pop_back();
    return false;
  }
}

}  // namespace net_instaweb
