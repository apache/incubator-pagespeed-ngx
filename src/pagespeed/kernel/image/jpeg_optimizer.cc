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

// Author: Bryan McQuade, Matthew Steele

#include "pagespeed/kernel/image/jpeg_optimizer.h"

#include <setjmp.h>
// 'stdio.h' provides FILE for jpeglib (needed for certain builds)
#include <stdio.h>
#include <algorithm>
#include <cstdlib>

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_reader.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"   // NOLINT
#else
#include "third_party/libjpeg_turbo/src/jpeglib.h"
#endif
}

using net_instaweb::MessageHandler;
using pagespeed::image_compression::ColorSampling;
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::JpegLossyOptions;
using pagespeed::image_compression::RETAIN;
using pagespeed::image_compression::YUV420;
using pagespeed::image_compression::YUV422;
using pagespeed::image_compression::YUV444;

namespace {
// Unfortunately, libjpeg normally only supports writing images to C FILE
// pointers, wheras we want to write to a C++ string.  Fortunately, libjpeg
// also provides an extension mechanism.  Below, we define a new kind of
// jpeg_destination_mgr for writing to strings.

// The below code was adapted from the JPEGMemoryReader class that can be found
// in src/o3d/core/cross/bitmap_jpg.cc in the Chromium source tree (r29423).
// That code is Copyright 2009, Google Inc.

#define DESTINATION_MANAGER_BUFFER_SIZE 4096
struct DestinationManager : public jpeg_destination_mgr {
  JOCTET buffer[DESTINATION_MANAGER_BUFFER_SIZE];
  GoogleString *str;
};

METHODDEF(void) InitDestination(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.next_output_byte = dest.buffer;
  dest.free_in_buffer = DESTINATION_MANAGER_BUFFER_SIZE;
};

METHODDEF(boolean) EmptyOutputBuffer(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.str->append(reinterpret_cast<char*>(dest.buffer),
                   DESTINATION_MANAGER_BUFFER_SIZE);

  dest.free_in_buffer = DESTINATION_MANAGER_BUFFER_SIZE;
  dest.next_output_byte = dest.buffer;

  return TRUE;
};

METHODDEF(void) TermDestination(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  const size_t datacount =
      DESTINATION_MANAGER_BUFFER_SIZE - dest.free_in_buffer;
  if (datacount > 0) {
    dest.str->append(reinterpret_cast<char*>(dest.buffer), datacount);
  }
};

// Call this function on a j_compress_ptr to install a writer that will write
// to the given string.
void JpegStringWriter(j_compress_ptr cinfo, GoogleString *data_dest) {
  if (cinfo->dest == NULL) {
    cinfo->dest = (struct jpeg_destination_mgr*)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                  sizeof(DestinationManager));
  }
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.str = data_dest;

  dest.init_destination = InitDestination;
  dest.empty_output_buffer = EmptyOutputBuffer;
  dest.term_destination = TermDestination;
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

// Marker for APPN segment is obtained by adding N to JPEG_APP0.
const int kColorProfileMarker = JPEG_APP0 + 2;
const int kExifDataMarker = JPEG_APP0 + 1;
// Signifies max bytes that needs to read, while reading jpeg segments like exif
// data, color profiles and etc.
const int kMaxSegmentSize = 0xFFFF;

// Initializes the jpeg compress struct.
void InitJpegCompress(j_compress_ptr cinfo, jpeg_error_mgr* compress_error) {
  memset(cinfo, 0, sizeof(jpeg_compress_struct));
  memset(compress_error, 0, sizeof(jpeg_error_mgr));

  cinfo->err = jpeg_std_error(compress_error);
  compress_error->error_exit = &ErrorExit;
  compress_error->output_message = &OutputMessage;
  jpeg_create_compress(cinfo);
}

void SetJpegCompressBeforeStartCompress(const JpegCompressionOptions& options,
    const jpeg_decompress_struct* jpeg_decompress,
    jpeg_compress_struct* jpeg_compress) {
  if (options.lossy) {
    const JpegLossyOptions& lossy_options = options.lossy_options;
    // Set the compression parameters if set and lossy compression is enabled,
    // else use the defaults. Last parameter to jpeg_set_quality resticts the
    // jpeg quantizer values in 8 bit values, even though jpeg support 12 bit
    // quantizer values, it is not supported widely.
    jpeg_set_quality(jpeg_compress, lossy_options.quality, 1);

    // Set the color subsampling if applicable.
    if (jpeg_compress->jpeg_color_space == JCS_YCbCr) {
      // Set the color sampling.
      if (lossy_options.color_sampling == YUV444) {
        jpeg_compress->comp_info[0].h_samp_factor = 1;
        jpeg_compress->comp_info[0].v_samp_factor = 1;
      } else if (lossy_options.color_sampling == YUV422) {
        jpeg_compress->comp_info[0].h_samp_factor = 2;
        jpeg_compress->comp_info[0].v_samp_factor = 1;
      } else if (lossy_options.color_sampling == YUV420) {
        jpeg_compress->comp_info[0].h_samp_factor = 2;
        jpeg_compress->comp_info[0].v_samp_factor = 2;
      } else if (lossy_options.color_sampling == RETAIN &&
                 jpeg_decompress != NULL) {
        // Retain the input.
        for (int idx = 0; idx < jpeg_compress->num_components; ++idx) {
          jpeg_compress->comp_info[idx].h_samp_factor =
              jpeg_decompress->comp_info[idx].h_samp_factor;
          jpeg_compress->comp_info[idx].v_samp_factor =
              jpeg_decompress->comp_info[idx].v_samp_factor;
        }
      }
    }
  }

  if (options.progressive) {
    jpeg_simple_progression(jpeg_compress);

    if (options.lossy && options.lossy_options.num_scans > 0) {
      // We can honour the num scans only if the number of scans we want is less
      // than or equals to total number of scans defined for this image, else
      // compress will fail.
      jpeg_compress->num_scans = std::min(jpeg_compress->num_scans,
                                          options.lossy_options.num_scans);
    }
  }
}

void SetJpegCompressAfterStartCompress(const JpegCompressionOptions& options,
    const jpeg_decompress_struct& jpeg_decompress,
    jpeg_compress_struct* jpeg_compress) {
  if (options.retain_color_profile || options.retain_exif_data) {
    jpeg_saved_marker_ptr marker;
    for (marker = jpeg_decompress.marker_list; marker != NULL;
         marker = marker->next) {
      // We only copy these headers if present in the decompress struct.
      if ((marker->marker == kExifDataMarker && options.retain_exif_data) ||
          (marker->marker == kColorProfileMarker &&
           options.retain_color_profile)) {
        jpeg_write_marker(jpeg_compress, marker->marker, marker->data,
                          marker->data_length);
      }
    }
  }
}

class JpegOptimizer {
 public:
  explicit JpegOptimizer(MessageHandler* handler);
  ~JpegOptimizer();

  // Take the given input file and compress it, either losslessly or lossily,
  // depending on the passed in options.  Note that the options parameter can be
  // null, in which case the default options are used.
  // If this function fails (returns false), it can be called again.
  // @return true on success, false on failure.
  bool CreateOptimizedJpeg(const GoogleString &original,
                           GoogleString *compressed,
                           const JpegCompressionOptions& options);

 private:
  bool DoCreateOptimizedJpeg(const GoogleString &original,
                             jpeg_decompress_struct *jpeg_decompress,
                             GoogleString *compressed,
                             const JpegCompressionOptions& options);

  bool OptimizeLossless(jpeg_decompress_struct *jpeg_decompress,
                        GoogleString *compressed,
                        const JpegCompressionOptions& options);

  bool OptimizeLossy(jpeg_decompress_struct *jpeg_decompress,
                     GoogleString *compressed,
                     const JpegCompressionOptions& options);

  // Structures for jpeg compression.
  jpeg_compress_struct jpeg_compress_;
  jpeg_error_mgr compress_error_;
  MessageHandler* message_handler_;
  pagespeed::image_compression::JpegReader reader_;

  DISALLOW_COPY_AND_ASSIGN(JpegOptimizer);
};

JpegOptimizer::JpegOptimizer(MessageHandler* handler)
  : message_handler_(handler),
    reader_(handler) {
  InitJpegCompress(&jpeg_compress_, &compress_error_);
}

JpegOptimizer::~JpegOptimizer() {
  jpeg_destroy_compress(&jpeg_compress_);
}

bool JpegOptimizer::OptimizeLossy(
    jpeg_decompress_struct *jpeg_decompress,
    GoogleString *compressed,
    const JpegCompressionOptions& options) {
  if (!options.lossy) {
    PS_LOG_DFATAL(message_handler_, \
        "lossy is not set in options for lossy jpeg compression");
    return false;
  }

  // Copy data from the source to the dest.
  jpeg_compress_.image_width = jpeg_decompress->image_width;
  jpeg_compress_.image_height = jpeg_decompress->image_height;
  jpeg_compress_.input_components = jpeg_decompress->num_components;

  // Persist the input file's colorspace.
  jpeg_decompress->out_color_space = jpeg_decompress->jpeg_color_space;
  jpeg_compress_.in_color_space = jpeg_decompress->jpeg_color_space;

  // Set the default options.
  jpeg_set_defaults(&jpeg_compress_);

  // Set optimize huffman to true.
  jpeg_compress_.optimize_coding = TRUE;

  SetJpegCompressBeforeStartCompress(options, jpeg_decompress, &jpeg_compress_);

  // Prepare to write to a string.
  JpegStringWriter(&jpeg_compress_, compressed);

  jpeg_start_compress(&jpeg_compress_, TRUE);
  jpeg_start_decompress(jpeg_decompress);

  // Write any markers if needed.
  SetJpegCompressAfterStartCompress(options, *jpeg_decompress, &jpeg_compress_);

  // Make sure input/output parameters are configured correctly.
  DCHECK(jpeg_compress_.image_width == jpeg_decompress->output_width);
  DCHECK(jpeg_compress_.image_height == jpeg_decompress->output_height);
  DCHECK(jpeg_compress_.input_components == jpeg_decompress->output_components);
  DCHECK(jpeg_compress_.in_color_space == jpeg_decompress->out_color_space);

  bool valid_jpeg = true;

  JSAMPROW row_pointer[1];
  row_pointer[0] = static_cast<JSAMPLE*>(malloc(
      jpeg_decompress->output_width * jpeg_decompress->output_components));
  while (jpeg_compress_.next_scanline < jpeg_compress_.image_height) {
    const JDIMENSION num_scanlines_read =
        jpeg_read_scanlines(jpeg_decompress, row_pointer, 1);
    if (num_scanlines_read != 1) {
      valid_jpeg = false;
      break;
    }

    if (jpeg_write_scanlines(&jpeg_compress_, row_pointer, 1) != 1) {
      // We failed to write all the row. Abort.
      valid_jpeg = false;
      break;
    }
  }

  free(row_pointer[0]);
  return valid_jpeg;
}

bool JpegOptimizer::OptimizeLossless(jpeg_decompress_struct *jpeg_decompress,
    GoogleString *compressed, const JpegCompressionOptions& options) {
  if (options.lossy) {
    PS_LOG_DFATAL(message_handler_, \
        "Lossy options are not allowed in lossless compression.");
    return false;
  }

  jvirt_barray_ptr *coefficients = jpeg_read_coefficients(jpeg_decompress);
  bool valid_jpeg = (coefficients != NULL);

  if (valid_jpeg) {
    // Copy data from the source to the dest.
    jpeg_copy_critical_parameters(jpeg_decompress, &jpeg_compress_);

    SetJpegCompressBeforeStartCompress(options, jpeg_decompress,
                                       &jpeg_compress_);

    // Set optimize huffman to true.
    jpeg_compress_.optimize_coding = TRUE;

    // Prepare to write to a string.
    JpegStringWriter(&jpeg_compress_, compressed);

    // Copy the coefficients into the compression struct.
    jpeg_write_coefficients(&jpeg_compress_, coefficients);

    // Write any markers if needed.
    SetJpegCompressAfterStartCompress(options, *jpeg_decompress,
                                      &jpeg_compress_);
  }

  return valid_jpeg;
}

// Helper for JpegOptimizer::CreateOptimizedJpeg().  This function does the
// work, and CreateOptimizedJpeg() does some cleanup.
bool JpegOptimizer::DoCreateOptimizedJpeg(
    const GoogleString &original,
    jpeg_decompress_struct *jpeg_decompress,
    GoogleString *compressed,
    const pagespeed::image_compression::JpegCompressionOptions& options) {
  // libjpeg's error handling mechanism requires that longjmp be used
  // to get control after an error.
  jmp_buf env;
  if (setjmp(env)) {
    // This code is run only when libjpeg hit an error, and called
    // longjmp(env).  Returning false will cause jpeg_abort_(de)compress to be
    // called on jpeg_(de)compress_, putting those structures back into a state
    // where they can be used again.
    return false;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(&env);
  jpeg_compress_.client_data = static_cast<void *>(&env);

  reader_.PrepareForRead(original.data(), original.size());

  if (options.retain_color_profile) {
    jpeg_save_markers(jpeg_decompress, kColorProfileMarker, kMaxSegmentSize);
  }

  if (options.retain_exif_data) {
    jpeg_save_markers(jpeg_decompress, kExifDataMarker, kMaxSegmentSize);
  }

  // Read jpeg data into the decompression struct.
  jpeg_read_header(jpeg_decompress, TRUE);

  bool valid_jpeg = false;
  if (options.lossy) {
    valid_jpeg = OptimizeLossy(jpeg_decompress, compressed, options);
  } else {
    valid_jpeg = OptimizeLossless(jpeg_decompress, compressed, options);
  }

  // Finish the compression process.
  jpeg_finish_compress(&jpeg_compress_);
  jpeg_finish_decompress(jpeg_decompress);

  return valid_jpeg;
}

bool JpegOptimizer::CreateOptimizedJpeg(const GoogleString &original,
    GoogleString *compressed, const JpegCompressionOptions& options) {
  jpeg_decompress_struct* jpeg_decompress = reader_.decompress_struct();

  bool result = DoCreateOptimizedJpeg(original, jpeg_decompress, compressed,
                                      options);

  jpeg_decompress->client_data = NULL;
  jpeg_compress_.client_data = NULL;

  if (!result) {
    // Clean up the state of jpeglib structures.  It is okay to abort even if
    // no (de)compression is in progress.  This is crucial because we enter
    // this block even if no jpeg-related error happened.
    jpeg_abort_decompress(jpeg_decompress);
    jpeg_abort_compress(&jpeg_compress_);
  }

  return result;
}

}  // namespace

namespace pagespeed {

namespace image_compression {

struct JpegScanlineWriter::Data {
  Data() {
    InitJpegCompress(&jpeg_compress_, &compress_error_);
  }

  ~Data() {
    jpeg_destroy_compress(&jpeg_compress_);
  }

  // Structures for jpeg compression.
  jpeg_compress_struct jpeg_compress_;
  jpeg_error_mgr compress_error_;
};

JpegScanlineWriter::JpegScanlineWriter(MessageHandler* handler)
  : data_(new Data()),
    message_handler_(handler) {
}

JpegScanlineWriter::~JpegScanlineWriter() {
  delete data_;
}

void JpegScanlineWriter::SetJmpBufEnv(jmp_buf* env) {
  data_->jpeg_compress_.client_data = static_cast<void *>(env);
}

ScanlineStatus JpegScanlineWriter::InitWithStatus(const size_t width,
                                                  const size_t height,
                                                  PixelFormat pixel_format) {
  data_->jpeg_compress_.image_width = width;
  data_->jpeg_compress_.image_height = height;

  switch (pixel_format) {
    case RGB_888:
      data_->jpeg_compress_.input_components = 3;
      data_->jpeg_compress_.in_color_space = JCS_RGB;
      break;
    case GRAY_8:
      data_->jpeg_compress_.input_components = 1;
      data_->jpeg_compress_.in_color_space = JCS_GRAYSCALE;
      break;
    case RGBA_8888:
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                              SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                              SCANLINE_JPEGWRITER, "transparency");
      break;
    default:
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                              SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                              SCANLINE_JPEGWRITER,
                              "unknown pixel format: %s",
                              GetPixelFormatString(pixel_format));
  }

  // Set the default options.
  jpeg_set_defaults(&data_->jpeg_compress_);

  // Set optimize huffman to true.
  data_->jpeg_compress_.optimize_coding = TRUE;

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

void JpegScanlineWriter::SetJpegCompressParams(
    const JpegCompressionOptions& options) {
  if (!options.lossy) {
    PS_LOG_DFATAL(message_handler_, \
        "Unable to perform lossless encoding in JpegScanlineWriter." \
        " Using jpeg default lossy encoding options.");
  }
  SetJpegCompressBeforeStartCompress(options, NULL, &data_->jpeg_compress_);
}

ScanlineStatus JpegScanlineWriter::InitializeWriteWithStatus(
    const void* const params,
    GoogleString * const compressed) {
  if (params == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_JPEGWRITER,
                            "missing JpegCompressionOptions*");
  }
  const JpegCompressionOptions* jpeg_compression_options =
      static_cast<const JpegCompressionOptions*>(params);
  SetJpegCompressParams(*jpeg_compression_options);
  JpegStringWriter(&data_->jpeg_compress_, compressed);
  jpeg_start_compress(&data_->jpeg_compress_, TRUE);
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus JpegScanlineWriter::WriteNextScanlineWithStatus(
    const void* const scanline_bytes) {
  JSAMPROW row_pointer[1] = {
    static_cast<JSAMPLE*>(
        const_cast<void*>(scanline_bytes))
  };
  unsigned int result = jpeg_write_scanlines(&data_->jpeg_compress_,
                                             row_pointer, 1);
  if (result == 1) {
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  } else {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_JPEGWRITER,
                            "jpeg_write_scanlines()");
  }
}

ScanlineStatus JpegScanlineWriter::FinalizeWriteWithStatus() {
  jpeg_finish_compress(&data_->jpeg_compress_);
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

void JpegScanlineWriter::AbortWrite() {
  data_->jpeg_compress_.client_data = NULL;
  jpeg_abort_compress(&data_->jpeg_compress_);
}

bool OptimizeJpeg(const GoogleString &original,
                  GoogleString *compressed,
                  MessageHandler* handler) {
  JpegOptimizer optimizer(handler);
  JpegCompressionOptions options;
  return optimizer.CreateOptimizedJpeg(original, compressed, options);
}

bool OptimizeJpegWithOptions(const GoogleString &original,
                             GoogleString *compressed,
                             const JpegCompressionOptions &options,
                             MessageHandler* handler) {
  JpegOptimizer optimizer(handler);
  return optimizer.CreateOptimizedJpeg(original, compressed, options);
}

}  // namespace image_compression

}  // namespace pagespeed
