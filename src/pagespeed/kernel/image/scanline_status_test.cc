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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace pagespeed {

namespace image_compression {
TEST(ScanlineStatusTest, ComesFromReader) {
  const ScanlineStatusSource kAllSources[] = {
    SCANLINE_UNKNOWN,
    SCANLINE_PNGREADER,
    SCANLINE_PNGREADERRAW,
    SCANLINE_GIFREADER,
    SCANLINE_GIFREADERRAW,
    SCANLINE_JPEGREADER,
    SCANLINE_WEBPREADER,
    SCANLINE_RESIZER,
    SCANLINE_PNGWRITER,
    SCANLINE_JPEGWRITER,
    SCANLINE_WEBPWRITER,
    SCANLINE_UTIL,
    SCANLINE_PIXEL_FORMAT_OPTIMIZER,
    FRAME_TO_SCANLINE_READER_ADAPTER,
    FRAME_TO_SCANLINE_WRITER_ADAPTER,
    SCANLINE_TO_FRAME_READER_ADAPTER,
    SCANLINE_TO_FRAME_WRITER_ADAPTER
  };

  EXPECT_EQ(NUM_SCANLINE_SOURCE, arraysize(kAllSources));

  for (int i = 0; i < NUM_SCANLINE_SOURCE; ++i) {
    ScanlineStatus status(SCANLINE_STATUS_SUCCESS, kAllSources[i], "");
    bool is_reader = (strstr(status.SourceStr(), "READER") != NULL);
    EXPECT_EQ(is_reader, status.ComesFromReader());
  }
}

}  // namespace image_compression

}  // namespace pagespeed
