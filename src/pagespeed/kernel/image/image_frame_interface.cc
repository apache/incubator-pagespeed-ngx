/*
 * Copyright 2014 Google Inc.
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

#include "base/logging.h"
#include "pagespeed/kernel/image/image_frame_interface.h"

namespace pagespeed {

namespace image_compression {

////////// ImageSpec

ImageSpec::ImageSpec() {
    Reset();
}

void ImageSpec::Reset() {
    width = 0;
    height = 0;
}

////////// FrameSpec

FrameSpec::FrameSpec() {
    Reset();
}

void  FrameSpec::Reset() {
  pixel_format = UNSUPPORTED;
}

////////// MultipleFrameReader

MultipleFrameReader::MultipleFrameReader(MessageHandler* const handler)
    : message_handler_(handler) {
  CHECK(handler != NULL);
}

MultipleFrameReader::~MultipleFrameReader() {
}

////////// MultipleFrameWriter

MultipleFrameWriter::MultipleFrameWriter(MessageHandler* const handler)
    : message_handler_(handler) {
  CHECK(handler != NULL);
}

MultipleFrameWriter::~MultipleFrameWriter() {
}

}  // namespace image_compression

}  // namespace pagespeed
