/**
 * Copyright 2009 Google Inc.
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

// Author: Bryan McQuade

#ifndef PNG_OPTIMIZER_GIF_READER_H_
#define PNG_OPTIMIZER_GIF_READER_H_

#include <string>

#include "base/logging.h"
#include "pagespeed/image_compression/png_optimizer.h"

namespace pagespeed {

namespace image_compression {

// Reader for GIF-encoded data.
class GifReader : public PngReaderInterface {
 public:
  GifReader();
  virtual ~GifReader();

  virtual bool ReadPng(const std::string& body,
                       png_structp png_ptr,
                       png_infop info_ptr,
                       int transforms);

  virtual bool GetAttributes(const std::string& body,
                             int* out_width,
                             int* out_height,
                             int* out_bit_depth,
                             int* out_color_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(GifReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif // PNG_OPTIMIZER_GIF_READER_H_
