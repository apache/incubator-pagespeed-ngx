/*
 * Copyright 2013 Google Inc.
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

#ifndef PAGESPEED_KERNEL_IMAGE_SCANLINE_INTERFACE_H_
#define PAGESPEED_KERNEL_IMAGE_SCANLINE_INTERFACE_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace pagespeed {

namespace image_compression {

#if defined(PAGESPEED_SCANLINE_IMAGE_FORMAT) ||         \
  defined(PAGESPEED_SCANLINE_FORMAT_ENUM_NAME) ||       \
  defined(PAGESPEED_SCANLINE_FORMAT_ENUM_STRING) ||     \
  defined(PAGESPEED_SCANLINE_FORMAT_MIME_STRING)
#error "Preprocessor macro collision"
#endif

#define PAGESPEED_SCANLINE_IMAGE_FORMAT(_X)                   \
    _X(IMAGE_UNKNOWN, ""),                                    \
    _X(IMAGE_JPEG, "image/jpeg"),                             \
    _X(IMAGE_PNG, "image/png"),                               \
    _X(IMAGE_GIF, "image/gif"),                               \
    _X(IMAGE_WEBP, "image/webp")

#define PAGESPEED_SCANLINE_FORMAT_ENUM_NAME(_S, _M) _S
#define PAGESPEED_SCANLINE_FORMAT_ENUM_STRING(_S, _M) #_S
#define PAGESPEED_SCANLINE_FORMAT_MIME_STRING(_S, _M) _M

enum ImageFormat {
  PAGESPEED_SCANLINE_IMAGE_FORMAT(PAGESPEED_SCANLINE_FORMAT_ENUM_NAME)
};

// Returns a string representation of the given ImageFormat.
inline const char* ImageFormatToMimeTypeString(ImageFormat img_type) {
  static const char* kImageMimeType[] = {
    PAGESPEED_SCANLINE_IMAGE_FORMAT(PAGESPEED_SCANLINE_FORMAT_MIME_STRING)
  };
  return kImageMimeType[img_type];
}

// Returns the MIME-type string corresponding to the given ImageFormat.
inline const char* ImageFormatToString(ImageFormat img_type) {
  static const char* kImageFormatName[] = {
    PAGESPEED_SCANLINE_IMAGE_FORMAT(PAGESPEED_SCANLINE_FORMAT_ENUM_STRING)
  };
  return kImageFormatName[img_type];
}

#undef PAGESPEED_SCANLINE_FORMAT_MIME_STRING
#undef PAGESPEED_SCANLINE_FORMAT_ENUM_STRING
#undef PAGESPEED_SCANLINE_FORMAT_ENUM_NAME
#undef PAGESPEED_SCANLINE_IMAGE_FORMAT

#if defined(PAGESPEED_SCANLINE_PIXEL_FORMAT) ||         \
  defined(PAGESPEED_SCANLINE_PIXEL_ENUM_NAME) ||        \
  defined(PAGESPEED_SCANLINE_PIXEL_ENUM_STRING)
#error "Preprocessor macro collision."
#endif

#define PAGESPEED_SCANLINE_PIXEL_FORMAT(_X)  \
  _X(UNSUPPORTED),   /* Not supported for reading the image. */         \
  _X(RGB_888),       /* RGB triplets, 24 bits per pixel */              \
  _X(RGBA_8888),     /* RGB triplet plus alpha channel, 32 bits per pixel */ \
  _X(GRAY_8)         /* Grayscale, 8 bits per pixel */

#define PAGESPEED_SCANLINE_PIXEL_ENUM_NAME(_Y) _Y
#define PAGESPEED_SCANLINE_PIXEL_ENUM_STRING(_Y) #_Y

enum PixelFormat {
    PAGESPEED_SCANLINE_PIXEL_FORMAT(PAGESPEED_SCANLINE_PIXEL_ENUM_NAME)
};

inline const char* GetPixelFormatString(PixelFormat pf) {
  static const char* kFormatNames[] = {
    PAGESPEED_SCANLINE_PIXEL_FORMAT(PAGESPEED_SCANLINE_PIXEL_ENUM_STRING)
  };
  return kFormatNames[pf];
}
#undef PAGESPEED_SCANLINE_PIXEL_ENUM_STRING
#undef PAGESPEED_SCANLINE_PIXEL_ENUM_NAME
#undef PAGESPEED_SCANLINE_PIXEL_FORMAT

class ScanlineReaderInterface {
 public:
  ScanlineReaderInterface() {}
  virtual ~ScanlineReaderInterface() {}

  // Reset the ScanlineReaderIngterface to its initial state.  This
  // will only return false as a result of an unhandled error
  // condition, such as a longjmp due to a libpng error.
  virtual bool Reset() = 0;

  // Returns number of bytes that required to store a scanline.
  virtual size_t GetBytesPerScanline() = 0;

  // Returns true if there are more scanlines to read.
  virtual bool HasMoreScanLines() = 0;

  virtual ScanlineStatus InitializeWithStatus(const void* image_buffer,
                                              size_t buffer_length) = 0;
  inline bool Initialize(const void* image_buffer, size_t buffer_length) {
    return InitializeWithStatus(image_buffer, buffer_length).Success();
  }

  // Reads the next available scanline. Returns the ScanlineStatus of
  // the conversion.
  virtual ScanlineStatus ReadNextScanlineWithStatus(
      void** out_scanline_bytes) = 0;

  // Reads the next available scanline. Returns false if the
  // scan fails.
  inline bool ReadNextScanline(void** out_scanline_bytes) {
    return ReadNextScanlineWithStatus(out_scanline_bytes).Success();
  }

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
  virtual ScanlineStatus InitWithStatus(const size_t width, const size_t height,
                                        PixelFormat pixel_format) = 0;
  inline bool Init(const size_t width, const size_t height,
                   PixelFormat pixel_format) {
    return InitWithStatus(width, height, pixel_format).Success();
  }

  virtual ScanlineStatus InitializeWriteWithStatus(const void* config,
                                                   GoogleString* const out) = 0;
  inline bool InitializeWrite(const void* config,
                              GoogleString* const out) {
    return InitializeWriteWithStatus(config, out).Success();
  }

  // Writes the current scan line with data provided. Returns false
  // if the write fails.
  virtual ScanlineStatus WriteNextScanlineWithStatus(void *scanline_bytes) = 0;
  inline bool WriteNextScanline(void *scanline_bytes) {
    return WriteNextScanlineWithStatus(scanline_bytes).Success();
  }

  // Finalizes write structure once all scanlines are written.
  virtual ScanlineStatus FinalizeWriteWithStatus() = 0;
  inline bool FinalizeWrite() {
    return FinalizeWriteWithStatus().Success();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanlineWriterInterface);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_SCANLINE_INTERFACE_H_
