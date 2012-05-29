/*
 * Copyright 2011 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

// Unit tests for out-of-memory handling in Image class

#include "net/instaweb/rewriter/public/image.h"

#include <sys/time.h>
#include <sys/resource.h>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/image_test_base.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
namespace {

class ImageOomTest : public ImageTestBase {
 public:
  virtual void SetUp() {
    ImageTestBase::SetUp();

    // All of these tests need to be disabled under valgrind since
    // valgrind and setrlimit don't get along
    if (!RunningOnValgrind()) {
      getrlimit(RLIMIT_AS, &old_mem_limit_);

      // Limit ourselves to about 100 million bytes of memory --- not enough
      // to fit in the 10000x10000 image (which has 100 million pixels).
      rlimit new_mem_limit = old_mem_limit_;
      new_mem_limit.rlim_cur = 100000000;
      setrlimit(RLIMIT_AS, &new_mem_limit);
    }
  }

  virtual void TearDown() {
    if (!RunningOnValgrind()) {
      // Restore previous rlimit
      setrlimit(RLIMIT_AS, &old_mem_limit_);
    }
    ImageTestBase::TearDown();
  }

 private:
  rlimit old_mem_limit_;
};

TEST_F(ImageOomTest, BlankImage) {
  if (RunningOnValgrind()) {
    return;
  }

  Image::CompressionOptions* options = new Image::CompressionOptions();
  options->recompress_png = true;
  // Make sure creating gigantic image fails cleanly.
  ImagePtr giant(BlankImageWithOptions(10000, 10000, Image::IMAGE_PNG,
                                       GTestTempDir(), &handler_, options));
  EXPECT_FALSE(giant->EnsureLoaded(true));
}

TEST_F(ImageOomTest, LoadImage) {
  if (RunningOnValgrind()) {
    return;
  }

  GoogleString buf;
  bool not_progressive = false;
  ImagePtr giant(ReadImageFromFile(Image::IMAGE_JPEG, kLarge, &buf,
                                   not_progressive));
  EXPECT_FALSE(giant->EnsureLoaded(true));

  // Make sure we can still load a reasonable image OK.
  buf.clear();
  ImagePtr small(
      ReadImageFromFile(Image::IMAGE_PNG, kCuppa, &buf, not_progressive));
  EXPECT_TRUE(small->EnsureLoaded(true));
}

}  // namespace
}  // namespace net_instaweb
