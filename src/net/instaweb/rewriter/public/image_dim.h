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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DIM_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DIM_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Specification for image dimensions, either in an image file
// or in the page source.  This is 1 method away from a struct.
class ImageDim {
 public:
  // Constructor ensures repeatable field values.
  ImageDim() : valid_(false), width_(-1), height_(-1) { }

  int width() const { return width_; }
  int height() const { return height_; }
  bool valid() const { return valid_; }

  void invalidate() { valid_ = false; }
  void set_dims(int width, int height) {
    width_ = width;
    height_ = height;
    valid_ = true;
  }

  // *Append* encoding of this to *out.
  void EncodeTo(std::string* out) const;

  // Decode encoding from "in", truncating it to remove consumed data.
  // Invalidate this and return false on parse failure; "in" is in an
  // arbitrary state in this case.
  bool DecodeFrom(StringPiece* in);

 private:
  bool valid_;  // If false, other two fields have arbitrary values.
  int width_;
  int height_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DIM_H_
