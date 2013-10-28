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
#include "third_party/libwebp/webp/encode.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

struct WebpConfiguration {
  // This contains a subset of the options in WebPConfig and
  // WebPPicture.

  typedef bool (*WebpProgressHook)(int percent, void* user_data);

  WebpConfiguration()
      : lossless(true), quality(75), method(3), target_size(0),
        alpha_compression(1), alpha_filtering(1), alpha_quality(100),
        progress_hook(NULL), user_data(NULL) {}
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

  WebpProgressHook progress_hook;   // If non-NULL, called during encoding.

  void* user_data;        // Can be used by progress_hook. This
                          // pointer remains owned by the client and
                          // must remain valid until
                          // WebpScanlineWriter::FinalizeWrite()
                          // completes.

  // NOTE: If you add more fields to this struct that feed into
  // WebPConfig, please update the CopyTo() method.
};


class WebpScanlineWriter : public ScanlineWriterInterface {
 public:
  explicit WebpScanlineWriter(MessageHandler* handler);
  virtual ~WebpScanlineWriter();

  virtual ScanlineStatus InitWithStatus(const size_t width, const size_t height,
                                        PixelFormat pixel_format);
  // Sets the WebP configuration to be 'params', which should be a
  // WebpConfiguration* and should not be NULL.
  virtual ScanlineStatus InitializeWriteWithStatus(const void* params,
                                                   GoogleString* const out);
  virtual ScanlineStatus WriteNextScanlineWithStatus(void *scanline_bytes);

  // Note that even after WriteNextScanline() has been called,
  // InitializeWrite() and FinalizeWrite() may be called repeatedly to
  // write the image with, say, different configs.
  virtual ScanlineStatus FinalizeWriteWithStatus();

  MessageHandler* message_handler() {
    return message_handler_;
  }

 private:
  // Number of bytes per row. See
  // https://developers.google.com/speed/webp/docs/api#encodingapi
  int stride_bytes_;

  // Allocated pointer to the start of memory for the RGB(A) data.
  uint8_t* rgb_;

  // Pointer to the end of the RGB(A) data (i.e. to the first byte
  // after the allocated memory).
  uint8_t* rgb_end_;

  // Pointer to the next byte of RGB(A) data to be filled in.
  uint8_t* position_bytes_;

  // libwebp objects for the WebP generation.
  WebPPicture picture_;
  WebPConfig* config_;
#ifndef NDEBUG
  WebPAuxStats stats_;
#endif

  // Pointer to the webp output.
  GoogleString* webp_image_;

  // Whether the image has an alpha channel.
  bool has_alpha_;

  // Whether Init() has been called successfully.
  bool init_ok_;

  // Whether WebPPictureImport* has been called.
  bool imported_;

  // Whether all the scanlines have been written.
  bool got_all_scanlines_;

  // The user-supplied progress hook.
  WebpConfiguration::WebpProgressHook progress_hook_;

  // The user-supplied user data for progress_hook. This pointer must
  // remain valid until FinalizeWrite() completes. This class does NOT
  // take ownership of this pointer.
  void* progress_hook_data_;

  // The function to be called by libwebp's progress hook (with 'this'
  // as the user data), which in turn will call the user-supplied function
  // in progress_hook_, passing it progress_hook_data_.
  static int ProgressHook(int percent, const WebPPicture* picture);

  // WebP does not have native support for gray scale images. The workaround
  // is to replicate the luminance to RGB; then WebP can compress the expanded
  // images efficiently.
  bool should_expand_gray_to_rgb_;

  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebpScanlineWriter);
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
  scoped_array<uint8_t> pixels_;

  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebpScanlineReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_WEBP_OPTIMIZER_H_
