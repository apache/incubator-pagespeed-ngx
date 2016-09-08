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

// Author: Victor Chudnovsky

#ifndef PAGESPEED_KERNEL_IMAGE_WEBP_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_WEBP_OPTIMIZER_H_

#include <cstddef>
#include "third_party/libwebp/src/webp/encode.h"
#include "third_party/libwebp/src/webp/mux.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

struct WebpConfiguration : public ScanlineWriterConfig {
  // This contains a subset of the options in WebPConfig and
  // WebPPicture.

  typedef bool (*WebpProgressHook)(int percent, void* user_data);

  WebpConfiguration()
      : lossless(true), quality(75), method(3), target_size(0),
        alpha_compression(1), alpha_filtering(1), alpha_quality(100),
        kmin(0), kmax(0), progress_hook(NULL), user_data(NULL) {}

  ~WebpConfiguration() override;

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

  // Parameters related to animated WebP:
  size_px kmin;           // Minimum keyframe interval, i.e., number of
                          // non-keyframes between consecutive keyframes. If
                          // kmin == 0, keyframes are not used. Libwebp
                          // requires kmax > kmin >= (kmax / 2) + 1. Reasonable
                          // choices are (3,5) for lossy encoding and (9,17)
                          // for lossless encoding.
  size_px kmax;           // Maximum keyframe interval.

  WebpProgressHook progress_hook;   // If non-NULL, called during encoding.

  void* user_data;        // Can be used by progress_hook. This
                          // pointer remains owned by the client and
                          // must remain valid until
                          // WebpScanlineWriter::FinalizeWrite()
                          // completes.

  // NOTE: If you add more fields to this struct that feed into
  // WebPConfig, please update the CopyTo() method.
};

class WebpFrameWriter : public MultipleFrameWriter {
 public:
  explicit WebpFrameWriter(MessageHandler* handler);
  virtual ~WebpFrameWriter();

  // Sets the WebP configuration to be 'config', which should be a
  // WebpConfiguration* and should not be NULL.
  virtual ScanlineStatus Initialize(const void* config, GoogleString* out);

  // image_spec must remain valid for the lifetime of
  // WebpFrameWriter.
  virtual ScanlineStatus PrepareImage(const ImageSpec* image_spec);

  // frame_spec must remain valid while the frame is being written.
  virtual ScanlineStatus PrepareNextFrame(const FrameSpec* frame_spec);

  virtual ScanlineStatus WriteNextScanline(const void *scanline_bytes);

  // Note that even after WriteNextScanline() has been called,
  // Initialize() and FinalizeWrite() may be called repeatedly to
  // write the image with, say, different configs.
  virtual ScanlineStatus FinalizeWrite();

 private:
  // The function to be called by libwebp's progress hook (with 'this'
  // as the user data), which in turn will call the user-supplied function
  // in progress_hook_, passing it progress_hook_data_.
  static int ProgressHook(int percent, const WebPPicture* picture);

  // Commits the just-read frame to the animation cache.
  ScanlineStatus CacheCurrentFrame();

  // Utility function to deallocate libwebp-defined data structures.
  void FreeWebpStructs();

  // This class does NOT own image_spec_.
  const ImageSpec* image_spec_;
  FrameSpec frame_spec_;

  // Zero-based index of the next frame (after the current one) to be
  // written.
  size_px next_frame_;

  // Zero-based index of the next scanline to be written.
  size_px next_scanline_;

  // Flag to indicate whether the current frame is empty, due to at
  // least one of its dimensions being zero. Note that all frames must
  // fit completely within their image (see the comment in
  // image_frame_interface.h), so out-of-bounds frames are not
  // considered here.
  bool empty_frame_;

  // Number of pixels to advance by exactly one row.
  size_px frame_stride_px_;

  // Pointer to the next pixel to be written via WriteNextScanline().
  // If the frame is offset (top or left != 0), this pointer is also offset.
  uint32_t* frame_position_px_;

  // The number of bytes per pixel in the current frame.
  uint32_t frame_bytes_per_pixel_;

  // libwebp objects for the WebP generation.
  WebPPicture webp_image_;

  // Last frame image when DISPOSAL_RESTORE is in-use.
  WebPPicture* webp_image_restore_;

  // FrameSpec of previous frame.
  FrameSpec previous_frame_spec_;

  // Encodes to WebP for animated images. Null for static images.
  WebPAnimEncoder* webp_encoder_;

  // Configuration for webp encoder.
  WebPConfig libwebp_config_;

  // Timestamp for the current animation frame.
  int timestamp_;

#ifndef NDEBUG
  WebPAuxStats stats_;
#endif

  // Pointer to the webp output.
  GoogleString* output_image_;

  // Whether the image has an alpha channel.
  bool has_alpha_;

  // Whether PrepareImage() has been called successfully.
  bool image_prepared_;

  // The user-supplied progress hook.
  WebpConfiguration::WebpProgressHook progress_hook_;

  // The user-supplied user data for progress_hook. This pointer must
  // remain valid until FinalizeWrite() completes. This class does NOT
  // take ownership of this pointer.
  void* progress_hook_data_;

  // WebP does not have native support for gray scale images. The workaround
  // is to replicate the luminance to RGB; then WebP can compress the expanded
  // images efficiently.
  bool should_expand_gray_to_rgb_;

  // Min and max keyframe interval values. Only applicable for animated webp.
  size_px kmin_;
  size_px kmax_;

  DISALLOW_COPY_AND_ASSIGN(WebpFrameWriter);
};

// WebpScanlineReader decodes WebP images. It returns a scanline (a row of
// pixels) each time it is called. The output format is RGB_888 if the input
// image does not have alpha channel, or RGBA_8888 otherwise. Animated WebP
// is not supported.
class WebpScanlineReader : public ScanlineReaderInterface {
 public:
  explicit WebpScanlineReader(MessageHandler* handler);
  virtual ~WebpScanlineReader();

  // Reset the scanline reader to its initial state.
  virtual bool Reset();

  // Initialize the reader with the given image stream. Note that image_buffer
  // must remain unchanged until the *first* call to ReadNextScanline().
  virtual ScanlineStatus InitializeWithStatus(const void* image_buffer,
                                              size_t buffer_length);

  // Return the next row of pixels. The entire image is decoded the first
  // time ReadNextScanline() is called, but only one scanline is returned
  // for each call.
  virtual ScanlineStatus ReadNextScanlineWithStatus(void** out_scanline_bytes);

  // Return the number of bytes in a row (without padding).
  virtual size_t GetBytesPerScanline() { return bytes_per_row_; }

  virtual bool HasMoreScanLines() { return (row_ < height_); }
  virtual PixelFormat GetPixelFormat() { return pixel_format_; }
  virtual size_t GetImageHeight() { return height_; }
  virtual size_t GetImageWidth() {  return width_; }
  // WebP does not have progressive mode.
  virtual bool IsProgressive() { return false; }

 private:
  // Buffer and length of the input (compressed) image.
  const uint8_t* image_buffer_;
  int buffer_length_;

  PixelFormat pixel_format_;
  size_t height_;
  size_t width_;
  size_t bytes_per_row_;
  size_t row_;
  bool was_initialized_;

  // Buffer for holding the decoded pixels.
  net_instaweb::scoped_array<uint8_t> pixels_;

  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebpScanlineReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_WEBP_OPTIMIZER_H_
