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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/pixel_format_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

using net_instaweb::MessageHandler;
using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::PngScanlineReaderRaw;
using pagespeed::image_compression::ReadTestFileWithExt;
using pagespeed::image_compression::kMessagePatternUnexpectedEOF;
using pagespeed::image_compression::kWebpTestDir;
using pagespeed::image_compression::PixelFormatOptimizer;

const char kOpaqueAlphaImage[] = "completely_opaque_32x20.png";
const char kNoAlphaImage[] = "opaque_32x20.png";

const char* kUnoptimizableImages[] = {
  "pagespeed_32x32_gray.png",    // no alpha, gray scale
  "opaque_32x20.png",            // no alpha, RGB
  "alpha_32x32.png",             // alpha, first few pixels are transparent
  "partially_opaque_32x20.png",  // alpha, only last pixel is transparent
};
const size_t kUnoptimizableImageCount = arraysize(kUnoptimizableImages);

// Message to ignore.
const char kMessagePatternScanline[] = "*scanline*";
const char kMessagePatternLongJmp[] = "*longjmp()*";

class PixelFormatOptimizerTest : public testing::Test {
 public:
  PixelFormatOptimizerTest() :
      message_handler_(new NullMutex),
      optimizer_(&message_handler_),
      input_reader_(&message_handler_),
      gold_reader_(&message_handler_) {
  }

  bool InitializeOptimizer(const char* file_name) {
    if (!ReadTestFileWithExt(kWebpTestDir, file_name, &input_image_)) {
      return false;
    }
    if (!input_reader_.Initialize(input_image_.data(),
                                  input_image_.length())) {
      return false;
    }
    return optimizer_.Initialize(&input_reader_).Success();
  }

  bool InitializeGoldReader(const char* file_name) {
    if (!ReadTestFileWithExt(kWebpTestDir, file_name, &gold_image_)) {
      return false;
    }
    return gold_reader_.Initialize(gold_image_.data(), gold_image_.length());
  }

 protected:
  MockMessageHandler message_handler_;
  PixelFormatOptimizer optimizer_;
  PngScanlineReaderRaw input_reader_;
  PngScanlineReaderRaw gold_reader_;
  GoogleString input_image_;
  GoogleString gold_image_;
  void* scanline_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PixelFormatOptimizerTest);
};

// The optimizable image will be converted to a new image.
TEST_F(PixelFormatOptimizerTest, Optimizable) {
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  ASSERT_TRUE(InitializeGoldReader(kNoAlphaImage));
  CompareImageReaders(&gold_reader_, &optimizer_);
}

// The un-optimizable images will stay the same after conversion.
TEST_F(PixelFormatOptimizerTest, Unoptimizable) {
  for (size_t i = 0; i < kUnoptimizableImageCount; ++i) {
    const char* file_name = kUnoptimizableImages[i];
    ASSERT_TRUE(InitializeOptimizer(file_name));
    ASSERT_TRUE(InitializeGoldReader(file_name));
    CompareImageReaders(&gold_reader_, &optimizer_);
  }
}

// Test that we don't have memory leakage if the object is initialized
// but no scanline is read.
TEST_F(PixelFormatOptimizerTest, InitializeWithoutRead) {
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
}

// Test that we don't have memory leakage if we don't read all of the
// scanlines.
TEST_F(PixelFormatOptimizerTest, ReadOneRow) {
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  ASSERT_TRUE(optimizer_.ReadNextScanline(&scanline_));
}

TEST_F(PixelFormatOptimizerTest, ReinitializeAfterOneRow) {
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  ASSERT_TRUE(optimizer_.ReadNextScanline(&scanline_));
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  ASSERT_TRUE(InitializeGoldReader(kNoAlphaImage));
  CompareImageReaders(&gold_reader_, &optimizer_);
}

TEST_F(PixelFormatOptimizerTest, ReInitializeAfterLastRow) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternScanline);
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  while (optimizer_.HasMoreScanLines()) {
    ASSERT_TRUE(optimizer_.ReadNextScanline(&scanline_));
  }
  ASSERT_FALSE(optimizer_.ReadNextScanline(&scanline_));
  // Initalize and use the object again.
  ASSERT_TRUE(InitializeOptimizer(kOpaqueAlphaImage));
  ASSERT_TRUE(InitializeGoldReader(kNoAlphaImage));
  CompareImageReaders(&gold_reader_, &optimizer_);
}

// The truncated image leads to a bad reader, which consequently causes
// the optimizer fail to initialize.
TEST_F(PixelFormatOptimizerTest, TruncatedImage) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternLongJmp);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternScanline);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);

  ASSERT_TRUE(ReadTestFileWithExt(kWebpTestDir, kOpaqueAlphaImage,
                                  &input_image_));
  int truncated_length = input_image_.length() * 0.8;
  ASSERT_TRUE(input_reader_.Initialize(input_image_.data(),
                                       truncated_length));
  ASSERT_FALSE(optimizer_.Initialize(&input_reader_).Success());
}

}  // namespace
