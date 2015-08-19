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

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/frame_interface_optimizer.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace pagespeed {

namespace image_compression {

// Friend adapter so we can instantiate GifFrameReader.
class TestGifFrameReader : public  GifFrameReader {
 public:
  explicit TestGifFrameReader(MessageHandler* handler) :
      GifFrameReader(handler) {}
};

}  // namespace image_compression

}  // namespace pagespeed

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::kAlphaOpaque;
using pagespeed::image_compression::kAlphaTransparent;
using pagespeed::image_compression::size_px;
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::FrameToScanlineReaderAdapter;
using pagespeed::image_compression::GifFrameReader;
using pagespeed::image_compression::MultipleFramePaddingReader;
using pagespeed::image_compression::MultipleFrameReader;
using pagespeed::image_compression::PackAsRgba;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PixelRgbaChannels;
using pagespeed::image_compression::PixelRgbaChannelsToString;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::RGBA_8888;
using pagespeed::image_compression::ScanlineReaderInterface;
using pagespeed::image_compression::ScanlineStatus;
using pagespeed::image_compression::TestGifFrameReader;

// Tests for FrameToScanlineReaderAdapter composed with
// MultipleFramePaddingReader.
class FrameScanlineAdapterWithPaddingTest : public testing::Test {
 public:
  FrameScanlineAdapterWithPaddingTest()
      : message_handler_(new NullMutex) {
  }

 protected:
  // Verifies that the pixels in the positions [start,end) all have the
  // value 'color' by comparing as many bytes as appropriate for the
  // given pixel format.
  void VerifyPixels(const void* const scanline, size_px start, size_px end,
                    const PixelRgbaChannels color, PixelFormat format) {
    size_t bytes_per_pixel = GetBytesPerPixel(format);
    for (size_px idx = start; idx < end; ++idx) {
      // We ASSERT rather than EXPECT here because, in case of failure,
      // we don't want a log message for every single pixel.
      ASSERT_EQ(0, memcmp((static_cast<const uint8_t*>(scanline) +
                           idx * bytes_per_pixel),
                          color,
                          bytes_per_pixel))
          << "[" << start << "," << end << "] (bpp:" << bytes_per_pixel << ")"
          << " got: "
          << PixelRgbaChannelsToString(static_cast<const uint8_t *>(scanline) +
                                       idx * bytes_per_pixel)
          << " want: " << PixelRgbaChannelsToString(color);
    }
  }


 protected:
  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameScanlineAdapterWithPaddingTest);
};

TEST_F(FrameScanlineAdapterWithPaddingTest, ReaderPadsLines) {
  GoogleString in;
  ReadTestFile(kGifTestDir, "frame_smaller_than_screen", "gif", &in);

  // Note that these consts are specific to this particular
  // image we're testing.
  const PixelRgbaChannels kTransparent = {0, 0, 0, kAlphaTransparent};
  const PixelRgbaChannels kRed = {0xD9, 0x20, 0x20, kAlphaOpaque};
  const size_px kWidth = 15;
  const size_px kHeight = 14;
  const size_px kForegroundYBegin = 1;
  const size_px kForegroundYEnd = 4;
  const size_px kForegroundXBegin = 2;
  const size_px kForegroundXEnd = 7;

  net_instaweb::scoped_ptr<MultipleFrameReader> mf_reader(
      new TestGifFrameReader(&message_handler_));
  ASSERT_TRUE(mf_reader != NULL);
  net_instaweb::scoped_ptr<ScanlineReaderInterface> reader(
      new FrameToScanlineReaderAdapter(
          new MultipleFramePaddingReader(mf_reader.release())));
  ASSERT_TRUE(reader != NULL);
  ScanlineStatus status = reader->InitializeWithStatus(in.c_str(), in.size());
  ASSERT_TRUE(status.Success()) << status.ToString();

  EXPECT_EQ(kWidth, reader->GetImageWidth());
  EXPECT_EQ(kHeight, reader->GetImageHeight());
  EXPECT_EQ(RGBA_8888, reader->GetPixelFormat());

  uint32_t* scanline = NULL;
  for (int j = 0; j < kHeight; ++j) {
    EXPECT_TRUE(reader->HasMoreScanLines());
    reader->ReadNextScanlineWithStatus(reinterpret_cast<void **>(&scanline));
    EXPECT_TRUE(status.Success()) << status.ToString();
    if ((j < kForegroundYBegin) || (j >= kForegroundYEnd)) {
      VerifyPixels(scanline, 0, kWidth, kTransparent, reader->GetPixelFormat());
    } else {
      VerifyPixels(scanline, 0, kForegroundXBegin,
                   kTransparent, reader->GetPixelFormat());
      VerifyPixels(scanline, kForegroundXBegin, kForegroundXEnd,
                   kRed, reader->GetPixelFormat());
      VerifyPixels(scanline, kForegroundXEnd, kWidth,
                   kTransparent, reader->GetPixelFormat());
    }
  }
}

}  // namespace
