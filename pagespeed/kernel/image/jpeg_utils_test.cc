/*
 * Copyright 2009 Google Inc.
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

// Author: Satyanarayana Manyam

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_utils.h"
#include "pagespeed/kernel/image/test_utils.h"

// DO NOT INCLUDE LIBJPEG HEADERS HERE. Doing so causes build errors
// on Windows. If you need to call out to libjpeg, please add helper
// methods in jpeg_optimizer_test_helper.h.

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::JpegUtils;

// The JPEG_TEST_DIR_PATH macro is set by the gyp target that builds this file.
const char* kColorJpegFile = "sjpeg2";
const char* kGreyScaleJpegFile = "testgray";
const char* kEmptyJpegFile = "emptyfile";
const char* kQuality100JpegFile = "quality100";

// Given one of the above file names, read the contents of the file into the
// given destination string.

using pagespeed::image_compression::kJpegTestDir;
using pagespeed::image_compression::ReadTestFile;

TEST(JpegUtilsTest, GetImageQualityFromImage) {
  MockMessageHandler message_handler(new NullMutex);
  GoogleString src_data;
  ReadTestFile(kJpegTestDir, kGreyScaleJpegFile, "jpg", &src_data);
  EXPECT_EQ(85, JpegUtils::GetImageQualityFromImage(src_data.data(),
                                                    src_data.size(),
                                                    &message_handler));

  src_data.clear();
  ReadTestFile(kJpegTestDir, kColorJpegFile, "jpg", &src_data);
  EXPECT_EQ(75, JpegUtils::GetImageQualityFromImage(src_data.data(),
                                                    src_data.size(),
                                                    &message_handler));

  src_data.clear();
  ReadTestFile(kJpegTestDir, kEmptyJpegFile, "jpg", &src_data);
  EXPECT_EQ(-1, JpegUtils::GetImageQualityFromImage(src_data.data(),
                                                    src_data.size(),
                                                    &message_handler));

  src_data.clear();
  ReadTestFile(kJpegTestDir, kQuality100JpegFile, "jpg", &src_data);
  EXPECT_EQ(100, JpegUtils::GetImageQualityFromImage(src_data.data(),
                                                     src_data.size(),
                                                     &message_handler));
}

}  // namespace
