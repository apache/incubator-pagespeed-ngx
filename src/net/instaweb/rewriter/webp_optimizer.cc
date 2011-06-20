/*
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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/webp_optimizer.h"

#include <csetjmp>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/image_compression/jpeg_reader.h"
extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"
#else
#include "third_party/libjpeg/jpeglib.h"
#endif
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/encode.h"
#else
#include "third_party/libwebp/webp/encode.h"
#endif
// TODO(jmaessen): open source imports & build of libwebp.
}

namespace net_instaweb {

namespace {

// Whether to enable support for YUV -> YUV conversion.  Is currently disabled,
// as trials showed colorspace mismatches in jpeg versus webp.
const bool kUseYUV = false;

// The YUV samples emerging from libjpeg are packed in that order (rather than
// being represented as three distinct planes, which is what libwebp does).
// We define the following constants to make reading array indexing code
// marginally easier.
const int kYPlane = 0;
const int kUPlane = 1;
const int kVPlane = 2;
const int kPlanes = 3;

#ifdef DCT_IFAST_SUPPORTED
const J_DCT_METHOD fastest_dct_method = JDCT_IFAST;
#else
#ifdef DCT_FLOAT_SUPPORTED
// const J_DCT_METHOD fastest_dct_method = JDCT_FLOAT;
#else
// const J_DCT_METHOD fastest_dct_method = JDCT_ISLOW;
#endif
#endif


int GoogleStringWebpWriter(const uint8* data, size_t data_size,
                           const WebPPicture* const picture) {
  GoogleString* compressed_webp =
      static_cast<GoogleString*>(picture->custom_ptr);
  // The cast below deals with char signedness issues (data is always unsigned)
  compressed_webp->append(reinterpret_cast<const char*>(data), data_size);
  return 1;
}

class WebpOptimizer {
 public:
  WebpOptimizer();
  ~WebpOptimizer();

  // Take the given input file and transcode it to webp.
  // Return true on success.
  bool CreateOptimizedWebp(const GoogleString& original_jpeg,
                           GoogleString* compressed_webp);

 private:
  // Compute the offset of a pixel sample given x and y position.
  size_t PixelOffset(size_t x, size_t y) const {
    return (kPlanes * x + y * row_stride_);
  }
  // Fetch a pixel sample from the given plane and offset, modified by the given
  // 0/1 x and y offsets.  Note that all the arguments here are expected to be
  // constant except source_offset.
  int SampleAt(int plane, int source_offset, int x_offset, int y_offset) const {
    return static_cast<int>(pixels_[plane + source_offset +
                                    PixelOffset(x_offset, y_offset)]);
  }
  bool DoReadJpegPixels(J_COLOR_SPACE color_space,
                        const GoogleString& original_jpeg);
  bool ReadJpegPixels(J_COLOR_SPACE color_space,
                      const GoogleString& original_jpeg);
  bool WebPImportYUV(WebPPicture* const picture);

  // Structure for jpeg decompression
  pagespeed::image_compression::JpegReader reader_;
  uint8* pixels_;
  uint8** rows_;  // Holds offsets into pixels_ during decompression
  unsigned int width_, height_;  // Type-compatible with libjpeg.
  size_t row_stride_;
  // Structures for webp recompression

  DISALLOW_COPY_AND_ASSIGN(WebpOptimizer);
};  // class WebpOptimizer

WebpOptimizer::WebpOptimizer() : pixels_(NULL), rows_(NULL) { }
WebpOptimizer::~WebpOptimizer() {
  delete[] pixels_;
  DCHECK(rows_ == NULL);
}

// Does most of the work of ReadJpegPixels (see below); errors transfer control
// out so that we can clean up properly.
bool WebpOptimizer::DoReadJpegPixels(J_COLOR_SPACE color_space,
                                     const GoogleString& original_jpeg) {
  // Set up jpeg error handling.
  jmp_buf env;
  if (setjmp(env)) {
    // We get here if libjpeg encountered a decompression error.
    return false;
  }
  // Install env so that it is longjmp'd to on error:
  jpeg_decompress_struct* jpeg_decompress = reader_.decompress_struct();
  jpeg_decompress->client_data = static_cast<void*>(&env);

  reader_.PrepareForRead(original_jpeg);

  if (jpeg_read_header(jpeg_decompress, TRUE) != JPEG_HEADER_OK) {
    return false;
  }

  // Settings largely cribbed from the cwebp.c example source code.
  // Difference: we ask for YCbCr as the out_color_space.  Not sure
  // why RGB is used in the command line utility.  Is this so we handle
  // non-YCbCr jpegs gracefully without additional checking?
  jpeg_decompress->out_color_space = color_space;
  // For whatever reason, libjpeg doesn't always seem to define JDCT_FASTEST to
  // match a *configured, working* dct method (which makes this symbol pretty
  // pointless, actually)!  As a result, we end up having to use the default
  // (slow and conservative) method.  This is horrible and broken.
  //  jpeg_decompress->dct_method = JDCT_FASTEST;
  jpeg_decompress->do_fancy_upsampling = TRUE;

  if (!jpeg_start_decompress(jpeg_decompress) ||
      jpeg_decompress->output_components != kPlanes) {
    return false;
  }

  // Figure out critical dimensions of image, and allocate space for image data.
  width_ = jpeg_decompress->output_width;
  height_ = jpeg_decompress->output_height;
  row_stride_ = width_ * jpeg_decompress->output_components * sizeof(*pixels_);

  pixels_ = new uint8[row_stride_ * height_];
  // jpeglib expects to get an array of pointers to rows, so allocate one and
  // point it to contiguous rows in *pixels_.
  rows_ = new uint8*[height_];
  for (unsigned int i = 0; i < height_; ++i) {
    rows_[i] = pixels_ + PixelOffset(0, i);
  }
  while (jpeg_decompress->output_scanline < height_) {
    // Try to read all remaining lines; we should get as many as the library is
    // comfortable handing over at one go.
    int rows_read =
        jpeg_read_scanlines(jpeg_decompress,
                            rows_ + jpeg_decompress->output_scanline,
                            height_ - jpeg_decompress->output_scanline);
    if (rows_read == 0) {
      return false;
    }
  }
  return jpeg_finish_decompress(jpeg_decompress);
}

// Initialize width_, height_, row_stride_, and pixels_ with data from the
// jpeg_decompress structure.  Returns a status for errors that are caught in
// our code.  Jpeglib errors are handled by longjmp-ing to internal handler
// code.  We rely on the destructor to clean up pixel data after an error.
//
// Most of the work is done in DoReadJpegPixels, with errors ending up out here
// where we can clean them up.  This avoids stack variable trouble if
// decompression fails and longjmps.
bool WebpOptimizer::ReadJpegPixels(J_COLOR_SPACE color_space,
                                   const GoogleString& original_jpeg) {
  bool read_ok = DoReadJpegPixels(color_space, original_jpeg);
  delete[] rows_;
  rows_ = NULL;
  jpeg_decompress_struct* jpeg_decompress = reader_.decompress_struct();
  // NULL out the setjmp information stored by DoReadJpegPixels; there should be
  // no further decompression failures, and the stack would be invalid if there
  // were.
  jpeg_decompress->client_data = NULL;
  jpeg_destroy_decompress(jpeg_decompress);
  return read_ok;
}

// Import YUV pixels_ into *picture, downsampling UV as appropriate.  This is
// based on the RGB downsampling code in libwebp v0.2 src/enc/picture.c, but
// there's annoyingly no YUV downsampling code there.
// If WebPImportYUV succeeds, picture will have bitmaps allocated and must
// be cleaned up using WebPPictureFree(...).
bool WebpOptimizer::WebPImportYUV(WebPPicture* const picture) {
  if (!WebPPictureAlloc(picture)) {
    return false;
  }
  // Luma (Y) import
  for (size_t y = 0; y < height_; ++y) {
    for (size_t x = 0; x < width_; ++x) {
      picture->y[x + y * picture->y_stride] =
          pixels_[kYPlane + PixelOffset(x, y)];
    }
  }
  // Downsample U and V, handling boundaries.  Better averaging is a TODO
  // in the webp code, so this may need to change in future.
  unsigned int half_height = height_ >> 1;
  unsigned int half_width = width_ >> 1;
  unsigned int extra_height = height_ & 1;
  unsigned int extra_width = width_ & 1;
  size_t x, y;
  // Note that to preserve similar structure in the edge cases below We rely on
  // overincrement of x and y and use them after the loop terminates.  This
  // should make it easier to understand the loop structures.
  for (y = 0; y < half_height; ++y) {
    for (x = 0; x < half_width; ++x) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 1, 0) +
          SampleAt(kUPlane, source_offset, 0, 1) +
          SampleAt(kUPlane, source_offset, 1, 1);
      picture->u[picture_offset] = (2 + pixel_sum_u) >> 2;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 1, 0) +
          SampleAt(kVPlane, source_offset, 0, 1) +
          SampleAt(kVPlane, source_offset, 1, 1);
      picture->v[picture_offset] = (2 + pixel_sum_v) >> 2;
    }
    // Note: x == half_width
    if (extra_width != 0) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 0, 1);
      picture->u[picture_offset] = (1 + pixel_sum_u) >> 1;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 0, 1);
      picture->v[picture_offset] = (1 + pixel_sum_v) >> 1;
    }
  }
  if (extra_height != 0) {
    // Note: y == half_height
    for (x = 0; x < half_width; ++x) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 1, 0);
      picture->u[picture_offset] = (1 + pixel_sum_u) >> 1;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 1, 0);
      picture->v[picture_offset] = (1 + pixel_sum_v) >> 1;
    }
    // Note: x == half_width
    if (extra_width != 0) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0);
      picture->u[picture_offset] = pixel_sum_u;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0);
      picture->v[picture_offset] = pixel_sum_v;
    }
  }
  return true;
}

// Main body of transcode.
bool WebpOptimizer::CreateOptimizedWebp(
    const GoogleString& original_jpeg, GoogleString* compressed_webp) {
  // Begin by making sure we can create a webp image at all:
  WebPPicture picture;
  WebPConfig config;
  if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) {
    // Version mismatch.
    return false;
  } else if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, config.quality)) {
    // Couldn't use the default preset.
    return false;
  } else if (!WebPValidateConfig(&config)) {
    return false;
  }

  J_COLOR_SPACE color_space = kUseYUV ? JCS_YCbCr : JCS_RGB;

  if (!ReadJpegPixels(color_space, original_jpeg)) {
    return false;
  }

  // At this point, we're done reading the jpeg, and the color data
  // is stored in *pixels.  Now we just need to turn this into a webp.
  // Regardless of the import method we use, we need to set the picture
  // up beforehand as follows:
  picture.writer = &GoogleStringWebpWriter;
  picture.custom_ptr = static_cast<void*>(compressed_webp);
  picture.width = width_;
  picture.height = height_;

  if (kUseYUV) {
    // pixels_ are YUV at full resolution; WebP requires us to downsample the U
    // and V planes explicitly (and store the three planes separately).
    if (!WebPImportYUV(&picture)) {
      return false;
    }
  } else if (!WebPPictureImportRGB(&picture, pixels_, row_stride_)) {
    return false;
  }

  // We're done with the original pixels, so clean them up.  If an error occurs,
  // this cleanup will happen in the destructor instead.
  delete[] pixels_;
  pixels_ = NULL;

  // Now we need to take picture and WebP encode it.
  bool result = WebPEncode(&config, &picture);

  // Clean up the picture and return status.
  WebPPictureFree(&picture);

  return result;
}

}  // namespace

bool OptimizeWebp(const GoogleString& original_jpeg,
                  GoogleString* compressed_webp) {
  WebpOptimizer optimizer;
  return optimizer.CreateOptimizedWebp(original_jpeg, compressed_webp);
}

}  // namespace net_instaweb
