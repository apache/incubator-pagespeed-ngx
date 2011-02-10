/*
 * Copyright 2010 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

// Unit tests for endian-dependent operations used in image rewriting.

#include "net/instaweb/rewriter/public/image.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {
namespace {

TEST(ImageEndianTest, CharToIntTest) {
  EXPECT_EQ(0xff, CharToInt(-1));
  // Worried about C++ implicit conversions to/from int for arg passing:
  EXPECT_EQ(0x05, CharToInt(static_cast<char>(0xffffff05)));
  EXPECT_EQ(0x83, CharToInt(static_cast<char>(0xffffff83)));
  EXPECT_EQ(0x33, CharToInt(0x33));
  // Now test deserialization from buffer full of negative values
  const char buf[] = { 0xf1, 0xf2, 0xf3, 0xf4, 0 };
  EXPECT_EQ(0xf1f2, JpegIntAtPosition(buf, 0));
  EXPECT_EQ(0xf2f3, JpegIntAtPosition(buf, 1));
  EXPECT_EQ(0xf2f1, GifIntAtPosition(buf, 0));
  EXPECT_EQ(0xf4f3, GifIntAtPosition(buf, 2));
  EXPECT_EQ(static_cast<int>(0xf1f2f3f4), PngIntAtPosition(buf, 0));
}

}  // namespace
}  // namespace net_instaweb
