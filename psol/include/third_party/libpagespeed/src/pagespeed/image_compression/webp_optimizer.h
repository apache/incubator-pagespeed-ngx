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

// Author: Victor Chudnovsky

#ifndef PAGESPEED_IMAGE_COMPRESSION_WEBP_OPTIMIZER_H_
#define PAGESPEED_IMAGE_COMPRESSION_WEBP_OPTIMIZER_H_

#include <string>

#include "third_party/libwebp/webp/encode.h"

#include "pagespeed/image_compression/scanline_interface.h"

namespace pagespeed {

namespace image_compression {

struct WebpConfiguration {
  // This contains a subset of the options in
  // libwebp/webp/encode.h:WebPConfig, with our own defaults.

  WebpConfiguration()
      : lossless(true), quality(100), method(3), target_size(0),
        alpha_compression(0), alpha_filtering(1), alpha_quality(100) {}
  void CopyTo(WebPConfig* webp_config) const;

  int lossless;           // Lossless encoding (0=lossy(default), 1=lossless).
  float quality;          // between 0 (smallest file) and 100 (biggest)
  int method;             // quality/speed trade-off (0=fast, 6=slower-better)

  // Parameters related to lossy compression only:
  int target_size;        // if non-zero, set the desired target size in bytes.
                          // Takes precedence over the 'compression' parameter.
  int alpha_compression;  // Algorithm for encoding the alpha plane (0 = none,
                          // 1 = compressed with WebP lossless). Default is 1.
  int alpha_filtering;    // Predictive filtering method for alpha plane.
                          //  0: none, 1: fast, 2: best. Default is 1.
  int alpha_quality;      // Between 0 (smallest size) and 100 (lossless).
                          // Default is 100.
  // NOTE: If you add more fields to this struct, please update the
  // CopyTo() method.
};


class WebpScanlineWriter : public ScanlineWriterInterface {
 public:
  WebpScanlineWriter();
  virtual ~WebpScanlineWriter();

  virtual bool Init(const size_t width, const size_t height,
                    PixelFormat pixel_format);
  bool InitializeWrite(const WebpConfiguration& config,
                       std::string* const out);

  virtual bool WriteNextScanline(void *scanline_bytes);

  // Note that even after WriteNextScanline() has been called,
  // InitializeWrite() and FinalizeWrite() may be called repeatedly to
  // write the image with, say, different configs.
  virtual bool FinalizeWrite();

 private:
  // Number of bytes per row. See
  // https://developers.google.com/speed/webp/docs/api#encodingapi
  int stride_bytes_;

  // Allocated pointer to the start of memory for the RGB(A) data.
  uint8_t* rgb_;

  // Pointer to the end of the RGB(A) data (i.e. to the first byte
  // after the allocated memory).
  uint8_t* rgb_end_;

  // Pointer to the next byte of RGB(A) data to be filled in.
  uint8_t* position_bytes_;

  // libwebp objects for the WebP generation.
  WebPPicture picture_;
  WebPConfig* config_;
#ifndef NDEBUG
  WebPAuxStats stats_;
#endif

  // Pointer to the webp output.
  std::string* webp_image_;

  // Whether the image has an alpha channel.
  bool has_alpha_;

  // Whether Init() has been called successfully.
  bool init_ok_;

  // Whether WebPPictureImport* has been called.
  bool imported_;

  // Whether all the scanlines have been written.
  bool got_all_scanlines_;

  DISALLOW_COPY_AND_ASSIGN(WebpScanlineWriter);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_IMAGE_COMPRESSION_WEBP_OPTIMIZER_H_
