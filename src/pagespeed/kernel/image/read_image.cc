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
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/jpeg_reader.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/scanline_utils.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

// Create a scanline image reader.
ScanlineReaderInterface* CreateScanlineReader(
    ImageFormat image_type,
    const void* image_buffer,
    size_t buffer_length,
    MessageHandler* handler,
    ScanlineStatus* status) {
  scoped_ptr<ScanlineReaderInterface> allocated_reader;
  const char* which = NULL;

  switch (image_type) {
    case IMAGE_PNG:
      allocated_reader.reset(new PngScanlineReaderRaw(handler));
      which = "PngScanlineReaderRaw";
      break;

    case IMAGE_GIF:
      allocated_reader.reset(new GifScanlineReaderRaw(handler));
      which = "GifScanlineReader";
      break;

    case IMAGE_JPEG:
      allocated_reader.reset(new JpegScanlineReader(handler));
      which = "JpegScanlineReader";
      break;

    case IMAGE_WEBP:
      allocated_reader.reset(new WebpScanlineReader(handler));
      which = "WebpScanlineReader";
      break;

    case IMAGE_UNKNOWN:
      break;

    // No default so compiler will complain if any enum is not processed.
  }

  if (which == NULL) {
      *status = PS_LOGGED_STATUS(PS_LOG_DFATAL, handler,
                                 SCANLINE_STATUS_UNSUPPORTED_FORMAT,
                                 SCANLINE_UTIL,
                                 "invalid image type for reader: %d",
                                 image_type);
      return NULL;
  }
  if (allocated_reader.get() == NULL) {
    *status = PS_LOGGED_STATUS(PS_LOG_ERROR, handler,
                               SCANLINE_STATUS_MEMORY_ERROR,
                               SCANLINE_UTIL,
                               "failed to allocate %s", which);
    return NULL;
  }

  *status = allocated_reader->InitializeWithStatus(image_buffer, buffer_length);
  return status->Success() ? allocated_reader.release() : NULL;
}

// Returns an uninitialized scanline image writer.
ScanlineWriterInterface* InstantiateScanlineWriter(
    ImageFormat image_type,
    MessageHandler* handler,
    ScanlineStatus* status) {
  scoped_ptr<ScanlineWriterInterface> writer;
  const char* which = NULL;

  switch (image_type) {
    case pagespeed::image_compression::IMAGE_JPEG:
      {
        scoped_ptr<JpegScanlineWriter> jpeg_writer(
            new JpegScanlineWriter(handler));
        if (jpeg_writer != NULL) {
          // TODO(huibao): Set up error handling inside JpegScanlineWriter.
          // Remove 'setjmp' from the clients and remove the 'SetJmpBufEnv'
          // method.
          jmp_buf env;
          if (setjmp(env)) {
            // This code is run only when libjpeg hit an error, and
            // called longjmp(env). Note that this only works for as
            // long as this stack frame is valid.
            jpeg_writer->AbortWrite();
            return NULL;
          }

          jpeg_writer->SetJmpBufEnv(&env);
          writer.reset(jpeg_writer.release());
        }
        which  = "JpegScanlineWriter";
      }
      break;

    case pagespeed::image_compression::IMAGE_PNG:
      writer.reset(new PngScanlineWriter(handler));
      which = "PngScanlineWriter";
      break;

    case pagespeed::image_compression::IMAGE_WEBP:
      writer.reset(new WebpScanlineWriter(handler));
      which = "WebpScanlineWriter";
      break;

    default:
      *status = PS_LOGGED_STATUS(PS_LOG_DFATAL, handler,
                                 SCANLINE_STATUS_UNSUPPORTED_FORMAT,
                                 SCANLINE_UTIL,
                                 "invalid image type for writer: %d",
                                 image_type);
      return NULL;
  }

  if (writer.get() == NULL) {
    *status = PS_LOGGED_STATUS(PS_LOG_ERROR, handler,
                               SCANLINE_STATUS_MEMORY_ERROR,
                               SCANLINE_UTIL,
                               "failed to allocate %s", which);
    return NULL;
  }

  if (status->Success()) {
    return writer.release();
  } else {
    return NULL;
  }
}


// Returns an initialized scanline image writer.
ScanlineWriterInterface* CreateScanlineWriter(
    ImageFormat image_type,
    PixelFormat pixel_format,
    size_t width,
    size_t height,
    const void* config,
    GoogleString* image_data,
    MessageHandler* handler,
    ScanlineStatus* status) {
  scoped_ptr<ScanlineWriterInterface> writer(
      InstantiateScanlineWriter(image_type, handler, status));
  if (status->Success()) {
    *status = writer->InitWithStatus(width, height, pixel_format);
  }
  if (status->Success()) {
    *status = writer->InitializeWriteWithStatus(config, image_data);
  }
  if (status->Success()) {
    return writer.release();
  } else {
    return NULL;
  }
}


////////// ImageFrame API

MultipleFrameReader* CreateImageFrameReader(
    ImageFormat image_type,
    const void* image_buffer,
    size_t buffer_length,
    MessageHandler* handler,
    ScanlineStatus* status) {
  MultipleFrameReader* allocated_reader = NULL;

  // Native ImageFrame implementations
  // TODO(vchudnov): Fill in. For now we delegate everything.

  // Image formats for which we do not have an ImageFrame
  // implementation result in a wrapper around the corresponding
  // Scanline object.
  scoped_ptr<ScanlineReaderInterface> scanline_reader(
      CreateScanlineReader(image_type, image_buffer, buffer_length,
                           handler, status));
  if (status->Success()) {
    allocated_reader = new ScanlineToFrameReaderAdapter(
        scanline_reader.release(), handler);
    if (allocated_reader == NULL) {
      *status = PS_LOGGED_STATUS(
          PS_LOG_ERROR, handler,
          SCANLINE_STATUS_MEMORY_ERROR,
          SCANLINE_UTIL,
          "failed to allocate ScanlineToFrameReaderAdapter");
    } else {
      *status = allocated_reader->Initialize(image_buffer, buffer_length);
    }
  }
  return allocated_reader;
}

MultipleFrameWriter* CreateImageFrameWriter(
    ImageFormat image_type,
    const void* config,
    GoogleString* image_data,
    MessageHandler* handler,
    ScanlineStatus* status) {
  MultipleFrameWriter* allocated_writer = NULL;

  // Native ImageFrame implementations
  // TODO(vchudnov): Fill in. For now we delegate everything.

  // Image formats for which we do not have an ImageFrame
  // implementation result in a wrapper around the corresponding
  // Scanline object.
  scoped_ptr<ScanlineWriterInterface> scanline_writer(
      InstantiateScanlineWriter(image_type, handler, status));
  if (status->Success()) {
    allocated_writer = new ScanlineToFrameWriterAdapter(
        scanline_writer.release(), handler);
    if (allocated_writer == NULL) {
     *status = PS_LOGGED_STATUS(
         PS_LOG_ERROR, handler,
         SCANLINE_STATUS_MEMORY_ERROR,
         SCANLINE_UTIL,
         "failed to allocate ScanlineToFrameWriterAdapter");
    } else {
      *status = allocated_writer->Initialize(config, image_data);
    }
  }
  return allocated_writer;
}

////////// Utilities

bool ReadImage(ImageFormat image_type,
               const void* image_buffer,
               size_t buffer_length,
               void** pixels,
               PixelFormat* pixel_format,
               size_t* width,
               size_t* height,
               size_t* stride,
               MessageHandler* handler) {
  // Instantiate and initialize the reader based on image type.
  scoped_ptr<ScanlineReaderInterface> reader;
  reader.reset(CreateScanlineReader(image_type, image_buffer, buffer_length,
                                    handler));
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
