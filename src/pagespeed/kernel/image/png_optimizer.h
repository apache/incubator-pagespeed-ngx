/*
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

// Author: Bryan McQuade, Satyanarayana Manyam

#ifndef PAGESPEED_KERNEL_IMAGE_PNG_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_PNG_OPTIMIZER_H_

// Note: we should not include setjmp.h here, since libpng 1.2 headers
// include it themselves, and get unhappy if we do it ourselves.

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}  // extern "C"

#include <setjmp.h>
#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_interface.h"

namespace pagespeed {

namespace image_compression {

class ScanlineStreamInput;

struct PngCompressParams {
  PngCompressParams(int level, int strategy);

  // Indicates what png filter type to be used while compressing the image.
  // Valid values for this are
  //   PNG_FILTER_NONE
  //   PNG_FILTER_SUB
  //   PNG_FILTER_UP
  //   PNG_FILTER_AVG
  //   PNG_FILTER_PAETH
  //   PNG_ALL_FILTERS
  int filter_level;
  // Indicates which compression strategy to use while compressing the image.
  // Valid values for this are
  //   Z_FILTERED
  //   Z_HUFFMAN_ONLY
  //   Z_RLE
  //   Z_FIXED
  //   Z_DEFAULT_COMPRESSION
  int compression_strategy;
};

// Helper that manages the lifetime of the png_ptr and info_ptr.
class ScopedPngStruct {
 public:
  enum Type {
    READ,
    WRITE
  };

  explicit ScopedPngStruct(Type t);
  ~ScopedPngStruct();

  bool valid() const { return png_ptr_ != NULL && info_ptr_ != NULL; }

  // This will only return false as a result of a longjmp due to an
  // unhandled libpng error.
  bool reset();

  png_structp png_ptr() const { return png_ptr_; }
  png_infop info_ptr() const { return info_ptr_; }

 private:
  png_structp png_ptr_;
  png_infop info_ptr_;
  Type type_;
};

// Helper class that provides an API to read a PNG image from some
// source.
class PngReaderInterface {
 public:
  PngReaderInterface();
  virtual ~PngReaderInterface();

  // Parse the contents of body, convert to a PNG, and populate the
  // PNG structures with the PNG representation. If 'require_opaque'
  // is true, returns an image without an alpha channel if the
  // original image has no transparent pixels, and fails
  // otherwise. Returns true on success, false on failure.
  virtual bool ReadPng(const GoogleString& body,
                       png_structp png_ptr,
                       png_infop info_ptr,
                       int transforms,
                       bool require_opaque) const = 0;

  // Parse the contents of body, convert to a PNG, and populate the
  // PNG structures with the PNG representation. Returns true on
  // success, false on failure.
  bool ReadPng(const GoogleString& body,
               png_structp png_ptr,
               png_infop info_ptr,
               int transforms) const {
    return ReadPng(body, png_ptr, info_ptr, transforms, false);
  }

  // Get just the attributes of the given image. out_bit_depth is the
  // number of bits per channel. out_color_type is one of the
  // PNG_COLOR_TYPE_* declared in png.h.
  // TODO(bmcquade): consider merging this with ImageAttributes.
  virtual bool GetAttributes(const GoogleString& body,
                             int* out_width,
                             int* out_height,
                             int* out_bit_depth,
                             int* out_color_type) const = 0;

  // Get the background color, in the form of 8-bit RGB triplets. Note
  // that if the underlying image uses a bit_depth other than 8, the
  // background color will be scaled to 8-bits per channel.
  static bool GetBackgroundColor(
      png_structp png_ptr, png_infop info_ptr,
      unsigned char *red, unsigned char* green, unsigned char* blue);

  // Returns true if the alpha channel is actually a opaque. Returns
  // false otherwise. It is an error to call this method for an image
  // that does not have an alpha channel.
  static bool IsAlphaChannelOpaque(png_structp png_ptr, png_infop info_ptr);

 private:
  DISALLOW_COPY_AND_ASSIGN(PngReaderInterface);
};

// Reader for PNG-encoded data.
// This is sample code on how someone can use the scanline reader
// interface.
// bool func() {
//   if (setjmp(*GetJmpBuf())) {
//     return false;
//   }
//
//   InitializeRead(...)
//   while (HasMoreScanlines()) {
//     Scanline line;
//     ReadNextScanline(line);
//     ....
//     ....
//   }
// }
class PngScanlineReader : public ScanlineReaderInterface {
 public:
  PngScanlineReader();
  virtual ~PngScanlineReader();

  jmp_buf* GetJmpBuf();

  // This will only return false as a result of a longjmp due to an
  // unhandled libpng error.
  virtual bool Reset();

  // Initializes the read structures with the given input.
  bool InitializeRead(const PngReaderInterface& reader, const GoogleString& in);
  bool InitializeRead(const PngReaderInterface& reader, const GoogleString& in,
                      bool* is_opaque);

  virtual size_t GetBytesPerScanline();
  virtual bool HasMoreScanLines();
  virtual bool ReadNextScanline(void** out_scanline_bytes);
  virtual size_t GetImageHeight();
  virtual size_t GetImageWidth();
  virtual PixelFormat GetPixelFormat();

  void set_transform(int transform);
  void set_require_opaque(bool require_opaque);
  int GetColorType();
  bool GetBackgroundColor(
      unsigned char* red, unsigned char* green, unsigned char* blue);

 private:
  ScopedPngStruct read_;
  size_t current_scanline_;
  int transform_;
  bool require_opaque_;

  DISALLOW_COPY_AND_ASSIGN(PngScanlineReader);
};

class PngOptimizer {
 public:
  static bool OptimizePng(const PngReaderInterface& reader,
                          const GoogleString& in,
                          GoogleString* out);

  static bool OptimizePngBestCompression(const PngReaderInterface& reader,
                                         const GoogleString& in,
                                         GoogleString* out);

 private:
  PngOptimizer();
  ~PngOptimizer();

  // Take the given input and losslessly compress it by removing
  // all unnecessary chunks, and by choosing an optimal PNG encoding.
  // @return true on success, false on failure.
  bool CreateOptimizedPng(const PngReaderInterface& reader,
                          const GoogleString& in,
                          GoogleString* out);

  // Turn on best compression. Requires additional CPU but produces
  // smaller files.
  void EnableBestCompression() { best_compression_ = true; }

  bool WritePng(ScopedPngStruct* write, GoogleString* buffer);
  bool CopyReadToWrite();
  // The 'from' object is conceptually const, but libpng doesn't accept const
  // pointers in the read functions.
  bool CopyPngStructs(ScopedPngStruct* from, ScopedPngStruct* to);
  bool CreateBestOptimizedPngForParams(const PngCompressParams* param_list,
                                       size_t param_list_size,
                                       GoogleString* out);
  bool CreateOptimizedPngWithParams(ScopedPngStruct* write,
                                    const PngCompressParams& params,
                                    GoogleString* out);
  ScopedPngStruct read_;
  ScopedPngStruct write_;
  bool best_compression_;

  DISALLOW_COPY_AND_ASSIGN(PngOptimizer);
};

// Reader for PNG-encoded data.
class PngReader : public PngReaderInterface {
 public:
  PngReader();
  virtual ~PngReader();
  virtual bool ReadPng(const GoogleString& body,
                       png_structp png_ptr,
                       png_infop info_ptr,
                       int transforms,
                       bool require_opaque) const;

  virtual bool GetAttributes(const GoogleString& body,
                             int* out_width,
                             int* out_height,
                             int* out_bit_depth,
                             int* out_color_type) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PngReader);
};

// Class PngScanlineReaderRaw decodes PNG images and outputs the raw pixel data,
// image size, pixel type, etc. The class accepts all formats supported by
// libpng. The output is Gray_8, RGB_888, or RGBA_8888. The following
// transformations are used:
//   - Image with depth other than 8 bits/pixel is expanded or stripped to
//     8 bits/pixel.
//   - Paletted image is converted to RGB or RGBA depending on whether
//     transparency is specified.
//   - Gray_Alpha is converted to RGBA.
//
// Note: The input image stream must be valid throughout the life of the
//   object. In other words, the image_buffer input you set to the Initialize()
//   method cannot be changed until your last call to the ReadNextScanline()
//   method.
//
class PngScanlineReaderRaw : public ScanlineReaderInterface {
 public:
  PngScanlineReaderRaw();
  virtual ~PngScanlineReaderRaw();

  // This will only return false as a result of a longjmp due to an
  // unhandled libpng error.
  virtual bool Reset();

  // Initialize the reader with the given image stream. Note that image_buffer
  // must remain unchanged until the last call to ReadNextScanline().
  bool Initialize(const void* image_buffer, size_t buffer_length);

  // Return the next row of pixels. For non-progressive PNG, ReadNextScanline
  // will decode one row of pixels each time when it is called, but for
  // progressive PNG, ReadNextScanline will decode the entire image at the
  // first time when it is called.
  virtual bool ReadNextScanline(void** out_scanline_bytes);

  // Return the number of bytes in a row (without padding).
  virtual size_t GetBytesPerScanline() { return bytes_per_row_; }

  virtual bool HasMoreScanLines() { return (row_ < height_); }
  virtual PixelFormat GetPixelFormat() { return pixel_format_; }
  virtual size_t GetImageHeight() { return height_; }
  virtual size_t GetImageWidth() {  return width_; }

 private:
  PixelFormat pixel_format_;
  bool is_progressive_;
  size_t height_;
  size_t width_;
  size_t bytes_per_row_;
  size_t row_;
  bool was_initialized_;
  scoped_array<png_byte> image_buffer_;
  ScopedPngStruct png_struct_;
  // png_input_ stores a pointer to the input image stream. It also keeps
  // tracking the length of data that libpng has read. It is initialized
  // in Initialize() and is updated in ReadNextScanline().
  scoped_ptr<ScanlineStreamInput> png_input_;

  DISALLOW_COPY_AND_ASSIGN(PngScanlineReaderRaw);
};

// Class PngScanlineWriter writes a PNG image. It supports Gray_8, RGB_888,
// and RGBA_8888 formats.
class PngScanlineWriter : public ScanlineWriterInterface {
 public:
  PngScanlineWriter();
  virtual ~PngScanlineWriter();

  // Initialize the basic parameters for writing the image. Size of the image
  // must be 1-by-1 or larger.
  virtual bool Init(const size_t width, const size_t height,
                    PixelFormat pixel_format);

  // Initialize additional parameters for writing the image. You can set
  // 'params' to NULL to use the default compression configuration.
  bool Initialize(const PngCompressParams* params,
                  GoogleString* png_image);

  // Write a scanline with the data provided. Return false in case of error.
  virtual bool WriteNextScanline(void *scanline_bytes);

  // Finalize write structure once all scanlines are written.
  // If FinalizeWriter() is called before all of the scanlines have been
  // written, the object will be reset to the initial state.
  virtual bool FinalizeWrite();

 private:
  // Reset the object to the usable state.
  bool Reset();

  // Validate the input parameters.
  bool Validate(const PngCompressParams* params,
                GoogleString* png_image);

 private:
  size_t width_;
  size_t height_;
  size_t row_;
  PixelFormat pixel_format_;
  ScopedPngStruct png_struct_;
  bool was_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PngScanlineWriter);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_PNG_OPTIMIZER_H_
