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
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/scanline_utils.h"

extern "C" {
#include "third_party/giflib/lib/gif_lib.h"

#ifdef USE_SYSTEM_LIBPNG
#include "png.h"                                               // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

class ScopedGifStruct;

// Reader for GIF-encoded data.
class GifReader : public PngReaderInterface {
 public:
  explicit GifReader(MessageHandler* handler);
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
  MessageHandler* message_handler_;
  DISALLOW_COPY_AND_ASSIGN(GifReader);
};

// GifFrameReader decodes GIF images and outputs the raw pixel
// data, image size, pixel type, etc. The class accepts single frame
// (non-animated) GIF. The output is RGB_888 if transparent color is
// not specified, or RGBA_8888 otherwise. Animated GIFs are not
// supported and fail on Initialize().
//
// Note: The input image stream must be valid throughout the life of
//   the object. In other words, the image_buffer input you set to
//   Initialize() cannot be changed until your last call to the
//   ReadNextScanline(). That said, if you are sure that
//   your image is a progressive GIF, you can modify image_buffer
//   after the first call to ReadNextScanline().
//
// At the moment, this class only supports non-animate GIFs.
// TODO(vchudnov): Support animated GIFs.
class GifFrameReader : public MultipleFrameReader {
 public:
  explicit GifFrameReader(MessageHandler* handler);
  virtual ~GifFrameReader();

  virtual ScanlineStatus Reset();

  // Initialize the reader with the given image stream. Note that
  // image_buffer must remain unchanged until the last call to
  // ReadNextScanlineWithStatus().
  virtual ScanlineStatus Initialize(const void* image_buffer,
                                    size_t buffer_length);

  virtual bool HasMoreFrames() const {
    return (next_frame_ < image_spec_.num_frames);
  }

  virtual bool HasMoreScanlines() const {
    return (row_ < image_spec_.height);
  }

  virtual ScanlineStatus PrepareNextFrame();

  // Return the next row of pixels. For non-progressive GIF,
  // ReadNextScanline will decode one row of pixels each time when it
  // is called, but for progressive GIF, ReadNextScanline will decode
  // the entire image at the first time when it is called.
  virtual ScanlineStatus ReadNextScanline(const void** out_scanline_bytes);

  virtual ScanlineStatus GetFrameSpec(const FrameSpec** frame_spec) const {
    *frame_spec = &frame_spec_;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus GetImageSpec(const ImageSpec** image_spec) const {
    *image_spec = &image_spec_;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

 private:
  void ComputeOrExtendImageSize();

  // 'transparent_index' will be set to '-1' if no transparent color has been
  // defined.
  ScanlineStatus ProcessSingleImageGif(size_t* first_frame_offset,
                                       int* transparent_index);
  ScanlineStatus DecodeProgressiveGif();
  ScanlineStatus CreateColorMap(int transparent_index);
  bool HasVisibleBackground() const;

  bool was_initialized_;
  ImageSpec image_spec_;
  FrameSpec frame_spec_;
  int next_frame_;

  // The following are for the current frame.

  bool is_progressive_;
  // The current output row.
  int row_;
  size_t bytes_per_row_;

  // Palette of the image. It has 257 entries. The last one stores the
  // background color.
  net_instaweb::scoped_array<PaletteRGBA> gif_palette_;
  // Buffer for holding the color (RGB or RGBA) for a row of pixels.
  net_instaweb::scoped_array<GifByteType> image_buffer_;
  // Buffer for holding the palette index for a row of pixels (for
  // non-progressive GIF) or for the entire image (for progressive GIF).
  net_instaweb::scoped_array<GifByteType> image_index_;
  // gif_struct_ stores a pointer to the input image stream. It also
  // keeps track of the length of data that giflib has read. It is
  // initialized in Initialize() and is updated in
  // ReadNextScanlineWithStatus().
  scoped_ptr<ScopedGifStruct> gif_struct_;

  DISALLOW_COPY_AND_ASSIGN(GifFrameReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_GIF_READER_H_
