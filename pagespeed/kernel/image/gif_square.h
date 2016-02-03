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

#ifndef PAGESPEED_KERNEL_IMAGE_GIF_SQUARE_H_
#define PAGESPEED_KERNEL_IMAGE_GIF_SQUARE_H_

#include <cstddef>
#include <vector>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_util.h"

extern "C" {
#include "third_party/giflib/lib/gif_lib.h"
}

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {
namespace image_compression {

class GifSquare {
 public:
  GifSquare(bool manual_gcb, net_instaweb::MessageHandler* handler);
  ~GifSquare();

  // Opens a GIF file under the given 'filename'. Returns
  // true on success.
  bool Open(const GoogleString& filename);

  // Prepares the GIF screen to have dimensions 'width' by 'height', a
  // background color 'bg_color_idx' into the 'colormap' of size
  // 'num_colors' (a power of 2) that is used for the image. It marks
  // this as a GIF89 image if 'gif89' is set. If loop_count is less
  // than kNoLoopCountSpecified, it inserts an application extension
  // block indicating to loop the animation forever (if zero) or the
  // specified number of times. Returns true on success.
  //
  // NOTE: Chrome (at least v35) interprets the loop_count as the
  // number of times to *repeat* the animation, so the user will see
  // the animation loop_count+1 times.
  bool PrepareScreen(bool gif89, size_px width, size_px height,
                     const GifColorType* color_map, int num_colors,
                     int bg_color_idx, size_t loop_count);

  // Puts a single GIF image. The image is a 'width' by 'height'
  // rectangle whose color is given by the 'color_index'-th entry of
  // the 'colormap', is positioned at position 'left', 'top' from the
  // top left of the GIF screen, and will be discarded as per
  // 'disposal_method'. The 'transparent_idx'-th color is marked as
  // being fully transparent. The image is marked as being interlaced
  // if 'interlace' is set, and is displayed for a duration of
  // 'delay_cs' centiseconds.
  //
  // Returns true on success.
  bool PutImage(size_px left, size_px top, size_px width, size_px height,
                const GifColorType* colormap, int num_colors,
                int color_index, int transparent_idx,
                bool interlace, int delay_cs, int disposal_method);

  // Animates all GIF images by specifying that they should each be
  // displayed for 'delay_cs' centiseconds, be disposed according to
  // 'disposal_method', and have the 'transparent_idx' color of the
  // previously-specified colormap be transparent
  bool AnimateAllImages(int delay_cs, int transparent_idx,
                        int disposal_method);

  // Flushes and closes the GIF file.
  bool Close();

  static const size_t kNoLoopCountSpecified;
  static const GifColorType kGifWhite;
  static const GifColorType kGifBlack;
  static const GifColorType kGifGray;
  static const GifColorType kGifRed;
  static const GifColorType kGifGreen;
  static const GifColorType kGifBlue;
  static const GifColorType kGifYellow;

 private:
  //  Whether to insert the GCB blocks manually. This is needed for
  //  now to get non-zero animation delays to work.
  //  (EGifGCBToSavedExtension seems to not save animation delays.)
  bool manual_gcb_;
  net_instaweb::MessageHandler* handler_;
  bool success_;
  GifFileType* gif_file_;
  int num_images_;
  bool closed_;
  std::vector<ColorMapObject*> colormaps_;

  // If 'success' is false, writes 'prefix' and the GIF file error
  // message to the error log and sets success_ to false. Returns
  // 'success'.
  bool Log(bool success, const char* prefix);

  // Logs 'prefix' and 'message' to error logs, sets success_ to
  // false, and returns false.
  bool Fail(const char* prefix, const char* message);

  bool CanProceed() {
    return success_ && !closed_;
  }
};

}  // namespace image_compression
}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_GIF_SQUARE_H_
