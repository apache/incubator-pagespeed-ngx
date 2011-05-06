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
// Author: skerner@google.com (Sam Kerner)

#include "base/scoped_ptr.h"
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/spriter/public/image_spriter.h"
#include "net/instaweb/spriter/public/image_spriter.pb.h"
#include "net/instaweb/spriter/mock_image_library_interface.h"
#include "net/instaweb/util/public/gmock.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {
namespace spriter {

namespace {

// GMock names.
using ::testing::Return;
using ::testing::_;
using ::testing::StrictMock;
using ::testing::DoAll;
using ::testing::SetArgumentPointee;

// Constants used in several tests.
const int kSpriteId = 12345;
const ImageLibraryInterface::FilePath kInBasePath("/input/path");
const ImageLibraryInterface::FilePath kOutBasePath("/output/path");
const ImageLibraryInterface::FilePath kCombinedImagePath("subdir/out.png");
const ImageLibraryInterface::FilePath kPngA("path/to/a.png");
const ImageLibraryInterface::FilePath kPngB("b.png");

// Set up a protobuf of spriting settings.  Tests use this to get
// a reasonable default.
void SetupCommonOptions(SpriterInput* spriter_input,
                        ImageFormat format) {
  spriter_input->set_id(kSpriteId);

  SpriteOptions* options = spriter_input->mutable_options();
  options->set_placement_method(VERTICAL_STRIP);
  options->set_input_base_path(kInBasePath);
  options->set_output_base_path(kOutBasePath);
  options->set_output_image_path(kCombinedImagePath);
  options->set_output_format(format);
}

// This class acts as a delegate to ImageLibraryInterface,
// asserting if any failure occurs.
class FailOnImageLibError : public ImageLibraryInterface::Delegate {
 public:
  // Implement ImageLibInterface::Delegate:
  virtual void OnError(const GoogleString& error) {
    ASSERT_TRUE(0) << "Unexpected error: " << error;
  }
};

// Test that ImageSpriter produces an empty image when asked to sprite
// zero images.
TEST(SpriterTest, ZeroImages) {
  SpriterInput spriter_input;
  SetupCommonOptions(&spriter_input, PNG);

  // Build mock ojects:
  FailOnImageLibError no_failures_allowed;

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockCanvas> >
      mock_canvas(new StrictMock<MockImageLibraryInterface::MockCanvas>);

  testing::StrictMock<MockImageLibraryInterface>
      mock_image_lib(kInBasePath, kOutBasePath, &no_failures_allowed);

  // No images: Combined image will be 0x0.
  EXPECT_CALL(mock_image_lib, CreateCanvas(0, 0))
      .WillOnce(Return(mock_canvas.get()));

  EXPECT_CALL(*mock_canvas, WriteToFile(kCombinedImagePath, PNG))
      .WillOnce(Return(true));

  // spriter.Sprite() will free the canvas it gets.  In the test,
  // the canvas is mock object |mock_canvas|.  Disown the pointer.
  EXPECT_FALSE(NULL == mock_canvas.release());

  ImageSpriter spriter(&mock_image_lib);
  scoped_ptr<SpriterResult> sprite_result(spriter.Sprite(spriter_input));

  ASSERT_TRUE(sprite_result.get());
  EXPECT_EQ(kSpriteId, sprite_result->id());
  EXPECT_EQ(kOutBasePath, sprite_result->output_base_path());
  EXPECT_EQ(0, sprite_result->image_position_size());
}

// Sprite a single image.  Test that ImageSpriter produces an image
// that is the same size as the input.
TEST(SpriterTest, OneImage) {
  SpriterInput spriter_input;
  SetupCommonOptions(&spriter_input, JPEG);

  spriter_input.add_input_image_set()->set_path(kPngA);

  FailOnImageLibError no_failures_allowed;

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockCanvas> >
      mock_canvas(new StrictMock<MockImageLibraryInterface::MockCanvas>);

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockImage> >
      mock_image_a(new StrictMock<MockImageLibraryInterface::MockImage>);

  testing::StrictMock<MockImageLibraryInterface>
      mock_image_lib(kInBasePath, kOutBasePath, &no_failures_allowed);

  EXPECT_CALL(mock_image_lib, ReadFromFile(kPngA))
      .WillOnce(Return(mock_image_a.get()));

  // Image #1: 10x11.
  EXPECT_CALL(*mock_image_a, GetDimensions(_, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(10),
                      SetArgumentPointee<1>(11),
                      Return(true)));

  EXPECT_CALL(*mock_canvas, DrawImage(mock_image_a.get(), 0, 0))
      .WillOnce(Return(true));

  // Canvas should be 10x11
  EXPECT_CALL(mock_image_lib, CreateCanvas(10, 11))
      .WillOnce(Return(mock_canvas.get()));

  EXPECT_CALL(*mock_canvas, WriteToFile(kCombinedImagePath, JPEG))
      .WillOnce(Return(true));

  // spriter.Sprite() will free these.
  EXPECT_FALSE(NULL == mock_canvas.release());
  EXPECT_FALSE(NULL == mock_image_a.release());

  ImageSpriter spriter(&mock_image_lib);
  scoped_ptr<SpriterResult> sprite_result(spriter.Sprite(spriter_input));

  ASSERT_TRUE(sprite_result.get());
  EXPECT_EQ(kSpriteId, sprite_result->id());
  EXPECT_EQ(kOutBasePath, sprite_result->output_base_path());
  EXPECT_EQ(1, sprite_result->image_position_size());
}

// Sprite two images.  Test that ImageSpriter produces an image
// that is the right overall size, and places the two images in
// the expected locations within the combined image.
TEST(SpriterTest, TwoImages) {
  SpriterInput spriter_input;
  SetupCommonOptions(&spriter_input, PNG);

  // Add two mock images.
  spriter_input.add_input_image_set()->set_path(kPngA);
  spriter_input.add_input_image_set()->set_path(kPngB);

  FailOnImageLibError no_failures_allowed;

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockCanvas> >
      mock_canvas(new StrictMock<MockImageLibraryInterface::MockCanvas>);

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockImage> >
      mock_image_a(new StrictMock<MockImageLibraryInterface::MockImage>);

  scoped_ptr<StrictMock<MockImageLibraryInterface::MockImage> >
      mock_image_b(new StrictMock<MockImageLibraryInterface::MockImage>);

  testing::StrictMock<MockImageLibraryInterface>
      mock_image_lib(kInBasePath, kOutBasePath, &no_failures_allowed);

  EXPECT_CALL(mock_image_lib, ReadFromFile(kPngA))
      .WillOnce(Return(mock_image_a.get()));

  EXPECT_CALL(*mock_image_a, GetDimensions(_, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(10),
                      SetArgumentPointee<1>(11),
                      Return(true)));

  EXPECT_CALL(mock_image_lib, ReadFromFile(kPngB))
      .WillOnce(Return(mock_image_b.get()));

  EXPECT_CALL(*mock_image_b, GetDimensions(_, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(20),
                      SetArgumentPointee<1>(21),
                      Return(true)));

  int expected_width = 20;  // max(10, 20)
  int expected_hieght = 32;  // 11 + 21
  EXPECT_CALL(mock_image_lib, CreateCanvas(expected_width, expected_hieght))
      .WillOnce(Return(mock_canvas.get()));

  // Draw the first image at the origin.
  EXPECT_CALL(*mock_canvas, DrawImage(mock_image_a.get(), 0, 0))
      .WillOnce(Return(true));

  // Draw the second image just below the first.
  EXPECT_CALL(*mock_canvas, DrawImage(mock_image_b.get(), 0, 11))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_canvas, WriteToFile(kCombinedImagePath, PNG))
      .WillOnce(Return(true));

  // spriter.Sprite() will free these.
  EXPECT_FALSE(NULL == mock_canvas.release());
  EXPECT_FALSE(NULL == mock_image_a.release());
  EXPECT_FALSE(NULL == mock_image_b.release());

  ImageSpriter spriter(&mock_image_lib);
  scoped_ptr<SpriterResult> sprite_result(spriter.Sprite(spriter_input));

  ASSERT_TRUE(sprite_result.get());
  EXPECT_EQ(kSpriteId, sprite_result->id());
  EXPECT_EQ(kOutBasePath, sprite_result->output_base_path());
  EXPECT_EQ(2, sprite_result->image_position_size());
  EXPECT_EQ(kCombinedImagePath, sprite_result->output_image_path());
}
}  // namesapce
}  // namespace spriter
}  // namespace net_instaweb
