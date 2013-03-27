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

#ifndef PAGESPEED_IMAGE_COMPRESSION_SCANLINE_INTERFACE_H_
#define PAGESPEED_IMAGE_COMPRESSION_SCANLINE_INTERFACE_H_

#include "base/basictypes.h"

namespace pagespeed {

namespace image_compression {

#if defined(PAGESPEED_SCANLINE_PIXEL_FORMAT) || \
  defined(PAGESPEED_SCANLINE_PIXEL_FORMAT) || \
  defined(PAGESPEED_SCANLINE_PIXEL_FORMAT)
#error "Preprocessor macro collision."
#endif

#define PAGESPEED_SCANLINE_PIXEL_FORMAT(_X)  \
  _X(UNSUPPORTED),   /* Not supported for reading the image. */ \
  _X(RGB_888),       /* RGB triplets, 24 bits per pixel */  \
  _X(RGBA_8888),     /* RGB triplet plus alpha channel, 32 bits per pixel */ \
  _X(GRAY_8)         /* Grayscale, 8 bits per pixel */

#define PAGESPEED_SCANLINE_ENUM_NAME(Y) Y
#define PAGESPEED_SCANLINE_ENUM_STRING(Y) #Y

enum PixelFormat {
    PAGESPEED_SCANLINE_PIXEL_FORMAT(PAGESPEED_SCANLINE_ENUM_NAME)
};

inline const char* GetPixelFormatString(PixelFormat pf) {
  static const char* format_names[] = {
    PAGESPEED_SCANLINE_PIXEL_FORMAT(PAGESPEED_SCANLINE_ENUM_STRING)
  };
  return format_names[pf];
}
#undef PAGESPEED_SCANLINE_ENUM_STRING
#undef PAGESPEED_SCANLINE_ENUM_NAME
#undef PAGESPEED_SCANLINE_PIXEL_FORMAT

class ScanlineReaderInterface {
 public:
  ScanlineReaderInterface() {}
  virtual ~ScanlineReaderInterface() {}

  // Reset the scanline reader to its initial state.  This will only
  // return false as a result of an unhandled error condition, such
  // as a longjmp due to a libpng error.
  virtual bool Reset() = 0;

  // Returns number of bytes that required to store a scanline.
  virtual size_t GetBytesPerScanline() = 0;

  // Returns true if there are more scanlines to read.
  virtual bool HasMoreScanLines() = 0;

  // Reads the next available scanline. Returns false if the
  // scan fails.
  virtual bool ReadNextScanline(void** out_scanline_bytes) = 0;

  // Returns the height of the image.
  virtual size_t GetImageHeight() = 0;

  // Returns the width of the image.
  virtual size_t GetImageWidth() = 0;

  // Returns the pixel format that need to be used by writer.
  virtual PixelFormat GetPixelFormat() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanlineReaderInterface);
};

class ScanlineWriterInterface {
 public:
  ScanlineWriterInterface() {}
  virtual ~ScanlineWriterInterface() {}

  // Initialize the basic parameter for writing the image.
  virtual bool Init(const size_t width, const size_t height,
                    PixelFormat pixel_format) = 0;

  // Writes the current scan line with data provided. Returns false
  // if the write fails.
  virtual bool WriteNextScanline(void *scanline_bytes) = 0;

  // Finalizes write structure once all scanlines are written.
  virtual bool FinalizeWrite() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanlineWriterInterface);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_IMAGE_COMPRESSION_SCANLINE_INTERFACE_H_
