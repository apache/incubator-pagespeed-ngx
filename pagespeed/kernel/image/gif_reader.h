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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/scanline_utils.h"

extern "C" {
#include "third_party/giflib/lib/gif_lib.h"

#ifdef USE_SYSTEM_LIBPNG
#include "png.h"                                               // NOLINT
#else
#include "third_party/libpng/src/png.h"
#endif
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

class ScopedGifStruct;

// Utility to translate the frame (GIF "image") disposal method from
// the value encoded in the GIF file to the DisposalMethod enum.
FrameSpec::DisposalMethod GifDisposalToFrameSpecDisposal(int gif_disposal);

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

// GifFrameReader decodes GIF images and outputs the raw pixel data,
// image size, pixel type, etc. The class accepts both single frame
// and animated GIFs. The output is RGB_888 if transparent color is
// not specified, or RGBA_8888 otherwise.
//
// Note: The input image stream must be valid throughout the life of
//   the object. In other words, the image_buffer input you set to
//   Initialize() cannot be changed until your last call to the
//   ReadNextScanline(). That said, if you are sure that your image is
//   a single-frame progressive GIF, you can modify image_buffer after
//   the first call to ReadNextScanline().
//
// Note: In the wild, there are many static GIFs that don't conform to
//   the GIF standard by having, for example, image dimensions larger
//   than frame dimensions. The workaround for such images is to
//   properly pad those frames. Because this is such a common
//   occurrence, we disallow instantiating GifFrameReader directly and
//   instead require that clients call CreateImageFrameReader() to
//   instantiate this class.
class GifFrameReader : public MultipleFrameReader {
 public:
  virtual ~GifFrameReader();

  virtual ScanlineStatus Reset();

  // Initialize the reader with the given image stream. Note that
  // image_buffer must remain unchanged until the last call to
  // ReadNextScanlineWithStatus().
  virtual ScanlineStatus Initialize();

  virtual bool HasMoreFrames() const {
    return (image_initialized_ && (next_frame_ < image_spec_.num_frames));
  }

  virtual bool HasMoreScanlines() const {
    return (frame_initialized_ && (next_row_ < frame_spec_.height));
  }

  virtual ScanlineStatus PrepareNextFrame();

  // Return the next row of pixels. For non-progressive GIF,
  // ReadNextScanline will decode one row of pixels each time when it
  // is called, but for progressive GIF, ReadNextScanline will decode
  // the entire image at the first time when it is called.
  virtual ScanlineStatus ReadNextScanline(const void** out_scanline_bytes);

  virtual ScanlineStatus GetFrameSpec(FrameSpec* frame_spec) const {
    if (frame_spec == NULL) {
      return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                              SCANLINE_STATUS_INVOCATION_ERROR,
                              FRAME_GIFREADER,
                              "Unexpected NULL pointer.");
    }
    *frame_spec = frame_spec_;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus GetImageSpec(ImageSpec* image_spec) const {
    if (image_spec == NULL) {
      return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                              SCANLINE_STATUS_INVOCATION_ERROR,
                              FRAME_GIFREADER,
                              "Unexpected NULL pointer.");
    }
    *image_spec = image_spec_;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus set_quirks_mode(QuirksMode quirks_mode);

  // Apply the specified browser-specific tweaking of image_spec based
  // on the first frame's frame_spec and whether an explicit
  // loop_count was read from the GIF file.
  static void ApplyQuirksModeToImage(QuirksMode quirks_mode,
                                     bool has_loop_count,
                                     const FrameSpec& frame_spec,
                                     ImageSpec* image_spec);

  // Apply the specified browser-specific tweaking of the first
  // frame's frame_spec based on image_spec.
  static void ApplyQuirksModeToFirstFrame(QuirksMode quirks_mode,
                                          const ImageSpec& image_spec,
                                          FrameSpec* frame_spec);

 private:
  // Clients should call this function to instantiate
  // GifFrameReader. This function is defined in read_image.cc.
  friend MultipleFrameReader* InstantiateImageFrameReader(
      ImageFormat image_type,
      MessageHandler* handler,
      ScanlineStatus* status);

  // Used in gif_reader_test.cc and frame_interface_integration_test.cc.
  friend class TestGifFrameReader;

  // To instantiate this class, use InstantiateImageFrameReader, which
  // is declared a friend function above.
  explicit GifFrameReader(MessageHandler* handler);

  // The GIF format can specify one palette index which is treated as
  // transparent; we store this index in frame_transparent_index_. In
  // case the GIF file does not employ transparency, we store the
  // special "index" value kNoTransparentIndex instead.
  static const int kNoTransparentIndex;


  // Decodes a progressive image.
  ScanlineStatus DecodeProgressiveGif();

  // Decodes a non-progressive image.
  ScanlineStatus DecodeNonProgressiveGif();

  ScanlineStatus CreateColorMap();

  // Gets the image-scope meta-data (GIF screen size, global palette,
  // number of frames, etc.), resetting the GIF file offset before
  // returning.
  ScanlineStatus GetImageData();

  // Helper function for GetImageData() that reads the GIF application
  // extension.
  ScanlineStatus ProcessExtensionAffectingImage(bool past_first_frame);

  // Helper function for PrepareNextFrame that gets the frame-scope
  // metadata (duration, disposal, etc.) from the GIF Graphics
  // Extension Block.
  ScanlineStatus ProcessExtensionAffectingFrame();

  // Whether the image_has been initialized.
  bool image_initialized_;

  // Whether the current frame has been initialized.
  bool frame_initialized_;

  // Image metadata.
  ImageSpec image_spec_;

  // Frame metadata.
  FrameSpec frame_spec_;

  // Whether we've already encountered the animation loop count.
  bool has_loop_count_;

  // The next frame to be read AFTER the current frame.
  size_px next_frame_;

  // The following are for the current frame.

  // The next row to output via ReadScanline.
  size_px next_row_;

  // The palette index of the transparent entry in the current frame,
  // or kNoTransparentIndex otherwise. Set by PrepareNextFrame().
  int frame_transparent_index_;

  // Palette of the image, with 256 entries.
  net_instaweb::scoped_array<PaletteRGBA> gif_palette_;

  // Buffer for holding the color (RGB or RGBA) for a row of pixels.
  net_instaweb::scoped_array<GifByteType> frame_buffer_;

  // Buffer for holding the palette index for a row of pixels (for
  // non-progressive GIF) or for the entire image (for progressive GIF).
  net_instaweb::scoped_array<GifByteType> frame_index_;

  // gif_struct_ stores a pointer to the input image stream. It also
  // keeps track of the length of data that giflib has read. It is
  // initialized in Initialize() and is updated in
  // ReadNextScanlineWithStatus().
  net_instaweb::scoped_ptr<ScopedGifStruct> gif_struct_;

  // We need to know the palette size as we read each frame because,
  // in rare cases, images contain pixels referring to out-of-range
  // palette entries. In that case, in PrepareNextFrame() we make the
  // frame's pixel format be RGBA_8888 and in ReadNextScanline() we
  // replace the invalid pixels with transparent pixels.  Note that a
  // uint8_t would be sufficient for this field, but giflib returns an
  // int for the palette size.
  int frame_palette_size_;

  // For frames that are either progressive or NOT in RGBA_8888
  // format, we set this flag in PrepareNextFrame() and then read the
  // entire frame to see whether there are any pixels with
  // out-of-range palette entries. If so, in PrepareNextFrame() we
  // make the frame's pixel format be RGBA_8888 and in
  // ReadNextScanline() we replace the invalid pixels with transparent
  // pixels.
  bool frame_eagerly_read_;

  DISALLOW_COPY_AND_ASSIGN(GifFrameReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_GIF_READER_H_
