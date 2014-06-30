/*
 * Copyright 2014 Google Inc.
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

#include "pagespeed/kernel/image/gif_square.h"

#include <math.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

using net_instaweb::MessageHandler;

namespace pagespeed {
namespace image_compression {

const size_t GifSquare::kNoLoopCountSpecified = ~0;
const GifColorType GifSquare::kGifWhite = {0xFF, 0xFF, 0xFF};
const GifColorType GifSquare::kGifBlack = {0x00, 0x00, 0x00};
const GifColorType GifSquare::kGifGray = {0xF0, 0xF0, 0xF0};
const GifColorType GifSquare::kGifRed = {0xFF, 0x00, 0x00};
const GifColorType GifSquare::kGifGreen = {0x00, 0xFF, 0x00};
const GifColorType GifSquare::kGifBlue = {0x00, 0x00, 0xFF};
const GifColorType GifSquare::kGifYellow = {0xFF, 0xFF, 0x00};

#define LOBYTE(x)       ((x) & 0xff)
#define HIBYTE(x)       (((x) >> 8) & 0xff)

#if GIFLIB_MAJOR < 5
namespace {
// Later versions of gif_lib use a Gif prefix for these functions.

// In particular, the google3 version has the prefix but the    // [google3]
// open-source pagespeed version does not.                      // [google3]
ColorMapObject* (*GifMakeMapObject)(int, const GifColorType*) =
    MakeMapObject;
void (*GifFreeMapObject)(ColorMapObject*) = FreeMapObject;

// Borrowed from later versions of giflib.
typedef struct GraphicsControlBlock {
  int DisposalMode;
#define DISPOSAL_UNSPECIFIED      0       /* No disposal specified. */
#define DISPOSE_DO_NOT            1       /* Leave image in place */
#define DISPOSE_BACKGROUND        2       /* Set area too background color */
#define DISPOSE_PREVIOUS          3       /* Restore to previous content */
  bool UserInputFlag;      /* User confirmation required before disposal */
  int DelayTime;           /* pre-display delay in 0.01sec units */
  int TransparentColor;    /* Palette index for transparency, -1 if none */
#define NO_TRANSPARENT_COLOR    -1
} GraphicsControlBlock;

size_t EGifGCBToExtension(const GraphicsControlBlock *GCB,
                          GifByteType *GifExtension) {
  GifExtension[0] = 0;
  GifExtension[0] |= (GCB->TransparentColor == NO_TRANSPARENT_COLOR) ?
      0x00 : 0x01;
  GifExtension[0] |= GCB->UserInputFlag ? 0x02 : 0x00;
  GifExtension[0] |= ((GCB->DisposalMode & 0x07) << 2);
  GifExtension[1] = LOBYTE(GCB->DelayTime);
  GifExtension[2] = HIBYTE(GCB->DelayTime);
  GifExtension[3] = static_cast<char>(GCB->TransparentColor);
  return 4;
}
}  // namespace
#endif

GifSquare::GifSquare(bool manual_gcb,
                     MessageHandler* handler) :
    manual_gcb_(manual_gcb),
    handler_(handler),
    success_(true), gif_file_(NULL),
    num_images_(0), closed_(false) {
}

GifSquare::~GifSquare() {
  Close();
  for_each(colormaps_.begin(), colormaps_.end(), GifFreeMapObject);
  colormaps_.clear();
}

bool GifSquare::Open(const GoogleString& filename) {
#if GIFLIB_MAJOR >= 5
  int status = 0;
  gif_file_ = EGifOpenFileName(filename.c_str(), false,
                               &status);
  if (status != 0) {
    return Fail("EGifOpenFileName", GifErrorString(status));
  }
  return true;
#else
  gif_file_ = EGifOpenFileName(filename.c_str(), false);
  return Log(gif_file_ != NULL, "EGifOpenFileName");
#endif
}

bool GifSquare::PrepareScreen(bool gif89, size_px width, size_px height,
                              const GifColorType* color_map, int num_colors,
                              int bg_color_idx, size_t loop_count) {
  if (!CanProceed()) {
    return false;
  }

  if (num_colors & (num_colors-1)) {
    return Fail("num_colors", "not a power of 2");
  }
  int color_resolution = static_cast<int>(round(log2(num_colors))) - 1;

  colormaps_.clear();
  colormaps_.push_back(GifMakeMapObject(num_colors, color_map));

#if GIFLIB_MAJOR >= 5
  colormaps_[0]->SortFlag = 0;  // not initialized above
  EGifSetGifVersion(gif_file_, gif89);
#else
  EGifSetGifVersion("89a");
#endif

  if (!Log(EGifPutScreenDesc(gif_file_, width, height,
                             color_resolution, bg_color_idx, colormaps_[0]),
           "EGifPutScreenDesc")) {
    return false;
  }

  if (loop_count < kNoLoopCountSpecified) {
    // The loop count is encoded as a series of three bytes: a literal
    // '\x01' and then the count in lo-hi order. Cf. http://shortn/_19L2jvGJc9
    static const int kLen = 3;
    const uint8_t app_block_content[kLen] = {
      0x01,
      static_cast<uint8_t>(LOBYTE(loop_count)),
      static_cast<uint8_t>(HIBYTE(loop_count))
    };
#if GIFLIB_MAJOR >= 5
    return (
        Log(EGifPutExtensionLeader(gif_file_,
                                   APPLICATION_EXT_FUNC_CODE) &&
            EGifPutExtensionBlock(gif_file_, 11, "NETSCAPE2.0") &&
            EGifPutExtensionBlock(gif_file_, kLen,
                                  app_block_content) &&
            EGifPutExtensionTrailer(gif_file_),
            "EGifPutExtension*: loop count"));
#else
    return Log(EGifPutExtensionFirst(gif_file_,
                                     APPLICATION_EXT_FUNC_CODE,
                                     11, "NETSCAPE2.0") &&
               EGifPutExtensionLast(gif_file_, 0 /* not used */,
                                    kLen, app_block_content),
               "EGifPutExtension*: loop count");
#endif
  }

  return true;
}

bool GifSquare::PutImage(size_px left, size_px top,
                         size_px width, size_px height,
                         const GifColorType* colormap, int num_colors,
                         int color_index, int transparent_idx,
                         bool interlace, int delay_cs, int disposal_method) {
  if (!CanProceed()) {
    return false;
  }

  // If an animation delay was specified, we need to add a
  // GraphicsControlBlock.
  //
  // TODO(vchudnov): Ideally, we would not do this in this
  // function. See the comments in AnimateAllImages.
  if (manual_gcb_ &&
      (delay_cs >= 0 || transparent_idx >= 0 || disposal_method >= 0)) {
    GraphicsControlBlock gcb;
    gcb.DisposalMode = disposal_method;
    gcb.UserInputFlag = false;
    gcb.DelayTime = delay_cs;
    gcb.TransparentColor = transparent_idx;

    ExtensionBlock ext;
    size_t len = EGifGCBToExtension(&gcb, reinterpret_cast<GifByteType*>(&ext));
    if (!Log(EGifPutExtension(gif_file_, GRAPHICS_EXT_FUNC_CODE, len, &ext),
             "GCB status")) {
      return false;
    }
  }

  // egif_lib.c only clears the image colormap (allocated by
  // EGifPutImageDesc) in EGifCloseFile. If we are dealing with
  // animated GIFs, we thus need to clear it manually to prevent a
  // memory leak.
  if (gif_file_->Image.ColorMap != NULL) {
    GifFreeMapObject(gif_file_->Image.ColorMap);
    gif_file_->Image.ColorMap = NULL;
  }

  ColorMapObject* cmap = ((colormap != NULL && num_colors > 0) ?
                          GifMakeMapObject(num_colors, colormap) : NULL);
  if (!Log(EGifPutImageDesc(gif_file_, left, top,
                            width, height,
                            interlace, cmap),
           "EGifPutImageDesc")) {
    return false;
  }
  colormaps_.push_back(cmap);

  int num_pixels = width * height;
  for (int i = 0; i < num_pixels; ++i) {
    if (!Log(EGifPutPixel(gif_file_, color_index), "EGifPutPixel")) {
      return false;
    }
  }
  ++num_images_;

  return true;
}

bool GifSquare::AnimateAllImages(int delay_cs, int transparent_idx,
                                 int disposal_method) {
  if (!CanProceed()) {
    return false;
  }
  return false;

  // TODO(vchudnov): This does not yet work, so for the moment we're
  // adding the Graphics Control Blocks manually.
  if (!manual_gcb_ && delay_cs >= 0) {
    for (int j = 0; j < num_images_; ++j) {
      GraphicsControlBlock gcb;

      gcb.DisposalMode = disposal_method;
      gcb.UserInputFlag = false;
      gcb.DelayTime = delay_cs;
      gcb.TransparentColor = transparent_idx;

#if GIFLIB_MAJOR >= 5
      if (!Log(EGifGCBToSavedExtension(&gcb, gif_file_, j), "GCB status")) {
        return false;
      }
#endif
    }
  }
  return true;
}

bool GifSquare::Close() {
  if (!CanProceed()) {
    return false;
  }
  if (!closed_ &&
      (gif_file_ != NULL) &&
      !Log(EGifCloseFile(gif_file_), "EGifCloseFile")) {
    return false;
  }
  closed_ = true;
  return true;
}

bool GifSquare::Log(bool success, const char* prefix) {
#if GIFLIB_MAJOR >= 5
  return success ? success : Fail(prefix, ((gif_file_ != NULL) ?
                                           GifErrorString(gif_file_->Error) :
                                           "(?)"));
#else
  return success ? success : Fail(prefix, "");
#endif
}

bool GifSquare::Fail(const char* prefix, const char* message) {
  PS_LOG_ERROR(handler_, "Failure: %s: %s", prefix, message);
  success_ = false;
  return false;
}

}  // namespace image_compression
}  // namespace pagespeed
