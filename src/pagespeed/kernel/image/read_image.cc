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

#include <stdlib.h>  // for malloc
#include "base/logging.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/png_optimizer.h"

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"                                                // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}  // extern "C"

namespace pagespeed {

namespace image_compression {

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
  switch (image_type) {
    case IMAGE_PNG:
      {
        scoped_ptr<PngScanlineReaderRaw> png_reader(new PngScanlineReaderRaw());
        if (png_reader == NULL ||
            !png_reader->Initialize(image_buffer, buffer_length)) {
          return false;
        }
        reader.reset(png_reader.release());
      }
      break;

    case IMAGE_GIF:
      LOG(INFO) << "Gif image is not supported.";
      break;

    case IMAGE_JPEG:
      LOG(INFO) << "Jpeg image is not supported.";
      break;

    case IMAGE_WEBP:
      LOG(INFO) << "WebP image is not supported.";
      break;

    default:
      LOG(INFO) << "Invalid image type.";
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
