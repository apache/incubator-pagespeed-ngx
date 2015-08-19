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

// Author: Tom Bergan

#include "base/logging.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace pagespeed {
namespace image_compression {
namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;

class ScanlineInterfaceFrameAdapterTest : public ::testing::Test {
 public:
  ScanlineInterfaceFrameAdapterTest()
    : message_handler_(new NullMutex) {
  }

 protected:
  void ReadGifFile(const char *filename) {
    CHECK(ReadTestFileWithExt(kGifTestDir, filename, &original_image_));
  }

 protected:
  MockMessageHandler message_handler_;
  GoogleString original_image_;
  GoogleString converted_image_;
};

struct TestCase {
  const char *input_gif;
  ImageFormat output_format;
  bool success;
  ScanlineStatusType status_type;
  ScanlineStatusSource status_source;
};

TEST_F(ScanlineInterfaceFrameAdapterTest, PrepareImage) {
  const TestCase cases[] = {
    { "animated.gif", IMAGE_JPEG, false, SCANLINE_STATUS_UNSUPPORTED_FEATURE,
      SCANLINE_TO_FRAME_WRITER_ADAPTER },
    { "animated.gif", IMAGE_PNG, false, SCANLINE_STATUS_UNSUPPORTED_FEATURE,
      SCANLINE_TO_FRAME_WRITER_ADAPTER },
    { "interlaced.gif", IMAGE_JPEG, true, SCANLINE_STATUS_SUCCESS },
    { "interlaced.gif", IMAGE_PNG, true, SCANLINE_STATUS_SUCCESS },
  };

  for (size_t k = 0; k < arraysize(cases); ++k) {
    const TestCase& test = cases[k];
    ScanlineStatus status;
    ImageSpec spec;

    const GoogleString test_info = GoogleString("in test case: ") +
        test.input_gif + ", " + ImageFormatToString(test.output_format);

    // Read this GIF.
    ReadGifFile(test.input_gif);
    status = ScanlineStatus();
    net_instaweb::scoped_ptr<MultipleFrameReader> reader(
        CreateImageFrameReader(pagespeed::image_compression::IMAGE_GIF,
                               original_image_.data(),
                               original_image_.length(),
                               &message_handler_,
                               &status));

    ASSERT_TRUE(status.Success()) << test_info;
    ASSERT_NE(static_cast<void*>(NULL), reader.get()) << test_info;

    status = ScanlineStatus();
    EXPECT_TRUE(reader->GetImageSpec(&spec, &status)) << test_info;
    EXPECT_TRUE(status.Success()) << test_info;

    // Setup a writer and check the return status of PrepareImage.
    net_instaweb::scoped_ptr<MultipleFrameWriter> writer(
        CreateImageFrameWriter(test.output_format,
                               NULL,
                               &converted_image_,
                               &message_handler_,
                               &status));
    ASSERT_NE(static_cast<void*>(NULL), reader.get()) << test_info;

    status = ScanlineStatus();
    EXPECT_EQ(test.success, writer->PrepareImage(&spec, &status)) << test_info;
    EXPECT_EQ(test.status_type, status.type()) << test_info;
    if (!status.Success()) {
      EXPECT_EQ(test.status_source, status.source()) << test_info;
    }
  }
}

}  // namespace
}  // namespace image_compression
}  // namespace pagespeed
