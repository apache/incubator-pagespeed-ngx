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

// Author: Victor Chudnovsky

#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace pagespeed {

namespace image_compression {

////////// FrameToScanlineReaderAdapter

FrameToScanlineReaderAdapter::FrameToScanlineReaderAdapter(
    MultipleFrameReader* frame_reader) : impl_(frame_reader),
                                         image_spec_(NULL),
                                         frame_spec_(NULL) {
  CHECK(frame_reader != NULL);
}

bool FrameToScanlineReaderAdapter::Reset() {
  image_spec_ = NULL;
  frame_spec_ = NULL;
  return impl_->Reset().Success();
}

size_t FrameToScanlineReaderAdapter::GetBytesPerScanline() {
  return GetBytesPerPixel(GetPixelFormat()) * GetImageWidth();
}

bool FrameToScanlineReaderAdapter::HasMoreScanLines() {
  return impl_->HasMoreScanlines();
}

ScanlineStatus FrameToScanlineReaderAdapter::InitializeWithStatus(
    const void* image_buffer,
    size_t const buffer_length) {
  ScanlineStatus status = impl_->Initialize(image_buffer, buffer_length);
  if (status.Success()) {
    status = impl_->GetImageSpec(&image_spec_);
  }
  if (status.Success()) {
    status = impl_->PrepareNextFrame();
  }
  if (status.Success()) {
    status = impl_->GetFrameSpec(&frame_spec_);
  }
  return status;
}

ScanlineStatus FrameToScanlineReaderAdapter::ReadNextScanlineWithStatus(
    void** const out_scanline_bytes) {
  return impl_->ReadNextScanline(const_cast<const void**>(out_scanline_bytes));
}

size_t FrameToScanlineReaderAdapter::GetImageHeight() {
  return (image_spec_ != NULL) ? image_spec_->height : 0;
}

size_t FrameToScanlineReaderAdapter::GetImageWidth() {
  return (image_spec_ != NULL) ? image_spec_->width : 0;
}

PixelFormat FrameToScanlineReaderAdapter::GetPixelFormat() {
  return (frame_spec_ != NULL) ? frame_spec_->pixel_format : UNSUPPORTED;
}


////////// FrameToScanlineWriterAdapter

FrameToScanlineWriterAdapter::FrameToScanlineWriterAdapter(
    MultipleFrameWriter* const frame_writer)
    : impl_(frame_writer), init_done_(false) {
  CHECK(frame_writer != NULL);
}

ScanlineStatus FrameToScanlineWriterAdapter::InitWithStatus(
    const size_t width,
    const size_t height,
    const PixelFormat pixel_format) {
  image_spec_.width = width;
  image_spec_.height = height;
  frame_spec_.pixel_format = pixel_format;
  init_done_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus FrameToScanlineWriterAdapter::InitializeWriteWithStatus(
    const void* const config,
    GoogleString* const out) {
  if (!init_done_) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, impl_->message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_TO_SCANLINE_WRITER_ADAPTER,
                            "Initialize() called before Init()");
  }

  ScanlineStatus status = impl_->Initialize(config, out);
  if (status.Success()) {
    status = impl_->PrepareImage(&image_spec_);
  }
  if (status.Success()) {
    status = impl_->PrepareNextFrame(&frame_spec_);
  }
  return status;
}

ScanlineStatus FrameToScanlineWriterAdapter::WriteNextScanlineWithStatus(
    const void* const scanline_bytes) {
  return impl_->WriteNextScanline(scanline_bytes);
}

ScanlineStatus FrameToScanlineWriterAdapter::FinalizeWriteWithStatus() {
  return impl_->FinalizeWrite();
}


////////// ScanlineToFrameReaderAdapter

ScanlineToFrameReaderAdapter::ScanlineToFrameReaderAdapter(
    ScanlineReaderInterface* const scanline_reader,
    MessageHandler* const message_handler)
    : MultipleFrameReader(message_handler),
      state_(UNINITIALIZED),
      impl_(scanline_reader) {
  CHECK(scanline_reader != NULL);
}

ScanlineStatus ScanlineToFrameReaderAdapter::Reset() {
  state_ = UNINITIALIZED;
  frame_spec_.Reset();
  image_spec_.Reset();

  return (impl_->Reset() ?
          ScanlineStatus(SCANLINE_STATUS_SUCCESS) :
          PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                           SCANLINE_STATUS_INTERNAL_ERROR,
                           SCANLINE_TO_FRAME_READER_ADAPTER,
                           "Error in ScanlineReaderInterface::Reset()"));
}

ScanlineStatus ScanlineToFrameReaderAdapter::Initialize(
    const void* const image_buffer,
    const size_t buffer_length) {
  ScanlineStatus status = Reset();

  if (status.Success()) {
    status = impl_->InitializeWithStatus(image_buffer, buffer_length);
  }

  if (status.Success()) {
    image_spec_.width = impl_->GetImageWidth();
    image_spec_.height = impl_->GetImageHeight();
  }

  state_ = status.Success() ? INITIALIZED : ERROR;
  return status;
}

ScanlineStatus ScanlineToFrameReaderAdapter::PrepareNextFrame() {
  if (HasMoreFrames()) {
    frame_spec_.Reset();
    // TODO(vchudnov): Populate frame_spec_ fields.
  } else {
    state_ = ERROR;
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_TO_FRAME_READER_ADAPTER,
                            "PrepareNextFrame()");
  }

  frame_spec_.pixel_format = impl_->GetPixelFormat();
  state_ = FRAME_PREPARED;

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

bool ScanlineToFrameReaderAdapter::HasMoreScanlines() const {
  return impl_->HasMoreScanLines();
}

ScanlineStatus ScanlineToFrameReaderAdapter::ReadNextScanline(
    const void** const out_scanline_bytes) {
  return impl_->ReadNextScanlineWithStatus(
      const_cast<void**>(out_scanline_bytes));
}

bool ScanlineToFrameReaderAdapter::HasMoreFrames() const {
  // Note that once PrepareNextFrame is called, state_ will no longer
  // be INITIALIZED.
  return (state_ == INITIALIZED);
}

ScanlineStatus ScanlineToFrameReaderAdapter::GetImageSpec(
    const ImageSpec** const spec) const {
  *spec = const_cast<const ImageSpec*>(&image_spec_);
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus ScanlineToFrameReaderAdapter::GetFrameSpec(
    const FrameSpec** const spec) const {
  *spec = const_cast<const FrameSpec*>(&frame_spec_);
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

////////// ScanlineToFrameWriterAdapter

ScanlineToFrameWriterAdapter::ScanlineToFrameWriterAdapter(
    ScanlineWriterInterface* const scanline_writer,
    MessageHandler* const handler) : MultipleFrameWriter(handler),
                                     state_(UNINITIALIZED),
                                     impl_(scanline_writer),
                                     config_(NULL),
                                     out_(NULL) {
  CHECK(scanline_writer != NULL);
}

ScanlineStatus ScanlineToFrameWriterAdapter::Initialize(
    const void* const config,
    GoogleString* const out) {
  config_ = config;
  out_ = out;
  state_ = INITIALIZED;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus ScanlineToFrameWriterAdapter::PrepareNextFrame(
    const FrameSpec* const spec) {
  if (state_ != IMAGE_PREPARED) {
    state_ = ERROR;
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_TO_FRAME_WRITER_ADAPTER,
                            "prepare next frame should be called after "
                            "prepare image, and only once");
  }

  frame_spec_ = spec;
  ScanlineStatus status = impl_->InitWithStatus(image_spec_->width,
                                                image_spec_->height,
                                                frame_spec_->pixel_format);
  if (status.Success()) {
    status = impl_->InitializeWriteWithStatus(config_, out_);
  }
  state_ = (status.Success() ? FRAME_PREPARED : ERROR);
  return status;
}

ScanlineStatus ScanlineToFrameWriterAdapter::WriteNextScanline(
    const void* const scanline_bytes) {
  return impl_->WriteNextScanlineWithStatus(scanline_bytes);
}

ScanlineStatus ScanlineToFrameWriterAdapter::FinalizeWrite() {
  return impl_->FinalizeWriteWithStatus();
}

ScanlineStatus ScanlineToFrameWriterAdapter::PrepareImage(
    const ImageSpec* const spec) {
  image_spec_ = spec;
  state_ = IMAGE_PREPARED;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
