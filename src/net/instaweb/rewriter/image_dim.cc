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

#include "net/instaweb/rewriter/public/image_dim.h"

#include "net/instaweb/rewriter/cached_result.pb.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

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

ImageDim::ImageDim(const ResourceContext& data) {
  if (data.has_width() && data.has_height()) {
    valid_ = true;
    width_ = data.width();
    height_ = data.height();
  } else {
    valid_ = false;
    width_ = -1;
    height_ = -1;
  }
}

void ImageDim::ToResourceContext(ResourceContext* data) const {
  data->Clear();
  if (valid_) {
    data->set_width(width_);
    data->set_height(height_);
  }
}

void ImageDim::EncodeTo(std::string* out) const {
  if (valid_) {
    out->append(StrCat(IntegerToString(width_), "x",
                       IntegerToString(height_)));
  }
  out->append("x");
}

bool ImageDim::DecodeFrom(StringPiece* in) {
  valid_ = false;
  bool ok;
  if (in->empty()) {
    ok = false;
  } else if ((*in)[0] == 'x') {
    // No dimensions.
    in->remove_prefix(1);
    ok = true;
  } else if (DecodeIntX(in, &width_) &&
             DecodeIntX(in, &height_)) {
    // Dimensions.
    valid_ = true;
    ok = true;
  } else {
    ok = false;
  }
  return ok;
}

}  // namespace net_instaweb
