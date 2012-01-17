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
//
// Some common routines and constants for tests dealing with Images

#include "net/instaweb/rewriter/public/image_test_base.h"

#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char ImageTestBase::kTestData[] = "/net/instaweb/rewriter/testdata/";
const char ImageTestBase::kCuppa[] = "Cuppa.png";
const char ImageTestBase::kBikeCrash[] = "BikeCrashIcn.png";
const char ImageTestBase::kIronChef[] = "IronChef2.gif";
const char ImageTestBase::kCradle[] = "CradleAnimation.gif";
const char ImageTestBase::kPuzzle[] = "Puzzle.jpg";
const char ImageTestBase::kLarge[] = "Large.png";
const char ImageTestBase::kScenery[] = "Scenery.webp";

ImageTestBase::~ImageTestBase() {
}

// We use the output_type (ultimate expected output type after image
// processing) to set up rewrite permissions for the resulting Image object.
Image* ImageTestBase::ImageFromString(
    Image::Type output_type, const GoogleString& name,
    const GoogleString& contents, bool progressive) {
  net_instaweb::Image::CompressionOptions* image_options =
      new net_instaweb::Image::CompressionOptions();
  image_options->webp_preferred = output_type == Image::IMAGE_WEBP;
  image_options->jpeg_quality = -1;
  image_options->progressive_jpeg = progressive;
  image_options->convert_png_to_jpeg =  output_type == Image::IMAGE_JPEG;

  return NewImage(contents, name, GTestTempDir(), image_options, &handler_);
}

Image* ImageTestBase::ReadFromFileWithOptions(
    const char* name, GoogleString* contents,
    Image::CompressionOptions* options) {
  EXPECT_TRUE(file_system_.ReadFile(
      StrCat(GTestSrcDir(), kTestData, name).c_str(),
      contents, &handler_));
  return NewImage(*contents, name, GTestTempDir(), options, &handler_);
}

Image* ImageTestBase::ReadImageFromFile(
    Image::Type output_type, const char* filename, GoogleString* buffer,
    bool progressive) {
  EXPECT_TRUE(file_system_.ReadFile(
      StrCat(GTestSrcDir(), kTestData, filename).c_str(),
      buffer, &handler_));
  return ImageFromString(output_type, filename, *buffer, progressive);
}

}  // namespace net_instaweb
