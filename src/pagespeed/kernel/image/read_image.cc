/*
 * Copyright 2013 Google Inc.
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

// Author: Huibao Lin

#include "pagespeed/kernel/image/read_image.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include "base/logging.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/jpeg_reader.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

namespace pagespeed {

namespace image_compression {

// Create a scanline image reader.
ScanlineReaderInterface* CreateScanlineReader(ImageFormat image_type,
                                              const void* image_buffer,
                                              size_t buffer_length) {
  switch (image_type) {
    case IMAGE_PNG:
      {
        scoped_ptr<PngScanlineReaderRaw> png_reader(new PngScanlineReaderRaw());
        if (png_reader != NULL &&
            png_reader->Initialize(image_buffer, buffer_length)) {
          return png_reader.release();
        }
      }
      break;

    case IMAGE_GIF:
      {
        scoped_ptr<GifScanlineReaderRaw> gif_reader(new GifScanlineReaderRaw());
        if (gif_reader != NULL &&
            gif_reader->Initialize(image_buffer, buffer_length)) {
          return gif_reader.release();
        }
      }
      break;

    case IMAGE_JPEG:
      {
        scoped_ptr<JpegScanlineReader> jpeg_reader(new JpegScanlineReader());
        if (jpeg_reader != NULL &&
            jpeg_reader->Initialize(image_buffer, buffer_length)) {
          return jpeg_reader.release();
        }
      }
      break;

    case IMAGE_WEBP:
      {
        scoped_ptr<WebpScanlineReader> webp_reader(new WebpScanlineReader());
        if (webp_reader != NULL &&
            webp_reader->Initialize(image_buffer, buffer_length)) {
          return webp_reader.release();
        }
      }
      break;

    default:
      LOG(DFATAL) << "Invalid image type.";
  }

  return NULL;
}

// Return a scanline image writer.
ScanlineWriterInterface* CreateScanlineWriter(
    ImageFormat image_type,
    PixelFormat pixel_format,
    size_t width,
    size_t height,
    const void* config,
    GoogleString* image_data) {
  switch (image_type) {
    case pagespeed::image_compression::IMAGE_JPEG:
      {
        scoped_ptr<JpegScanlineWriter> writer(new JpegScanlineWriter());
        if (writer != NULL) {
          const JpegCompressionOptions* jpeg_config =
              reinterpret_cast<const JpegCompressionOptions*>(config);

          // TODO(huibao): Set up error handling inside JpegScanlineWriter.
          // Remove 'setjmp' from the clients and remove the 'SetJmpBufEnv'
          // method.
          jmp_buf env;
          if (setjmp(env)) {
            // This code is run only when libjpeg hit an error, and called
            // longjmp(env).
            writer->AbortWrite();
            return NULL;
          }

          writer->SetJmpBufEnv(&env);
          if (writer->Init(width, height, pixel_format)) {
            writer->SetJpegCompressParams(*jpeg_config);
            if (writer->InitializeWrite(image_data)) {
              return reinterpret_cast<ScanlineWriterInterface*>(
                  writer.release());
            }
          }
        }
      }
      break;

    case pagespeed::image_compression::IMAGE_PNG:
      {
        scoped_ptr<PngScanlineWriter> writer(new PngScanlineWriter());
        if (writer != NULL) {
          const PngCompressParams* png_config =
              reinterpret_cast<const PngCompressParams*>(config);
          if (writer->Init(width, height, pixel_format) &&
              writer->Initialize(png_config, image_data)) {
            return reinterpret_cast<ScanlineWriterInterface*>(writer.release());
          }
        }
      }
      break;

    case pagespeed::image_compression::IMAGE_WEBP:
      {
        scoped_ptr<WebpScanlineWriter> writer(new WebpScanlineWriter());
        if (writer != NULL) {
          const WebpConfiguration* webp_config =
              reinterpret_cast<const WebpConfiguration*>(config);
          if (writer->Init(width, height, pixel_format) &&
              writer->InitializeWrite(*webp_config, image_data)) {
            return reinterpret_cast<ScanlineWriterInterface*>(writer.release());
          }
        }
      }
      break;

    default:
      LOG(FATAL) << "Invalid image type";
  }
  return NULL;
}

bool ReadImage(ImageFormat image_type,
               const void* image_buffer,
               size_t buffer_length,
               void** pixels,
               PixelFormat* pixel_format,
               size_t* width,
               size_t* height,
               size_t* stride) {
  // Instantiate and initialize the reader based on image type.
  scoped_ptr<ScanlineReaderInterface> reader;
  reader.reset(CreateScanlineReader(image_type, image_buffer, buffer_length));
  if (reader.get() == NULL) {
    return false;
  }

  // The following information is available after the reader is initialized.
  // Copy them to the outputs if they are requested.
  if (pixel_format != NULL) {
    *pixel_format = reader->GetPixelFormat();
  }
  if (width != NULL) {
    *width = reader->GetImageWidth();
  }
  if (height != NULL) {
    *height = reader->GetImageHeight();
  }

  // Round up stride to a multiplier of 4.
  size_t bytes_per_row4 = (((reader->GetBytesPerScanline() + 3) >> 2) << 2);
  if (stride != NULL) {
    *stride = bytes_per_row4;
  }

  // Decode the image data (pixels) if it has been requested.
  if (pixels == NULL) {
    return true;
  }
  *pixels = NULL;
  const size_t data_length = reader->GetImageHeight() * bytes_per_row4;
  unsigned char* image_data = static_cast<unsigned char*>(malloc(data_length));
  if (image_data == NULL) {
    return false;
  }

  unsigned char* row_data = image_data;
  unsigned char* scanline = NULL;
  while (reader->HasMoreScanLines()) {
    if (!reader->ReadNextScanline(reinterpret_cast<void**>(&scanline))) {
      free(image_data);
      return false;
    }
    memcpy(row_data, scanline, reader->GetBytesPerScanline());
    row_data += bytes_per_row4;
  }

  *pixels = static_cast<void*>(image_data);
  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
