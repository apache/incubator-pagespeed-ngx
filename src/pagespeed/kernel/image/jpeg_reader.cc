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

#include <setjmp.h>  // for longjmp
#include <stdio.h>  // provides FILE for jpeglib (needed for certain builds)
#include <stdlib.h>  // for malloc

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jerror.h"                                                 // NOLINT
#include "jpeglib.h"                                                // NOLINT
#else
#include "third_party/libjpeg/jerror.h"
#include "third_party/libjpeg/jpeglib.h"
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

JpegReader::JpegReader() {
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

}  // namespace image_compression

}  // namespace pagespeed
