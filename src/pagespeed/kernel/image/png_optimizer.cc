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

#include "pagespeed/kernel/image/png_optimizer.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_utils.h"

#ifdef __native_client__
// For some reason that is not yet clear, invoking png_longjmp on
// native client causes a crash. Invoking longjmp on the jump buffer
// directly does not crash. The jump buffer is defined in the
// following libpng private header, so we include it here. See
// http://code.google.com/p/page-speed/issues/detail?id=644 for more
// information.
#include "third_party/libpng/pngstruct.h"
#endif

extern "C" {
#ifdef USE_SYSTEM_ZLIB
#include "zlib.h"  // NOLINT
#else
#include "third_party/zlib/zlib.h"
#endif

#include "third_party/optipng/src/opngreduc/opngreduc.h"
}

using pagespeed::image_compression::PngCompressParams;

namespace {

// we use these four combinations because different images seem to benefit from
// different parameters and this combination of 4 seems to work best for a large
// set of PNGs from the web.
const PngCompressParams kPngCompressionParams[] = {
  PngCompressParams(PNG_ALL_FILTERS, Z_DEFAULT_STRATEGY),
  PngCompressParams(PNG_ALL_FILTERS, Z_FILTERED),
  PngCompressParams(PNG_FILTER_NONE, Z_DEFAULT_STRATEGY),
  PngCompressParams(PNG_FILTER_NONE, Z_FILTERED)
};

const size_t kParamCount = arraysize(kPngCompressionParams);

void ReadPngFromStream(png_structp read_ptr,
                       png_bytep data,
                       png_size_t length) {
  pagespeed::image_compression::ScanlineStreamInput* input =
    reinterpret_cast<pagespeed::image_compression::ScanlineStreamInput*>(
      png_get_io_ptr(read_ptr));

  if (input->offset() + length <= input->length()) {
    memcpy(data, input->data() + input->offset(), length);
    input->set_offset(input->offset() + length);

  } else {
    DLOG(INFO) << "ReadPngFromStream: Unexpected EOF.";

    // We weren't able to satisfy the read, so abort.
#if PNG_LIBPNG_VER >= 10400
  #ifndef __native_client__
    png_longjmp(read_ptr, 1);
  #else
    // On native client, invoking png_longjmp as above causes a
    // crash. Invoking longjmp directly, however, works fine.  For the
    // time being we use this workaround for native client builds. See
    // http://code.google.com/p/page-speed/issues/detail?id=644 for
    // more information.
    longjmp(read_ptr->longjmp_buffer, 1);
  #endif
#else
    longjmp(read_ptr->jmpbuf, 1);
#endif
  }
}

void WritePngToString(png_structp write_ptr,
                      png_bytep data,
                      png_size_t length) {
  GoogleString& buffer =
      *reinterpret_cast<GoogleString*>(png_get_io_ptr(write_ptr));
  buffer.append(reinterpret_cast<char*>(data), length);
}

void PngErrorFn(png_structp png_ptr, png_const_charp msg) {
  DLOG(INFO) << "libpng error: " << msg;

  // Invoking the error function indicates a terminal failure, which
  // means we must longjmp to abort the libpng invocation.
#if PNG_LIBPNG_VER >= 10400
  #ifndef __native_client__
    png_longjmp(png_ptr, 1);
  #else
    // On native client, invoking png_longjmp as above causes a
    // crash. Invoking longjmp directly, however, works fine.  For the
    // time being we use this workaround for native client builds. See
    // http://code.google.com/p/page-speed/issues/detail?id=644 for
    // more information.
    longjmp(png_ptr->longjmp_buffer, 1);
  #endif

#else
  longjmp(png_ptr->jmpbuf, 1);
#endif
}

void PngWarningFn(png_structp png_ptr, png_const_charp msg) {
  DLOG(INFO) << "libpng warning: " << msg;
}

// no-op
void PngFlush(png_structp write_ptr) {}

// Helper that reads an unsigned 32-bit integer from a stream of
// big-endian bytes.
inline uint32 ReadUint32FromBigEndianBytes(const unsigned char* read_head) {
  return (static_cast<uint32>(*read_head) << 24) +
      (static_cast<uint32>(*(read_head + 1)) << 16) +
      (static_cast<uint32>(*(read_head + 2)) << 8) +
      static_cast<uint32>(*(read_head + 3));
}

}  // namespace

namespace pagespeed {

namespace image_compression {

PngCompressParams::PngCompressParams(int level, int strategy)
    : filter_level(level), compression_strategy(strategy) {
}

ScopedPngStruct::ScopedPngStruct(Type type)
    : png_ptr_(NULL), info_ptr_(NULL), type_(type) {
  switch (type) {
    case READ:
      png_ptr_ = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL);
      break;
    case WRITE:
      png_ptr_ = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                        NULL, NULL, NULL);
      break;
    default:
      LOG(DFATAL) << "Invalid Type " << type_;
      break;
  }
  if (png_ptr_ != NULL) {
    info_ptr_ = png_create_info_struct(png_ptr_);
  }

  png_set_error_fn(png_ptr_, NULL, &PngErrorFn, &PngWarningFn);
}

bool ScopedPngStruct::reset() {
  switch (type_) {
    case READ:
      png_destroy_read_struct(&png_ptr_, &info_ptr_, NULL);
      png_ptr_ = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                        NULL, NULL, NULL);
      break;
    case WRITE:
      png_destroy_write_struct(&png_ptr_, &info_ptr_);
      png_ptr_ = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                         NULL, NULL, NULL);
      break;
    default:
      LOG(DFATAL) << "Invalid Type " << type_;
      break;
  }

  if (setjmp(png_jmpbuf(png_ptr_))) {
    LOG(DFATAL) << "png_jumpbuf not set locally: risk of memory leaks";
    return false;
  }

  if (png_ptr_ != NULL) {
    info_ptr_ = png_create_info_struct(png_ptr_);
  }

  png_set_error_fn(png_ptr_, NULL, &PngErrorFn, &PngWarningFn);

  return true;
}

ScopedPngStruct::~ScopedPngStruct() {
  switch (type_) {
    case READ:
      png_destroy_read_struct(&png_ptr_, &info_ptr_, NULL);
      break;
    case WRITE:
      png_destroy_write_struct(&png_ptr_, &info_ptr_);
      break;
    default:
      break;
  }
}

PngReaderInterface::PngReaderInterface() {
}

PngReaderInterface::~PngReaderInterface() {
}

PngOptimizer::PngOptimizer()
    : read_(ScopedPngStruct::READ),
      write_(ScopedPngStruct::WRITE),
      best_compression_(false) {
}

PngOptimizer::~PngOptimizer() {
}

bool PngOptimizer::CreateOptimizedPng(const PngReaderInterface& reader,
                                      const GoogleString& in,
                                      GoogleString* out) {
  if (!read_.valid() || !write_.valid()) {
    LOG(DFATAL) << "Invalid ScopedPngStruct r: "
                << read_.valid() << ", w: " << write_.valid();
    return false;
  }

  out->clear();

  // Configure error handlers.
  if (setjmp(png_jmpbuf(read_.png_ptr()))) {
    LOG(DFATAL) << "png_jmpbuf not set locally: risk of memory leaks";
    return false;
  }

  if (setjmp(png_jmpbuf(write_.png_ptr()))) {
    LOG(DFATAL) << "png_jmpbuf not set locally: risk of memory leaks";
    return false;
  }

  if (!reader.ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                      PNG_TRANSFORM_IDENTITY)) {
    return false;
  }

  if (!opng_validate_image(read_.png_ptr(), read_.info_ptr())) {
    return false;
  }

  // Copy the image data from the read structures to the write structures.
  if (!CopyReadToWrite()) {
    return false;
  }

  // Perform all possible lossless image reductions
  // (e.g. RGB->palette, etc).
  opng_reduce_image(write_.png_ptr(), write_.info_ptr(), OPNG_REDUCE_ALL);

  if (best_compression_) {
    return CreateBestOptimizedPngForParams(kPngCompressionParams, kParamCount,
                                           out);
  } else {
    PngCompressParams params(PNG_FILTER_NONE, Z_DEFAULT_STRATEGY);
    return CreateOptimizedPngWithParams(&write_, params, out);
  }
}

bool PngOptimizer::CreateBestOptimizedPngForParams(
    const PngCompressParams* param_list, size_t param_list_size,
    GoogleString* out) {
  bool success = false;
  for (size_t idx = 0; idx < param_list_size; ++idx) {
    ScopedPngStruct write(ScopedPngStruct::WRITE);
    GoogleString temp_output;
    // libpng doesn't allow for reuse of the write structs, so we must copy on
    // each iteration of the loop.
    CopyPngStructs(&write_, &write);
    if (CreateOptimizedPngWithParams(&write, param_list[idx], &temp_output)) {
      // If this gives better compression update the output.
      if (out->empty() || out->size() > temp_output.size()) {
        out->swap(temp_output);
      }
      success |= true;
    }
  }
  return success;
}

bool PngOptimizer::CreateOptimizedPngWithParams(ScopedPngStruct* write,
    const PngCompressParams& params,
    GoogleString *out) {
  int compression_level =
      best_compression_ ? Z_BEST_COMPRESSION : Z_DEFAULT_COMPRESSION;
  png_set_compression_level(write->png_ptr(), compression_level);
  png_set_compression_mem_level(write->png_ptr(), 8);
  png_set_compression_strategy(write->png_ptr(), params.compression_strategy);
  png_set_filter(write->png_ptr(), PNG_FILTER_TYPE_BASE, params.filter_level);
  png_set_compression_window_bits(write->png_ptr(), 15);
  if (!WritePng(write, out)) {
    return false;
  }
  return true;
}

bool PngOptimizer::OptimizePng(const PngReaderInterface& reader,
                               const GoogleString& in,
                               GoogleString* out) {
  PngOptimizer o;
  return o.CreateOptimizedPng(reader, in, out);
}

bool PngOptimizer::OptimizePngBestCompression(const PngReaderInterface& reader,
                                              const GoogleString& in,
                                              GoogleString* out) {
  PngOptimizer o;
  o.EnableBestCompression();
  return o.CreateOptimizedPng(reader, in, out);
}

PngReader::PngReader() {
}

PngReader::~PngReader() {
}

bool PngReader::ReadPng(const GoogleString& body,
                        png_structp png_ptr,
                        png_infop info_ptr,
                        int transforms,
                        bool require_opaque) const {
    ScanlineStreamInput input;
    input.Initialize(body);

    if (setjmp(png_jmpbuf(png_ptr))) {
      return false;
    }
    png_set_read_fn(png_ptr, &input, &ReadPngFromStream);
    png_read_png(png_ptr, info_ptr, transforms, NULL);

    if (require_opaque &&
        ((transforms & PNG_TRANSFORM_STRIP_ALPHA) == 0)) {
      // We're not guaranteed that the image is opaque already.

      int color_type = png_get_color_type(png_ptr, info_ptr);
      if ((color_type & PNG_COLOR_MASK_ALPHA) != 0) {
        // Image has an alpha channel. Make sure it's opaque, and
        // strip it.

        if (!IsAlphaChannelOpaque(png_ptr, info_ptr)) {
          return false;
        }
        if ((OPNG_REDUCE_STRIP_ALPHA &
             opng_reduce_image(png_ptr, info_ptr, OPNG_REDUCE_STRIP_ALPHA))
            == 0) {
          return false;
        }
      }
    }
    return true;
}

bool PngReader::GetAttributes(const GoogleString& body,
                              int* out_width,
                              int* out_height,
                              int* out_bit_depth,
                              int* out_color_type) const {
  // We need to read the PNG signature plus the IDAT chunk.
  //
  // Signature is 8 bytes, documentation:
  //  http://www.libpng.org/pub/png/spec/1.2/png-1.2-pdg.html#PNG-file-signature
  //
  // Chunk layout is 4 bytes chunk len + 4 bytes chunk name + chunk +
  // 4 bytes chunk CRC, documentation:
  //  http://www.libpng.org/pub/png/spec/1.2/png-1.2-pdg.html#Chunk-layout
  //
  // IDAT chunk is 13 bytes (see code for details), documentation:
  //  http://www.libpng.org/pub/png/spec/1.2/png-1.2-pdg.html#C.IHDR

  const size_t kPngSigBytesSize = 8;
  const size_t kChunkLenSize = 4;
  const size_t kChunkNameSize = 4;
  const size_t kIHDRChunkSize = 13;
  const size_t kChunkCRCSize = 4;

  const size_t kPngMinHeaderSize =
      kPngSigBytesSize +
      kChunkLenSize +
      kChunkNameSize +
      kIHDRChunkSize +
      kChunkCRCSize;

  if (body.size() < kPngMinHeaderSize) {
    // Not enough bytes for us to read, so abort early.
    return false;
  }

  const unsigned char* read_head =
      reinterpret_cast<const unsigned char*>(body.data());

  // Validate the PNG signature.
  if (png_sig_cmp(
          const_cast<unsigned char*>(read_head), 0, kPngSigBytesSize) != 0) {
    return false;
  }
  read_head += kPngSigBytesSize;

  // The first 4 bytes of the chunk contains the chunk length.
  const uint32 first_chunk_len = ReadUint32FromBigEndianBytes(read_head);
  if (first_chunk_len != kIHDRChunkSize) {
    return false;
  }
  read_head += kChunkLenSize;

  if (strncmp("IHDR", reinterpret_cast<const char*>(read_head), 4) != 0) {
    return false;
  }

  // Compute the CRC for the chunk (using zlib's CRC computer since
  // it's already available to us).
  uint32 computed_crc = crc32(0L, Z_NULL, 0);
  computed_crc =
      crc32(computed_crc, read_head, kChunkNameSize + kIHDRChunkSize);
  read_head += kChunkNameSize;

  // Extract the expected CRC, after the end of the IHDR data.
  uint32 expected_crc =
      ReadUint32FromBigEndianBytes(read_head + kIHDRChunkSize);
  if (expected_crc != computed_crc) {
    // CRC mismatch. Invalid chunk. Abort.
    return false;
  }

  // Now read the IHDR chunk contents. Its layout is:
  // width: 4 bytes
  // height: 4 bytes
  // bit_depth: 1 byte
  // color_type: 1 byte
  // other data: 3 bytes
  *out_width = ReadUint32FromBigEndianBytes(read_head);
  *out_height = ReadUint32FromBigEndianBytes(read_head + 4);
  *out_bit_depth = read_head[8];
  *out_color_type = read_head[9];
  return true;
}

bool PngOptimizer::WritePng(ScopedPngStruct* write, GoogleString* buffer) {
  if (setjmp(png_jmpbuf(write->png_ptr()))) {
    return false;
  }
  png_set_write_fn(write->png_ptr(), buffer, &WritePngToString, &PngFlush);
  png_write_png(
      write->png_ptr(), write->info_ptr(), PNG_TRANSFORM_IDENTITY, NULL);

  return true;
}

bool PngOptimizer::CopyReadToWrite() {
  return CopyPngStructs(&read_, &write_);
}

bool PngOptimizer::CopyPngStructs(ScopedPngStruct* from, ScopedPngStruct* to) {
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type, filter_type;
  if (setjmp(png_jmpbuf(from->png_ptr()))) {
    return false;
  }
  png_get_IHDR(from->png_ptr(),
               from->info_ptr(),
               &width,
               &height,
               &bit_depth,
               &color_type,
               &interlace_type,
               &compression_type,
               &filter_type);

  if (setjmp(png_jmpbuf(to->png_ptr()))) {
    return false;
  }
  png_set_IHDR(to->png_ptr(),
               to->info_ptr(),
               width,
               height,
               bit_depth,
               color_type,
               interlace_type,
               compression_type,
               filter_type);

  // NOTE: if libpng's free_me capability is not enabled, sharing
  // rowbytes between the read and write structs will lead to a
  // double-free. Thus we test for the PNG_FREE_ME_SUPPORTED define
  // here.
#ifndef PNG_FREE_ME_SUPPORTED
#error PNG_FREE_ME_SUPPORTED is required or double-frees may happen.
#endif
  png_bytepp row_pointers = png_get_rows(from->png_ptr(), from->info_ptr());
  png_set_rows(to->png_ptr(), to->info_ptr(), row_pointers);

  png_colorp palette;
  int num_palette;
  if (png_get_PLTE(
          from->png_ptr(), from->info_ptr(), &palette, &num_palette) != 0) {
    png_set_PLTE(to->png_ptr(),
                 to->info_ptr(),
                 palette,
                 num_palette);
  }

  // Transparency is not considered metadata, although tRNS is
  // ancillary.
  png_bytep trans;
  int num_trans;
  png_color_16p trans_values;
  if (png_get_tRNS(from->png_ptr(),
                   from->info_ptr(),
                   &trans,
                   &num_trans,
                   &trans_values) != 0) {
    png_set_tRNS(to->png_ptr(),
                 to->info_ptr(),
                 trans,
                 num_trans,
                 trans_values);
  }

  double gamma;
  if (png_get_gAMA(from->png_ptr(), from->info_ptr(), &gamma) != 0) {
    png_set_gAMA(to->png_ptr(), to->info_ptr(), gamma);
  }

  // Do not copy bkgd, hist or sbit sections, since they are not
  // supported in most browsers.

  return true;
}

// static
bool PngReaderInterface::IsAlphaChannelOpaque(
    png_structp png_ptr, png_infop info_ptr) {
  png_uint_32 height;
  png_uint_32 width;
  int bit_depth;
  int color_type;

  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               NULL, NULL, NULL);

  if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
    // Image doesn't have alpha.
    LOG(DFATAL)
        << "IsAlphaChannelOpaque called for image without alpha channel.";
    return false;
  }

  png_bytep trans;
  int num_trans;
  png_color_16p trans_values;
  if (png_get_tRNS(png_ptr,
                   info_ptr,
                   &trans,
                   &num_trans,
                   &trans_values) != 0) {
    if ((color_type & PNG_COLOR_MASK_PALETTE) != 0) {
      for (int idx = 0; idx < num_trans; ++idx) {
        if (trans[idx] != 0xff) {
          return false;
        }
      }
      return true;
    } else {
      // Non-paletted image with a tRNS block is transparent
      return false;
    }
  } else {
    // There is no tRNS block.
    if ((color_type & PNG_COLOR_MASK_PALETTE) != 0) {
      // If we go this far, we have an image with
      // PNG_COLOR_MASK_ALPHA but no tRNS block. We're confused.
      LOG(DFATAL) << "PNG_COLOR_MASK is set but could not read tRNS.";
      return false;
    }
  }

  int channels = png_get_channels(png_ptr, info_ptr);

  if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
    if (channels != 4) {
      LOG(DFATAL) << "Encountered unexpected number of channels for RGBA"
                  << " image: " << channels;
      return false;
    }
  } else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (channels != 2) {
      LOG(DFATAL) << "Encountered unexpected number of channels for "
                  << "Gray + Alpha image: " << channels;
      return false;
    }
  } else {
    LOG(DFATAL) << "Encountered alpha image of unknown type :" << color_type;
    return false;
  }

  // We currently detect alpha only for 8/16 bit Gray/TrueColor with Alpha
  // channel. Only 8 or 16 bit depths are supported for these modes.
  if (bit_depth % 8 != 0) {
    DLOG(INFO) << "Received unexpected bit_depth: " << bit_depth;
    return false;
  }

  int bytes_per_channel = bit_depth / 8;
  int bytes_per_pixel = channels * bytes_per_channel;
  png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

  // Alpha channel is always the last channel.
  png_uint_32 alpha_byte_offset = (channels - 1) * bytes_per_channel;
  for (png_uint_32 row = 0; row < height; ++row) {
    unsigned char* row_bytes =
        static_cast<unsigned char*>(*(row_pointers + row));
    for (png_uint_32 pixel = 0; pixel < width * bytes_per_pixel;
         pixel += bytes_per_pixel) {
      for (int alpha_byte = 0; alpha_byte < bytes_per_channel;
           ++alpha_byte) {
        if ((row_bytes[pixel + alpha_byte_offset + alpha_byte] & 0xff) !=
            0xff) {
          return false;
        }
      }
    }
  }

  return true;
}

// static
bool PngReaderInterface::GetBackgroundColor(
    png_structp png_ptr, png_infop info_ptr,
    unsigned char *red, unsigned char* green, unsigned char* blue) {
  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  if (!png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD)) {
    return false;
  }
  png_color_16p bg;
  png_get_bKGD(png_ptr, info_ptr, &bg);
  const png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  const png_byte color_type = png_get_color_type(png_ptr, info_ptr);

  if (bit_depth == 16) {
    // Downsample 16bit to 8bit.
    *red = bg->red >> 8;
    *green = bg->green >> 8;
    *blue = bg->blue >> 8;
  } else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    // Upsample to 8bit.
    const int scale = 255 / ((bit_depth << 1) - 1);
    const unsigned char gray_8bit = bg->gray * scale;
    *red = gray_8bit;
    *green = gray_8bit;
    *blue = gray_8bit;
  } else if (bit_depth == 8) {
    *red = static_cast<unsigned char>(bg->red);
    *green = static_cast<unsigned char>(bg->green);
    *blue = static_cast<unsigned char>(bg->blue);
  } else {
    // TODO(bmcquade): we currently fall through to this case for
    // 1-bit paletted images. Consider adding support.
    DLOG(INFO) << "Unsupported bit_depth: "
               << static_cast<int>(bit_depth) << " color type: "
               << static_cast<int>(color_type);
    return false;
  }

  return true;
}

PngScanlineReader::PngScanlineReader()
    : read_(ScopedPngStruct::READ),
      current_scanline_(0),
      transform_(PNG_TRANSFORM_IDENTITY),
      require_opaque_(false) {
}

jmp_buf* PngScanlineReader::GetJmpBuf() {
  jmp_buf& buf = png_jmpbuf(read_.png_ptr());
  return &buf;
}

bool PngScanlineReader::Reset() {
  if (!read_.reset()) {
    return false;
  }
  current_scanline_ = 0;
  transform_ = PNG_TRANSFORM_IDENTITY;
  require_opaque_ = false;
  return true;
}

bool PngScanlineReader::InitializeRead(const PngReaderInterface& reader,
                                       const GoogleString& in) {
  bool is_opaque = false;
  return InitializeRead(reader, in, &is_opaque);
}

bool PngScanlineReader::InitializeRead(const PngReaderInterface& reader,
                                       const GoogleString& in,
                                       bool* is_opaque) {
  if (!read_.valid()) {
    LOG(DFATAL) << "Invalid ScopedPngStruct r: " << read_.valid();
    return false;
  }

  *is_opaque = require_opaque_;
  if (!reader.ReadPng(in, read_.png_ptr(), read_.info_ptr(), transform_,
                      require_opaque_)) {
    return false;
  }

  if (setjmp(png_jmpbuf(read_.png_ptr()))) {
    return false;
  }
  if (!require_opaque_) {
    int color_type = png_get_color_type(read_.png_ptr(), read_.info_ptr());
    *is_opaque = ((color_type & PNG_COLOR_MASK_ALPHA) == 0);
    if (!(*is_opaque) &&
        PngReaderInterface::IsAlphaChannelOpaque(
            read_.png_ptr(), read_.info_ptr())) {
      // Clear the read pointers.
      if (!read_.reset()) {
        return false;
      }
      *is_opaque = true;
      return reader.ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                            transform_ | PNG_TRANSFORM_STRIP_ALPHA);
    }
  }

  return true;
}

PngScanlineReader::~PngScanlineReader() {
}

size_t PngScanlineReader::GetBytesPerScanline() {
  return png_get_rowbytes(read_.png_ptr(), read_.info_ptr());
}

bool PngScanlineReader::HasMoreScanLines() {
  size_t height = png_get_image_height(read_.png_ptr(), read_.info_ptr());
  return current_scanline_ < height;
}

bool PngScanlineReader::ReadNextScanline(void** out_scanline_bytes) {
  if (!HasMoreScanLines()) {
    LOG(DFATAL) << "Read past last scanline.";
    return false;
  }

  if (setjmp(png_jmpbuf(read_.png_ptr()))) {
    return false;
  }
  png_bytepp row_pointers = png_get_rows(read_.png_ptr(), read_.info_ptr());
  *out_scanline_bytes = static_cast<void*>(*(row_pointers + current_scanline_));
  current_scanline_++;
  return true;
}

void PngScanlineReader::set_transform(int transform) {
  transform_ = transform;
}

void PngScanlineReader::set_require_opaque(bool require_opaque) {
  require_opaque_ = require_opaque;
}

size_t PngScanlineReader::GetImageHeight() {
  return png_get_image_height(read_.png_ptr(), read_.info_ptr());
}

size_t PngScanlineReader::GetImageWidth() {
  return png_get_image_width(read_.png_ptr(), read_.info_ptr());
}

int PngScanlineReader::GetColorType() {
  return png_get_color_type(read_.png_ptr(), read_.info_ptr());
}

PixelFormat PngScanlineReader::GetPixelFormat() {
  int bit_depth = png_get_bit_depth(read_.png_ptr(), read_.info_ptr());
  int color_type = png_get_color_type(read_.png_ptr(), read_.info_ptr());
  if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_GRAY) {
    return GRAY_8;
  } else if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB) {
    return RGB_888;
  } else if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
    return RGBA_8888;
  }

  return UNSUPPORTED;
}

bool PngScanlineReader::GetBackgroundColor(
  unsigned char* red, unsigned char* green, unsigned char* blue) {
  return PngReaderInterface::GetBackgroundColor(
      read_.png_ptr(), read_.info_ptr(), red, green, blue);
}

PngScanlineReaderRaw::PngScanlineReaderRaw()
  : pixel_format_(UNSUPPORTED),
    is_progressive_(false),
    height_(0),
    width_(0),
    bytes_per_row_(0),
    row_(0),
    was_initialized_(false),
    png_struct_(ScopedPngStruct::READ) {
}

PngScanlineReaderRaw::~PngScanlineReaderRaw() {
  if (was_initialized_) {
    Reset();
  }
}

bool PngScanlineReaderRaw::Reset() {
  pixel_format_ = UNSUPPORTED;
  is_progressive_ = false;
  height_ = 0;
  width_ = 0;
  bytes_per_row_ = 0;
  row_ = 0;
  was_initialized_ = false;
  if (!png_struct_.reset()) {
    return false;
  }
  png_input_->Reset();
  return true;
}

// Initialize the reader with the given image stream. Note that image_buffer
// must remain unchanged until the last call to ReadNextScanline().
bool PngScanlineReaderRaw::Initialize(const void* image_buffer,
                                      size_t buffer_length) {
  // Allocate and initialize png_input_, if that has not been done.
  if (png_input_ == NULL) {
    png_input_.reset(new ScanlineStreamInput());
    if (png_input_ == NULL) {
      return false;
    }
  }

  // Reset the reader if it has been initialized before.
  if (was_initialized_ && !Reset()) {
    return false;
  }

  if (!png_struct_.valid()) {
    return false;
  }

  png_structp png_ptr = png_struct_.png_ptr();
  png_infop info_ptr = png_struct_.info_ptr();

  if (setjmp(png_jmpbuf(png_ptr)) != 0) {
    // Jump to here if any error happens.
    png_struct_.reset();
    return false;
  }

  // Set up data feed for libpng.
  png_input_->Initialize(image_buffer, buffer_length);
  png_set_read_fn(png_ptr, png_input_.get(), ReadPngFromStream);

  png_uint_32 width, height;
  int32 bit_depth, color_type, interlace_type;
  png_read_info(png_ptr, info_ptr);
  const int ok = png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
                              &color_type, &interlace_type, NULL, NULL);
  if (ok == 0) {
    png_struct_.reset();
    return false;
  }

  // Set up transformations. We will transform the input to one of these
  // formats: GRAY_8, RGB_888, and RGBA_8888.
  //
  // Reference for setting up transformations is in the png_read_png() function
  // in pngread.c.
  //
  // Strip 16 bit per color down to 8 bits per color.
  png_set_strip_16(png_ptr);
  // Expand paletted colors into true RGB triplets.
  // Expand grayscale images to full 8 bits from 1, 2, or 4 bits per pixel.
  // Expand paletted or RGB images with transparency to full alpha channels
  // so the data will be available as RGBA quartets.
  if ((bit_depth < 8) ||
      (color_type == PNG_COLOR_TYPE_PALETTE) ||
      (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))) {
    png_set_expand(png_ptr);
  }
  // Set up callbacks for interlacing (progressive) image.
  png_set_interlace_handling(png_ptr);
  // Expand Gray_Alpha to RGBA.
  if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }

  // Update the reader struct after setting the transformations.
  png_read_update_info(png_ptr, info_ptr);
  // Get the updated color type.
  color_type = png_get_color_type(png_ptr, info_ptr);

  // Determine the pixel format and the number of channels.
  switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
      pixel_format_ = GRAY_8;
      break;
    case PNG_COLOR_TYPE_RGB:
    case PNG_COLOR_TYPE_PALETTE:
      pixel_format_ = RGB_888;
      break;
    case PNG_COLOR_TYPE_RGBA:
      pixel_format_ = RGBA_8888;
      break;
    default:  // Unrecognized format.
      LOG(INFO) << "Unrecognized color type.";
      png_struct_.reset();
      return false;
  }

  // Copy the information to the object properties.
  width_ = width;
  height_ = height;
  bytes_per_row_ = width_ * GetNumChannelsFromPixelFormat(pixel_format_);
  row_ = 0;
  is_progressive_ = (interlace_type == PNG_INTERLACE_ADAM7);
  was_initialized_ = true;
  return true;
}

bool PngScanlineReaderRaw::ReadNextScanline(void** out_scanline_bytes) {
  if (!was_initialized_ || !HasMoreScanLines()) {
    return false;
  }

  png_structp png_ptr = png_struct_.png_ptr();

  // In case libpng has an error, program will jump to the following 'setjmp',
  // which will have value of non-zero. To clean up memory properly, we have
  // to define row_pointers before 'setjmp' and clean it up when error happens.
  scoped_array<png_bytep> row_pointers;
  if (setjmp(png_jmpbuf(png_ptr)) != 0) {
    row_pointers.reset();
    Reset();
    return false;
  }

  // At the first time when ReadNextScanline() is called, we allocate buffer
  // to store the decoded pixels. For non-progressive (non-interlacing)
  // image, we need buffer to store a row of pixels. For progressive image,
  // we need buffer to store the entire image. For progressive image, we
  // also decode the entire image at the first call.
  if (row_ == 0) {
    if (!is_progressive_) {
      image_buffer_.reset(new png_byte[bytes_per_row_]);
    } else {
      image_buffer_.reset(new png_byte[bytes_per_row_ * height_]);
      // For a progressive PNG, we have to decode the entire image before
      // rendering any row. So at the first time when ReadNextScanline()
      // is called, we decode the entire image into image_buffer_.
      if (image_buffer_ != NULL) {
        // Initialize an array of pointers, which specify the address of rows.
        row_pointers.reset(new png_bytep[height_]);
        if (row_pointers == NULL) {
          Reset();
          return false;
        }
        for (size_t i = 0; i < height_; ++i) {
          row_pointers[i] = image_buffer_.get() + i * bytes_per_row_;
        }

        // Decode the entire image. The results are stored in image_buffer_.
        png_read_image(png_ptr, row_pointers.get());
      }
    }
    if (image_buffer_ == NULL) {
      Reset();
      return false;
    }
  }

  if (!is_progressive_) {
    // For a non-progressive PNG, we decode the image a row at a time.
    png_read_row(png_ptr, image_buffer_.get(), NULL);
    *out_scanline_bytes = static_cast<void*>(image_buffer_.get());
  } else {
    // For a progressive PNG, we simply point the output to the corresponding
    // row, because the image has already been decoded.
    *out_scanline_bytes =
      static_cast<void*>(image_buffer_.get() + row_ * bytes_per_row_);
  }

  ++row_;
  return true;
}

PngScanlineWriter::PngScanlineWriter() :
  width_(0),
  height_(0),
  row_(0),
  pixel_format_(UNSUPPORTED),
  png_struct_(ScopedPngStruct::WRITE),
  was_initialized_(false) {
}

PngScanlineWriter::~PngScanlineWriter() {
  if (was_initialized_) {
    Reset();
  }
}

bool PngScanlineWriter::Reset() {
  width_ = 0;
  height_ = 0;
  row_ = 0;
  pixel_format_ = UNSUPPORTED;
  if (!png_struct_.reset()) {
    return false;
  }
  was_initialized_ = false;
  return true;
}

bool PngScanlineWriter::Validate(const PngCompressParams* params,
                                 GoogleString* png_image) {
  if (params != NULL) {
    // PNG_NO_FILTERS == 0
    // PNG_ALL_FILTERS == (PNG_FILTER_NONE | PNG_FILTER_SUB | PNG_FILTER_UP | \
    //                     PNG_FILTER_AVG | PNG_FILTER_PAETH)
    if (params->filter_level & (~PNG_ALL_FILTERS)) {
      LOG(DFATAL) << "Filter level must be one of the following values, "
                  << "or bitwise OR of some of them: "
                  << "PNG_NO_FILTERS, "
                  << "PNG_FILTER_NONE, "
                  << "PNG_FILTER_SUB, "
                  << "PNG_FILTER_UP, "
                  << "PNG_FILTER_AVG, "
                  << "PNG_FILTER_PAETH.";
    }

    switch (params->compression_strategy) {
      case Z_DEFAULT_STRATEGY:
      case Z_FILTERED:
      case Z_HUFFMAN_ONLY:
      case Z_RLE:
      case Z_FIXED:
        break;
      default:
        LOG(DFATAL) << "Compression strategy must be one of the following "
                    << "values: "
                    << "Z_DEFAULT_STRATEGY, "
                    << "Z_FILTERED, "
                    << "Z_HUFFMAN_ONLY, "
                    << "Z_RLE, "
                    << "Z_FIXED.";
        return false;
    }
  }

  if (png_image == NULL) {
    LOG(DFATAL) << "Ouput PNG image cannot be NULL.";
    return false;
  }
  return true;
}

bool PngScanlineWriter::Init(const size_t width, const size_t height,
                             PixelFormat pixel_format) {
  // Reset the reader if it has been initialized before.
  if (was_initialized_ && !Reset()) {
    LOG(DFATAL) << "Failed to re-initialize the writer.";
    return false;
  }

  if (!png_struct_.valid()) {
    LOG(DFATAL) << "Invalid png_struct.";
    return false;
  }

  if (width < 1 || height < 1) {
    LOG(DFATAL) << "Width and height of the image must be positive values.";
    return false;
  }

  switch (pixel_format) {
    case GRAY_8:
    case RGB_888:
    case RGBA_8888:
      break;
    default:
      LOG(DFATAL) << "Pixel format must be GRAY_8, RGB_888, or RGBA_8888";
      return false;
  }

  width_ = width;
  height_ = height;
  pixel_format_ = pixel_format;
  return true;
}

// Initialize the basic parameter for writing the image. To use the default
// compression parameters, set 'params' to NULL.
bool PngScanlineWriter::Initialize(const PngCompressParams* params,
                                   GoogleString* png_image) {
  // Validate input arguments.
  if (!Validate(params, png_image)) {
    return false;
  }

  png_image->clear();

  const int bit_depth = 8;
  int color_type = -1;
  switch (pixel_format_) {
    case GRAY_8:
      color_type = PNG_COLOR_TYPE_GRAY;
      break;
    case RGB_888:
      color_type = PNG_COLOR_TYPE_RGB;
      break;
    default:  // RGBA_8888. Init() has filtered out invalid values.
      color_type = PNG_COLOR_TYPE_RGB_ALPHA;
  }

  png_structp png_ptr = png_struct_.png_ptr();
  png_infop info_ptr = png_struct_.info_ptr();

  if (setjmp(png_jmpbuf(png_ptr)) != 0) {
    // Jump to here if any error happens.
    Reset();
    return false;
  }

  if (params != NULL) {
    png_set_compression_strategy(png_ptr, params->compression_strategy);
    png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, params->filter_level);
  }

  png_set_write_fn(png_ptr, png_image, &WritePngToString, &PngFlush);
  png_set_IHDR(png_ptr, info_ptr, width_, height_, bit_depth, color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);
  was_initialized_ = true;
  return true;
}

// Write a scanline with the data provided. Return false in case of error.
bool PngScanlineWriter::WriteNextScanline(void *scanline_bytes) {
  if (was_initialized_ && row_ < height_) {
    png_write_row(png_struct_.png_ptr(),
                  reinterpret_cast<png_bytep>(scanline_bytes));
    ++row_;
    return true;
  }
  return false;
}

// Finalize write structure once all scanlines are written.
bool PngScanlineWriter::FinalizeWrite() {
  if (was_initialized_ && row_ == height_) {
    png_write_end(png_struct_.png_ptr(), png_struct_.info_ptr());
    return true;
  } else {
    Reset();
    return false;
  }
}

}  // namespace image_compression

}  // namespace pagespeed
