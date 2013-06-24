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

// Author: Bryan McQuade

#ifndef PAGESPEED_KERNEL_IMAGE_GIF_READER_H_
#define PAGESPEED_KERNEL_IMAGE_GIF_READER_H_

#include <stdbool.h>
#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_interface.h"

#include "third_party/giflib/lib/gif_lib.h"
#include "third_party/libpng/png.h"

namespace pagespeed {

namespace image_compression {

class ScopedGifStruct;
struct PaletteRGBA;

// Reader for GIF-encoded data.
class GifReader : public PngReaderInterface {
 public:
  GifReader();
  virtual ~GifReader();

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
  DISALLOW_COPY_AND_ASSIGN(GifReader);
};

// GifScanlineReaderRaw decodes GIF images and outputs the raw pixel data, image
// size, pixel type, etc. The class accepts single frame (non-animated) GIF.
// The output is RGB_888 if transparent color is not specified, or RGBA_8888
// otherwise. Animated GIFs are not supported and fail on Initialize().
//
// Note: The input image stream must be valid throughout the life of the
//   object. In other words, the image_buffer input you set to Initialize()
//   cannot be changed until your last call to the ReadNextScanline().
//   That has been said, if you are sure that your image is a progressive GIF,
//   you can modify image_buffer after the first call to ReadNextScanline().
//
class GifScanlineReaderRaw : public ScanlineReaderInterface {
 public:
  GifScanlineReaderRaw();
  virtual ~GifScanlineReaderRaw();
  virtual bool Reset();

  // Initialize the reader with the given image stream. Note that image_buffer
  // must remain unchanged until the last call to ReadNextScanline().
  bool Initialize(const void* image_buffer, size_t buffer_length);

  // Return the next row of pixels. For non-progressive GIF, ReadNextScanline
  // will decode one row of pixels each time when it is called, but for
  // progressive GIF, ReadNextScanline will decode the entire image at the
  // first time when it is called.
  virtual bool ReadNextScanline(void** out_scanline_bytes);

  // Return the number of bytes in a row (without padding).
  virtual size_t GetBytesPerScanline() { return bytes_per_row_; }

  virtual size_t GetImageHeight();
  virtual size_t GetImageWidth();
  virtual bool HasMoreScanLines() { return (row_ < GetImageHeight()); }
  virtual PixelFormat GetPixelFormat() { return pixel_format_; }

 private:
  bool ProcessSingleImageGif(size_t* offset, int* transparent_index);
  bool CreateColorMap();
  bool DecodeProgressiveGif();

 private:
  PixelFormat pixel_format_;
  bool is_progressive_;
  // The current output row.
  size_t row_;
  size_t pixel_size_;
  size_t bytes_per_row_;
  // If the encoded image covers the entire logical screen, we don't need to
  // draw the background and no_background will be set to true. Most GIF images
  // belong to this category.
  bool no_background_;
  bool was_initialized_;
  // Palette of the image. It has 257 entries. The last one stores the
  // background color.
  scoped_array<PaletteRGBA> gif_palette_;
  // Buffer for holding the color (RGB or RGBA) for a row of pixels.
  scoped_array<GifByteType> image_buffer_;
  // Buffer for holding the palette index for a row of pixels (for
  // non-progressive GIF) or for the entire image (for progressive GIF).
  scoped_array<GifByteType> image_index_;
  // gif_struct_ stores a pointer to the input image stream. It also keeps
  // track of the length of data that giflib has read. It is initialized
  // in Initialize() and is updated in ReadNextScanline().
  scoped_ptr<ScopedGifStruct> gif_struct_;

  DISALLOW_COPY_AND_ASSIGN(GifScanlineReaderRaw);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_GIF_READER_H_
