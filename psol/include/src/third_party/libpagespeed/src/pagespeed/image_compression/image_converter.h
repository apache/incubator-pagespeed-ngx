/**
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

// Author: Satyanarayana Manyam

#ifndef PAGESPEED_IMAGE_COMPRESSION_IMAGE_CONVERTER_H_
#define PAGESPEED_IMAGE_COMPRESSION_IMAGE_CONVERTER_H_

#include <string>

#include "pagespeed/image_compression/scanline_interface.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"

namespace pagespeed {

namespace image_compression {

class ImageConverter {
 public:
  // Converts image one line at a time, between different image formats.
  static bool ConvertImage(ScanlineReaderInterface& reader,
                           ScanlineWriterInterface& writer);

  // Optimizes the given png image, also converts to jpeg and take the
  // the one that has smaller size and set the output. Returns false
  // if both of them fails.
  static bool OptimizePngOrConvertToJpeg(
      PngReaderInterface& png_struct_reader,
      const std::string& in,
      const JpegCompressionOptions& options,
      std::string* out,
      bool *is_out_png);

 private:
  ImageConverter();
  ~ImageConverter();

  DISALLOW_COPY_AND_ASSIGN(ImageConverter);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_IMAGE_COMPRESSION_IMAGE_CONVERTER_H_
