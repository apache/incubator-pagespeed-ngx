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
#include <cstdint>
#include <vector>

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/frame_interface_optimizer.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/test_utils.h"
namespace {

using net_instaweb::MessageHandler;
using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::FrameSpec;
using pagespeed::image_compression::GRAY_8;
using pagespeed::image_compression::ImageSpec;
using pagespeed::image_compression::MultipleFramePaddingReader;
using pagespeed::image_compression::MultipleFrameReader;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PixelRgbaChannels;
using pagespeed::image_compression::PixelRgbaChannelsToString;
using pagespeed::image_compression::RgbaChannels;
using pagespeed::image_compression::RgbaToPackedRgba;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
using pagespeed::image_compression::RGBA_ALPHA;
using pagespeed::image_compression::RGBA_BLUE;
using pagespeed::image_compression::RGBA_GREEN;
using pagespeed::image_compression::RGBA_NUM_CHANNELS;
using pagespeed::image_compression::RGBA_RED;
using pagespeed::image_compression::ScanlineStatus;
using pagespeed::image_compression::SCANLINE_STATUS_INVOCATION_ERROR;
using pagespeed::image_compression::SCANLINE_STATUS_SUCCESS;
using pagespeed::image_compression::SCANLINE_UNKNOWN;
using pagespeed::image_compression::kAlphaTransparent;
using pagespeed::image_compression::size_px;

// Fake reader class that synthesizes a series of frames whose specs are
// given in the constructor argument. The color of the frame is the
// bit-wise inverse of the image background color.
class FakeReader : public MultipleFrameReader {
 public:
  FakeReader(const ImageSpec& image_spec,
             const std::vector<FrameSpec>& frames,
             MessageHandler* handler) :
      MultipleFrameReader(handler),
      image_spec_(image_spec),
      frames_(frames),
      state_(UNINITIALIZED) {
    Reset();
  }

  virtual ~FakeReader() {}

  virtual ScanlineStatus Reset() {
    current_frame_ = 0;
    next_frame_ = 0;
    current_scanline_ = 0;
    scanline_.reset();
    state_ = INITIALIZED;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus Initialize() {
    return Reset();
  }

  virtual bool HasMoreFrames() const {
    return (next_frame_ < frames_.size());
  }

  virtual bool HasMoreScanlines() const {
    return (current_scanline_ < frames_[current_frame_].height);
  }

  static void GetForegroundColor(const PixelRgbaChannels bg_color,
                                 PixelRgbaChannels fg_color) {
    for (int channel = 0;
         channel < static_cast<int>(RGBA_NUM_CHANNELS);
         ++channel) {
      fg_color[channel] = ~bg_color[channel];
    }
  }

  virtual ScanlineStatus PrepareNextFrame() {
    if ((state_ < INITIALIZED) || !HasMoreFrames()) {
      return ScanlineStatus(SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_UNKNOWN,
                            "FakeReader::PrepareNextFrame called unexpectedly");
    }

    current_frame_ = next_frame_;
    ++next_frame_;
    current_scanline_ = 0;
    FrameSpec frame = frames_[current_frame_];
    size_t bytes_per_pixel = GetBytesPerPixel(frame.pixel_format);

    scanline_.reset(new uint8_t[frame.width * bytes_per_pixel]);
    PixelRgbaChannels foreground_color;
    GetForegroundColor(image_spec_.bg_color, foreground_color);
    for (int i = 0; i < frame.width; ++i) {
      memcpy(&scanline_[i * bytes_per_pixel],
             foreground_color,
             bytes_per_pixel);
    }

    state_ = FRAME_PREPARED;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus ReadNextScanline(const void** out_scanline_bytes) {
    if (((state_ != FRAME_PREPARED) && (state_ != SCANLINE_READ)) ||
        (!HasMoreScanlines())) {
      return ScanlineStatus(SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_UNKNOWN,
                            "FakeReader::ReadNextScanline called unexpectedly");
    }

    *out_scanline_bytes = scanline_.get();
    ++current_scanline_;
    state_ = SCANLINE_READ;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus GetFrameSpec(FrameSpec* frame_spec) const {
    *frame_spec = frames_[current_frame_];
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual ScanlineStatus GetImageSpec(ImageSpec* image_spec) const {
    *image_spec = image_spec_;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

 private:
  enum State {
    UNINITIALIZED = 0,
    INITIALIZED,
    FRAME_PREPARED,
    SCANLINE_READ
  };
  ImageSpec image_spec_;
  std::vector<FrameSpec> frames_;

  size_px current_frame_;
  size_px next_frame_;
  size_px current_scanline_;
  net_instaweb::scoped_array<uint8_t> scanline_;

  State state_;

  DISALLOW_COPY_AND_ASSIGN(FakeReader);
};

// Verifies that the pixels in the positions [start,end) all have the
// value 'color' by comparing as many bytes as appropriate for the
// given pixel format.
void VerifyPixels(const void* const scanline, size_px start, size_px end,
                  const PixelRgbaChannels color, PixelFormat format) {
  size_t bytes_per_pixel = GetBytesPerPixel(format);
  for (size_px idx = start; idx < end; ++idx) {
    // We ASSERT rather than EXPECT here because, in case of failure,
    // we don't want a log message for every single pixel.
    ASSERT_EQ(0,
              memcmp(
                  static_cast<const uint8_t*>(scanline) + idx * bytes_per_pixel,
                  color,
                  bytes_per_pixel))
        << "[" << start << "," << end << "](bpp:" << bytes_per_pixel << ") "
        << "got: "
        << PixelRgbaChannelsToString(static_cast<const uint8_t *>(scanline) +
                                     idx * bytes_per_pixel)
        << "want: " << PixelRgbaChannelsToString(color);
  }
}

class MultipleFramePaddingReaderTest : public testing::Test {
 public:
  MultipleFramePaddingReaderTest()
      : message_handler_(new NullMutex) {
  }

 protected:
  // Tests that an image with 'image_spec' and a series of frames
  // described by 'all_frames' are properly padded by
  // MultipleFramePaddingReader. This method is invoked by several
  // test cases below.
  void TestAllFramesPadded(const ImageSpec& image_spec,
                           const std::vector<FrameSpec>& all_frames) {
    static const PixelRgbaChannels kTransparent = {0, 0, 0, kAlphaTransparent};

    net_instaweb::scoped_ptr<MultipleFrameReader> padder(
        new MultipleFramePaddingReader(
            new FakeReader(image_spec, all_frames, &message_handler_)));

    PixelRgbaChannels fg_color;
    FakeReader::GetForegroundColor(image_spec.bg_color, fg_color);

    FrameSpec frame_orig;  // FrameSpec specified in all_frames
    FrameSpec frame_spec;  // FrameSpec returned from MultipleFramePaddingReader
    ScanlineStatus status;
    EXPECT_TRUE(padder->Initialize(NULL, 0, &status)) << status.ToString();
    for (size_px frame_idx = 0; frame_idx < all_frames.size(); ++frame_idx) {
      ASSERT_TRUE(padder->HasMoreFrames());
      ASSERT_TRUE(padder->PrepareNextFrame(&status)) << status.ToString();

      FrameSpec frame_orig = all_frames[frame_idx];

      EXPECT_TRUE(padder->GetFrameSpec(&frame_spec, &status))
          << status.ToString();
      EXPECT_EQ(image_spec.width, frame_spec.width);
      EXPECT_EQ(image_spec.height, frame_spec.height);
      EXPECT_EQ(0, frame_spec.top);
      EXPECT_EQ(0, frame_spec.left);
      EXPECT_EQ(frame_orig.pixel_format, frame_spec.pixel_format);

      for (size_px line_idx = 0; line_idx < image_spec.height; ++line_idx) {
        ASSERT_TRUE(padder->HasMoreScanlines());
        const void* scanline = NULL;
        EXPECT_TRUE(padder->ReadNextScanline(&scanline, &status))
            << status.ToString();
        EXPECT_FALSE(NULL == scanline);

        size_px foreground_start = 0;
        size_px foreground_end = 0;

        if ((line_idx >=frame_orig.top) &&
            (line_idx < (frame_orig.top + frame_orig.height))) {
          foreground_start = image_spec.TruncateXIndex(frame_orig.left);
          foreground_end = image_spec.TruncateXIndex(frame_orig.left +
                                                     frame_orig.width);
        }

        VerifyPixels(scanline, 0, foreground_start,
                     (image_spec.use_bg_color ?
                      image_spec.bg_color : kTransparent),
                     frame_spec.pixel_format);
        VerifyPixels(scanline, foreground_start, foreground_end,
                     fg_color, frame_spec.pixel_format);
        VerifyPixels(scanline, foreground_end, image_spec.width,
                     (image_spec.use_bg_color ?
                      image_spec.bg_color : kTransparent),
                     frame_spec.pixel_format);
      }
      EXPECT_FALSE(padder->HasMoreScanlines());
    }
    EXPECT_FALSE(padder->HasMoreFrames());
  }

  // Tests that, for a given format, we properly pad each frame in an
  // image. The frames tested have several positions and sizes.
  void TestReaderPadsAllFrames(PixelFormat pixel_format, bool use_bg_color) {
    ImageSpec image_spec;
    FrameSpec frame_spec;
    PixelRgbaChannels bg_color_rgba;
    std::vector<FrameSpec> all_frames;

    bg_color_rgba[RGBA_RED] = 5;
    bg_color_rgba[RGBA_GREEN] = 15;
    bg_color_rgba[RGBA_BLUE] = 25;
    bg_color_rgba[RGBA_ALPHA] = 35;

    image_spec.width = 100;
    image_spec.height = 100;
    image_spec.num_frames = 1;
    image_spec.use_bg_color = use_bg_color;
    memcpy(image_spec.bg_color, bg_color_rgba, sizeof(bg_color_rgba));

    frame_spec.width = 20;
    frame_spec.height = 30;
    frame_spec.top = 10;
    frame_spec.left = 15;
    frame_spec.pixel_format = pixel_format;
    all_frames.push_back(frame_spec);

    frame_spec.width = 35;
    frame_spec.height = 17;
    frame_spec.top = 51;
    frame_spec.left = 14;
    frame_spec.pixel_format = pixel_format;
    all_frames.push_back(frame_spec);

    // Frame coincides with image.
    frame_spec.width = 100;
    frame_spec.height = 100;
    frame_spec.top = 0;
    frame_spec.left = 0;
    frame_spec.pixel_format = pixel_format;
    all_frames.push_back(frame_spec);

    // Frame offset and falls off image.
    frame_spec.width = 100;
    frame_spec.height = 100;
    frame_spec.top = 10;
    frame_spec.left = 10;
    frame_spec.pixel_format = pixel_format;
    all_frames.push_back(frame_spec);

    // Frame larger than image.
    frame_spec.width = 200;
    frame_spec.height = 200;
    frame_spec.top = 0;
    frame_spec.left = 0;
    frame_spec.pixel_format = pixel_format;
    all_frames.push_back(frame_spec);

    TestAllFramesPadded(image_spec, all_frames);
  }

  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleFramePaddingReaderTest);
};


TEST_F(MultipleFramePaddingReaderTest, ReaderPadsRGBA_8888) {
  TestReaderPadsAllFrames(RGBA_8888, true);
  TestReaderPadsAllFrames(RGBA_8888, false);
}

TEST_F(MultipleFramePaddingReaderTest, ReaderPadsRGB_888) {
  TestReaderPadsAllFrames(RGB_888, true);
  TestReaderPadsAllFrames(RGB_888, false);
}

TEST_F(MultipleFramePaddingReaderTest, ReaderPadsGRAY_8) {
  TestReaderPadsAllFrames(GRAY_8, true);
  TestReaderPadsAllFrames(GRAY_8, false);
}

}  // namespace
