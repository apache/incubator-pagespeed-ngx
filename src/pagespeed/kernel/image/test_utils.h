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

#ifndef PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_
#define PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_

#include <cstddef>
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/read_image.h"

namespace pagespeed {

namespace image_compression {

  const char kTestRootDir[] = "/pagespeed/kernel/image/testdata/";

const char kGifTestDir[] = "gif/";
const char kPngSuiteTestDir[] = "pngsuite/";
const char kPngSuiteGifTestDir[] = "pngsuite/gif/";
const char kPngTestDir[] = "png/";
const char kJpegTestDir[] = "jpeg/";

bool ReadFile(const GoogleString& file_name,
              GoogleString* content);

bool ReadTestFile(const GoogleString& path,
                  const char* name,
                  const char* extension,
                  GoogleString* content);

bool ReadTestFileWithExt(const GoogleString& path,
                         const char* name_with_extension,
                         GoogleString* content);

void DecodeAndCompareImages(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_
