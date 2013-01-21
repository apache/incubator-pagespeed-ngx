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
#include "pagespeed/image_compression/webp_optimizer.h"

namespace pagespeed {

namespace image_compression {

class ImageConverter {
 public:
  enum ImageType {
    IMAGE_NONE = 0,
    IMAGE_PNG,
    IMAGE_JPEG,
    IMAGE_WEBP
  };

  // Converts image one line at a time, between different image formats.
  static bool ConvertImage(ScanlineReaderInterface* reader,
                           ScanlineWriterInterface* writer);

  static bool ConvertPngToJpeg(
      const PngReaderInterface& png_struct_reader,
      const std::string& in,
      const JpegCompressionOptions& options,
      std::string* out);

  // Reads the PNG encoded in 'in' with 'png_struct_reader', encodes
  // it in WebP format using the options in 'config', and writes the
  // resulting WebP in 'out'. Note that if config.alpha_quality==0,
  // this function will fail when attempting to convert an image with
  // transparent pixels. Returns is_opaque set to true iff the 'in'
  // image was opaque.
  static bool ConvertPngToWebp(
      const PngReaderInterface& png_struct_reader,
      const std::string& in,
      const WebpConfiguration& config,
      std::string* out,
      bool* is_opaque);

  // Reads the PNG encoded in 'in' with 'png_struct_reader', encodes
  // it in WebP format using the options in 'config', and writes the
  // resulting WebP in 'out'. Note that if config.alpha_quality==0,
  // this function will fail when attempting to convert an image with
  // transparent pixels. Returns is_opaque set to true iff the 'in'
  // image was opaque. On entry, '*webp_writer' must be NULL; on exit,
  // it contains the WebpScanlineWriter instance that was used to
  // write the WebP, and the caller is responsible for deleting
  // it. Most clients will prefer to use the other form
  // ConvertPngToWebp.
  static bool ConvertPngToWebp(
      const PngReaderInterface& png_struct_reader,
      const std::string& in,
      const WebpConfiguration& config,
      std::string* out,
      bool* is_opaque,
      WebpScanlineWriter** webp_writer);

  // Optimizes the given png image, also converts to jpeg and take the
  // the one that has smaller size and set the output. Returns false
  // if both of them fails.
  static bool OptimizePngOrConvertToJpeg(
      const PngReaderInterface& png_struct_reader,
      const std::string& in,
      const JpegCompressionOptions& options,
      std::string* out,
      bool* is_out_png);

  // Populates 'out' with a version of the input image 'in' resulting
  // in the smallest size, and returns the corresponding
  // ImageType. The image formats that are candidates for the output
  // image are: lossless WebP, optimized PNG, custom JPEG (if
  // jpeg_options != NULL), and custom WebP (if webp_config !=
  // NULL). To compensate for the loss in quality in the custom JPEG
  // and WebP (which are presumably lossy), these two formats must be
  // substantially smaller than the optimized PNG and the lossless
  // WebP in order to be chosen. In the case where none of these image
  // formats could be generated or the original image turns out to be
  // the smallest, copies the original image to 'out' and returns
  // IMAGE_NONE.
  static ImageType GetSmallestOfPngJpegWebp(
      // TODO(bmcquade): should be a ScanlineReaderInterface.
      const PngReaderInterface& png_struct_reader,
      const std::string& in,
      const JpegCompressionOptions* jpeg_options,
      const WebpConfiguration* webp_config,
      std::string* out);

 private:
  ImageConverter();
  ~ImageConverter();

  DISALLOW_COPY_AND_ASSIGN(ImageConverter);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_IMAGE_COMPRESSION_IMAGE_CONVERTER_H_
