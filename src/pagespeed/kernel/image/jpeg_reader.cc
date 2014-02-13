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

#include "pagespeed/kernel/image/jpeg_reader.h"

#include <setjmp.h>
#include <stdlib.h>

#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jerror.h"                                                 // NOLINT
#include "jpeglib.h"                                                // NOLINT
#else
#include "third_party/libjpeg_turbo/src/jerror.h"
#include "third_party/libjpeg_turbo/src/jpeglib.h"
#endif
}

namespace {

// Unfortunately, libjpeg normally only supports reading images from C FILE
// pointers, wheras we want to read from a C++ string.  Fortunately, libjpeg
// also provides an extension mechanism.  Below, we define a new kind of
// jpeg_source_mgr for reading from strings.

// The below code was adapted from the JPEGMemoryReader class that can be found
// in src/o3d/core/cross/bitmap_jpg.cc in the Chromium source tree (r29423).
// That code is Copyright 2009, Google Inc.

METHODDEF(void) InitSource(j_decompress_ptr cinfo) {}

METHODDEF(boolean) FillInputBuffer(j_decompress_ptr cinfo) {
  // Should not be called because we already have all the data
  ERREXIT(cinfo, JERR_INPUT_EOF);
  return TRUE;
}

METHODDEF(void) SkipInputData(j_decompress_ptr cinfo,
                              long num_bytes) {              // NOLINT
  jpeg_source_mgr &mgr = *(cinfo->src);
  const int bytes_remaining = mgr.bytes_in_buffer - num_bytes;
  mgr.bytes_in_buffer = bytes_remaining < 0 ? 0 : bytes_remaining;
  mgr.next_input_byte += num_bytes;
}

METHODDEF(void) TermSource(j_decompress_ptr cinfo) {}

// Call this function on a j_decompress_ptr to install a reader that will read
// from the given string.
void JpegStringReader(j_decompress_ptr cinfo,
                      const void* image_data,
                      size_t image_length) {
  if (cinfo->src == NULL) {
    cinfo->src = (struct jpeg_source_mgr*)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                  sizeof(jpeg_source_mgr));
  }
  struct jpeg_source_mgr &src = *(cinfo->src);

  src.init_source = InitSource;
  src.fill_input_buffer = FillInputBuffer;
  src.skip_input_data = SkipInputData;
  src.resync_to_restart = jpeg_resync_to_restart;  // default method
  src.term_source = TermSource;

  src.bytes_in_buffer = image_length;
  src.next_input_byte = static_cast<const JOCTET*>(image_data);
}

// ErrorExit() is installed as a callback, called on errors
// encountered within libjpeg.  The longjmp jumps back
// to the setjmp in JpegOptimizer::CreateOptimizedJpeg().
void ErrorExit(j_common_ptr jpeg_state_struct) {
  jmp_buf *env = static_cast<jmp_buf *>(jpeg_state_struct->client_data);
  (*jpeg_state_struct->err->output_message)(jpeg_state_struct);
  if (env)
    longjmp(*env, 1);
}

// OutputMessageFromReader is called by libjpeg code on an error when reading.
// Without this function, a default function would print to standard error.
void OutputMessage(j_common_ptr jpeg_decompress) {
  // The following code is handy for debugging.
  /*
  char buf[JMSG_LENGTH_MAX];
  (*jpeg_decompress->err->format_message)(jpeg_decompress, buf);
  DLOG(INFO) << "JPEG Reader Error: " << buf;
  */
}

}  // namespace

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

struct JpegEnv {
  jpeg_decompress_struct jpeg_decompress_;
  jpeg_error_mgr decompress_error_;
  jmp_buf jmp_buf_env_;
};

JpegReader::JpegReader(MessageHandler* handler)
  : message_handler_(handler) {
  jpeg_decompress_ = static_cast<jpeg_decompress_struct*>(
      malloc(sizeof(jpeg_decompress_struct)));
  decompress_error_ = static_cast<jpeg_error_mgr*>(
      malloc(sizeof(jpeg_error_mgr)));
  memset(jpeg_decompress_, 0, sizeof(jpeg_decompress_struct));
  memset(decompress_error_, 0, sizeof(jpeg_error_mgr));

  jpeg_decompress_->err = jpeg_std_error(decompress_error_);
  decompress_error_->error_exit = &ErrorExit;
  decompress_error_->output_message = &OutputMessage;
  jpeg_create_decompress(jpeg_decompress_);
}

JpegReader::~JpegReader() {
  jpeg_destroy_decompress(jpeg_decompress_);
  free(decompress_error_);
  free(jpeg_decompress_);
}

void JpegReader::PrepareForRead(const void* image_data, size_t image_length) {
  // Prepare to read from a string.
  JpegStringReader(jpeg_decompress_, image_data, image_length);
}

JpegScanlineReader::JpegScanlineReader(MessageHandler* handler) :
  jpeg_env_(NULL),
  pixel_format_(UNSUPPORTED),
  height_(0),
  width_(0),
  row_(0),
  bytes_per_row_(0),
  was_initialized_(false),
  message_handler_(handler) {
  row_pointer_[0] = NULL;
}

JpegScanlineReader::~JpegScanlineReader() {
  if (was_initialized_) {
    Reset();
  }
  free(jpeg_env_);
}

bool JpegScanlineReader::Reset() {
  pixel_format_ = UNSUPPORTED;
  height_ = 0;
  width_ = 0;
  row_ = 0;
  bytes_per_row_ = 0;
  was_initialized_ = false;

  jpeg_destroy_decompress(&(jpeg_env_->jpeg_decompress_));
  memset(jpeg_env_, 0, sizeof(JpegEnv));
  free(row_pointer_[0]);
  row_pointer_[0] = NULL;
  return true;
}

ScanlineStatus JpegScanlineReader::InitializeWithStatus(const void* image_data,
                                                        size_t image_length) {
  if (was_initialized_) {
    // Reset the reader if it has been initialized before.
    Reset();
  } else if (jpeg_env_ == NULL) {
    jpeg_env_ = static_cast<JpegEnv*>(malloc(sizeof(JpegEnv)));
    memset(jpeg_env_, 0, sizeof(JpegEnv));
  }

  // libjpeg's error handling mechanism requires that longjmp be used
  // to get control after an error.
  if (setjmp(jpeg_env_->jmp_buf_env_)) {
    // This code is run only when libjpeg hit an error and called
    // longjmp(env). It will reset the object to a state where it can be used
    // again.
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_JPEGREADER,
                            "libjpeg failed to decode the image.");
  }

  jpeg_error_mgr* decompress_error = &(jpeg_env_->decompress_error_);
  jpeg_decompress_struct* jpeg_decompress = &(jpeg_env_->jpeg_decompress_);
  jpeg_decompress->err = jpeg_std_error(decompress_error);
  decompress_error->error_exit = &ErrorExit;
  decompress_error->output_message = &OutputMessage;
  jpeg_create_decompress(jpeg_decompress);

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(jpeg_env_->jmp_buf_env_);

  // Prepare to read from a string.
  JpegStringReader(jpeg_decompress, image_data, image_length);

  // Read jpeg data into the decompression struct.
  jpeg_read_header(jpeg_decompress, TRUE);

  width_ = jpeg_decompress->image_width;
  height_ = jpeg_decompress->image_height;

  // Decode the image to GRAY_8 if it was in gray scale, or to RGB_888
  // otherwise.
  if (jpeg_decompress->jpeg_color_space == JCS_GRAYSCALE) {
    jpeg_decompress->out_color_space = JCS_GRAYSCALE;
    pixel_format_ = GRAY_8;
    bytes_per_row_ = width_;
  } else {
    jpeg_decompress->out_color_space = JCS_RGB;
    pixel_format_ = RGB_888;
    bytes_per_row_ = 3 * width_;
  }

  was_initialized_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus JpegScanlineReader::ReadNextScanlineWithStatus(
    void** out_scanline_bytes) {
  if (!was_initialized_ || !HasMoreScanLines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_JPEGREADER,
                            "The reader was not initialized or does not "
                            "have any more scanlines.");
  }

  if (setjmp(jpeg_env_->jmp_buf_env_)) {
    // This code is run only when libjpeg hit an error and called
    // longjmp(env). It will reset the object to a state where it can be used
    // again.
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_JPEGREADER,
                            "libjpeg failed to decode the image.");
  }

  // At the time when ReadNextScanline is called, allocate buffer for holding
  // a row of pixels, and initiate decompression.
  jpeg_decompress_struct* jpeg_decompress = &(jpeg_env_->jpeg_decompress_);
  if (row_ == 0) {
    row_pointer_[0] = static_cast<JSAMPLE*>(malloc(bytes_per_row_));
    jpeg_start_decompress(jpeg_decompress);
  }

  // Try to read a scanline.
  const JDIMENSION num_scanlines_read =
      jpeg_read_scanlines(jpeg_decompress, row_pointer_, 1);
  if (num_scanlines_read != 1) {
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_PARSE_ERROR,
                            SCANLINE_JPEGREADER,
                            "libjpeg failed to read a scanline.");
  }
  *out_scanline_bytes = row_pointer_[0];
  ++row_;

  // At the last row, ask libjpeg to finish decompression.
  if (!HasMoreScanLines()) {
    jpeg_finish_decompress(jpeg_decompress);
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
