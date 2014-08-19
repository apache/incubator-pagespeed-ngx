/*
 * Copyright 2010 Google Inc.
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

// Author: Bryan McQuade, Matthew Steele

#ifndef PAGESPEED_KERNEL_IMAGE_JPEG_READER_H_
#define PAGESPEED_KERNEL_IMAGE_JPEG_READER_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

struct jpeg_decompress_struct;
struct jpeg_error_mgr;

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

struct JpegEnv;

// A very thin wrapper that configures a jpeg_decompress_struct for
// reading from a string. The caller is responsible for
// configuring a jmp_buf and setting it as the client_data of the
// jpeg_decompress_struct, like so:
//
// jmp_buf env;
// if (setjmp(env)) {
//   // error handling
// }
// JpegReader reader;
// jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();
// jpeg_decompress->client_data = static_cast<void*>(&env);
// reader.PrepareForRead(src);
// // perform other operations on jpeg_decompress.
class JpegReader {
 public:
  explicit JpegReader(MessageHandler* handler);
  ~JpegReader();

  jpeg_decompress_struct *decompress_struct() const { return jpeg_decompress_; }

  void PrepareForRead(const void* image_data, size_t image_length);

 private:
  jpeg_decompress_struct *jpeg_decompress_;
  jpeg_error_mgr *decompress_error_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(JpegReader);
};

// JpegScanlineReader decodes JPEG image. It returns a scanline (a row of
// pixels) each time it is called. The output format is GRAY_8 if the input
// image has JCS_GRAYSCALE format, or RGB_888 otherwise.
class JpegScanlineReader : public ScanlineReaderInterface {
 public:
  explicit JpegScanlineReader(MessageHandler* handler);
  virtual ~JpegScanlineReader();
  virtual bool Reset();

  // Initialize the reader with the given image stream. Note that image_buffer
  // must remain unchanged until the last call to ReadNextScanline().
  virtual ScanlineStatus InitializeWithStatus(const void* image_buffer,
                                              size_t buffer_length);

  // Return the next row of pixels.
  virtual ScanlineStatus ReadNextScanlineWithStatus(void** out_scanline_bytes);

  // Return the number of bytes in a row (without padding).
  virtual size_t GetBytesPerScanline() { return bytes_per_row_; }

  virtual bool HasMoreScanLines() { return (row_ < height_); }
  virtual PixelFormat GetPixelFormat() { return pixel_format_; }
  virtual size_t GetImageHeight() { return height_; }
  virtual size_t GetImageWidth() {  return width_; }
  virtual bool IsProgressive() { return is_progressive_; }

 private:
  JpegEnv* jpeg_env_;  // State of libjpeg
  unsigned char* row_pointer_[1];  // Pointer for a row buffer
  PixelFormat pixel_format_;
  size_t height_;
  size_t width_;
  size_t row_;
  size_t bytes_per_row_;
  bool was_initialized_;
  bool is_progressive_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(JpegScanlineReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_JPEG_READER_H_
