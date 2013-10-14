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

// Author: Satyanarayana Manyam

#include "pagespeed/kernel/image/image_converter.h"

using net_instaweb::MessageHandler;


#include <setjmp.h>
#include <cstddef>

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}  // extern "C"

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_interface.h"

namespace {
// In some cases, converting a PNG to JPEG results in a smaller
// file. This is at the cost of switching from lossless to lossy, so
// we require that the savings are substantial before in order to do
// the conversion. We choose 80% size reduction as the minimum before
// we switch a PNG to JPEG.
const double kMinJpegSavingsRatio = 0.8;

// As above, but for use when comparing lossy WebPs to lossless formats.
const double kMinWebpSavingsRatio = 0.8;

// If 'new_image' and 'new_image_type' represent a valid image that is
// smaller than 'threshold_ratio' times the size of the current
// 'best_image' (if any), then updates 'best_image' and
// 'best_image_type' to point to the values of 'new_image' and
// 'new_image_type'.
void SelectSmallerImage(
    pagespeed::image_compression::ImageConverter::ImageType new_image_type,
    const GoogleString& new_image,
    const double threshold_ratio,
    pagespeed::image_compression::ImageConverter::ImageType* const
    best_image_type,
    const GoogleString** const best_image,
    MessageHandler* handler) {
  size_t new_image_size = new_image.size();
  if (new_image_size > 0 &&
      ((*best_image_type ==
        pagespeed::image_compression::ImageConverter::IMAGE_NONE) ||
       ((new_image_type !=
         pagespeed::image_compression::ImageConverter::IMAGE_NONE) &&
        (*best_image != NULL) &&
        (new_image_size < (*best_image)->size() * threshold_ratio)))) {
    *best_image_type = new_image_type;
    *best_image = &new_image;

    PS_DLOG_INFO(handler, \
        "%p best is now %d", static_cast<void *>(best_image_type), \
        new_image_type);
  }
};
}  // namespace

namespace pagespeed {

namespace image_compression {

ScanlineStatus ImageConverter::ConvertImageWithStatus(
    ScanlineReaderInterface* reader,
    ScanlineWriterInterface* writer) {
  void* scan_row;
  while (reader->HasMoreScanLines()) {
    ScanlineStatus reader_status =
        reader->ReadNextScanlineWithStatus(&scan_row);
    if (!reader_status.Success()) {
      return reader_status;
    }
    ScanlineStatus writer_status =
        writer->WriteNextScanlineWithStatus(scan_row);
    if (!writer_status.Success()) {
      return writer_status;
    }
  }

  ScanlineStatus writer_status = writer->FinalizeWriteWithStatus();
  if (!writer_status.Success()) {
    return writer_status;
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

bool ImageConverter::ConvertPngToJpeg(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const JpegCompressionOptions& options,
    GoogleString* out,
    MessageHandler* handler) {
  DCHECK(out->empty());
  out->clear();

  // Initialize the reader.
  PngScanlineReader png_reader(handler);

  // Since JPEG only support 8 bits/channels, we need convert PNG
  // having 1,2,4,16 bits/channel to 8 bits/channel.
  //   -PNG_TRANSFORM_EXPAND expands 1,2 and 4 bit channels to 8 bit
  //                         channels, and de-colormaps images.
  //   -PNG_TRANSFORM_STRIP_16 will strip 16 bit channels to get 8 bit
  //                           channels.
  png_reader.set_transform(PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16);

  // Since JPEGs can only support opaque images, require this in the reader.
  png_reader.set_require_opaque(true);

  // Configure png reader error handlers.
  if (setjmp(*png_reader.GetJmpBuf())) {
    PS_LOG_DFATAL(handler, "png_jmpbuf not set locally: risk of memory leaks");
    return false;
  }

  if (!png_reader.InitializeRead(png_struct_reader, in)) {
    return false;
  }

  // Try converting if the image is opaque.
  bool jpeg_success = false;
  size_t width = png_reader.GetImageWidth();
  size_t height = png_reader.GetImageHeight();
  PixelFormat format = png_reader.GetPixelFormat();

  if (height > 0 && width > 0 && format != UNSUPPORTED) {
    JpegScanlineWriter jpeg_writer(handler);

    // libjpeg's error handling mechanism requires that longjmp be used
    // to get control after an error.
    jmp_buf env;
    if (setjmp(env)) {
      // This code is run only when libjpeg hit an error, and called
      // longjmp(env).
      jpeg_writer.AbortWrite();
    } else {
      jpeg_writer.SetJmpBufEnv(&env);
      if (jpeg_writer.Init(width, height, format)) {
        jpeg_writer.InitializeWrite(&options, out);
        jpeg_success = ConvertImage(&png_reader, &jpeg_writer);
      }
    }
  }
  return jpeg_success;
}

bool ImageConverter::OptimizePngOrConvertToJpeg(
    const PngReaderInterface& png_struct_reader, const GoogleString& in,
    const JpegCompressionOptions& options, GoogleString* out,
    bool* is_out_png, MessageHandler* handler) {

  bool jpeg_success = ConvertPngToJpeg(png_struct_reader, in, options, out,
                                       handler);

  // Try Optimizing the PNG.
  // TODO(satyanarayana): Try reusing the PNG structs for png->jpeg and optimize
  // png operations.
  GoogleString optimized_png_out;
  bool png_success = PngOptimizer::OptimizePngBestCompression(
      png_struct_reader, in, &optimized_png_out, handler);

  // Consider using jpeg's only if it gives substantial amount of byte savings.
  if (png_success &&
      (!jpeg_success ||
       out->size() > kMinJpegSavingsRatio * optimized_png_out.size())) {
    out->clear();
    out->assign(optimized_png_out);
    *is_out_png = true;
  } else {
    *is_out_png = false;
  }

  return jpeg_success || png_success;
}

bool ImageConverter::ConvertPngToWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const WebpConfiguration& webp_config,
    GoogleString* const out,
    bool* is_opaque,
    MessageHandler* handler) {
    WebpScanlineWriter* webp_writer = NULL;
    bool success = ConvertPngToWebp(png_struct_reader, in, webp_config,
                                    out, is_opaque, &webp_writer, handler);
    delete webp_writer;
    return success;
}

bool ImageConverter::ConvertPngToWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const WebpConfiguration& webp_config,
    GoogleString* const out,
    bool* is_opaque,
    WebpScanlineWriter** webp_writer,
    MessageHandler* handler) {
  DCHECK(out->empty());
  out->clear();

  if (*webp_writer != NULL) {
    PS_LOG_INFO(handler, "Expected *webp_writer == NULL");
    return false;
  }

  // Initialize the reader.
  PngScanlineReader png_reader(handler);

  // Since the WebP API only support 8 bits/channels, we need convert PNG
  // having 1,2,4,16 bits/channel to 8 bits/channel.
  //   -PNG_TRANSFORM_EXPAND expands 1,2 and 4 bit channels to 8 bit
  //                         channels, and de-colormaps images.
  //   -PNG_TRANSFORM_STRIP_16 will strip 16 bit channels to get 8 bit/channel
  //   -PNG_TRANSFORM_GRAY_TO_RGB will transform grayscale to RGB
  png_reader.set_transform(
      PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16 |
      PNG_TRANSFORM_GRAY_TO_RGB);

  // If alpha quality is zero, refuse to process transparent images.
  png_reader.set_require_opaque(webp_config.alpha_quality == 0);

  // Configure png reader error handlers.
  if (setjmp(*png_reader.GetJmpBuf())) {
    PS_LOG_DFATAL(handler, "png_jmpbuf not set locally: risk of memory leaks");
    return false;
  }
  if (!png_reader.InitializeRead(png_struct_reader, in, is_opaque)) {
    return false;
  }

  bool webp_success = false;
  size_t width = png_reader.GetImageWidth();
  size_t height = png_reader.GetImageHeight();
  PixelFormat format = png_reader.GetPixelFormat();

  (*webp_writer) = new WebpScanlineWriter(handler);

  if (height > 0 && width > 0 && format != UNSUPPORTED) {
    if ((*webp_writer)->Init(width, height, format) &&
        (*webp_writer)->InitializeWrite(&webp_config, out)) {
      webp_success = ConvertImage(&png_reader, *webp_writer);
    }
  }

  return webp_success;
}

ImageConverter::ImageType ImageConverter::GetSmallestOfPngJpegWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const JpegCompressionOptions* jpeg_options,
    const WebpConfiguration* webp_config,
    GoogleString* out,
    MessageHandler* handler) {
  GoogleString jpeg_out, png_out, webp_lossless_out, webp_lossy_out;
  const GoogleString* best_lossless_image = NULL;
  const GoogleString* best_lossy_image = NULL;
  const GoogleString* best_image = NULL;
  ImageType best_lossless_image_type = IMAGE_NONE;
  ImageType best_lossy_image_type = IMAGE_NONE;
  ImageType best_image_type = IMAGE_NONE;

  WebpScanlineWriter* webp_writer = NULL;
  WebpConfiguration webp_config_lossless;
  bool is_opaque = false;
  if (!ConvertPngToWebp(png_struct_reader, in, webp_config_lossless,
                        &webp_lossless_out, &is_opaque, &webp_writer,
                        handler)) {
    PS_DLOG_INFO(handler, "Could not convert image to lossless WebP");
    webp_lossless_out.clear();
  }
  if ((webp_config != NULL) &&
      (!webp_writer->InitializeWrite(webp_config, &webp_lossy_out) ||
       !webp_writer->FinalizeWrite())) {
    PS_DLOG_INFO(handler, "Could not convert image to custom WebP");
    webp_lossy_out.clear();
  }
  delete webp_writer;

  if (!PngOptimizer::OptimizePngBestCompression(png_struct_reader, in,
                                                &png_out, handler)) {
    PS_DLOG_INFO(handler, "Could not optimize PNG");
    png_out.clear();
  }

  // If jpeg options are passed in and we haven't determined for sure
  // that the image has transparency, try jpeg conversion.
  if ((jpeg_options != NULL) &&
      (webp_lossy_out.empty() || is_opaque) &&
      !ConvertPngToJpeg(png_struct_reader, in, *jpeg_options, &jpeg_out,
      handler)) {
    PS_DLOG_INFO(handler, "Could not convert image to JPEG");
    jpeg_out.clear();
  }

  SelectSmallerImage(IMAGE_NONE, in, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);
  SelectSmallerImage(IMAGE_WEBP, webp_lossless_out, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);
  SelectSmallerImage(IMAGE_PNG, png_out, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);

  SelectSmallerImage(IMAGE_WEBP, webp_lossy_out, 1,
                     &best_lossy_image_type, &best_lossy_image, handler);
  SelectSmallerImage(IMAGE_JPEG, jpeg_out, 1,
                     &best_lossy_image_type, &best_lossy_image, handler);

  // To compensate for the lower quality, the lossy images must be
  // substantially smaller than the lossless images.
  double threshold_ratio = (best_lossy_image_type == IMAGE_WEBP ?
                            kMinWebpSavingsRatio : kMinJpegSavingsRatio);
  best_image_type = best_lossless_image_type;
  best_image = best_lossless_image;
  SelectSmallerImage(best_lossy_image_type, *best_lossy_image, threshold_ratio,
                     &best_image_type, &best_image, handler);

  out->clear();
  out->assign((best_image != NULL) ? *best_image : in);

  return best_image_type;
}

}  // namespace image_compression

}  // namespace pagespeed
