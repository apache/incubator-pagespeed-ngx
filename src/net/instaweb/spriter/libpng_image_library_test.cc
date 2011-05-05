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
// Author: abliss@google.com (Adam Bliss)

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/spriter/libpng_image_library.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kTestData[] = "/net/instaweb/rewriter/testdata/";
const char kCuppa[] = "Cuppa.png";
const char kBikeCrash[] = "BikeCrashIcn.png";

}  // namespace

namespace net_instaweb {
namespace spriter {

class LibpngImageLibraryTest : public testing::Test {
 protected:
  class LogDelegate : public ImageLibraryInterface::Delegate {
    virtual void OnError(const GoogleString& error) const {
      ASSERT_TRUE(false) << "Unexpected error: " << error;
    }
  };
  virtual void SetUp() {
    delegate_.reset(new LogDelegate());
    mkdir(GTestTempDir().c_str(), 0777);
    src_library_.reset(new LibpngImageLibrary(StrCat(GTestSrcDir(), kTestData),
        StrCat(GTestTempDir(), "/"), delegate_.get()));
    tmp_library_.reset(new LibpngImageLibrary(StrCat(GTestTempDir(), "/"),
        StrCat(GTestTempDir(), "/"), delegate_.get()));
  }
  ImageLibraryInterface::Image* ReadFromFile(const StringPiece& filename) {
    return src_library_->ReadFromFile(filename.as_string());
  }
  ImageLibraryInterface::Canvas* CreateCanvas(int width, int height) {
    return src_library_->CreateCanvas(width, height);
  }
  ImageLibraryInterface::Image* WriteAndRead(ImageLibraryInterface::Canvas* c) {
    bool success = c->WriteToFile("out.png", PNG);
    return success ? tmp_library_->ReadFromFile("out.png") : NULL;
  }

  // A library that reads in our test source data and writes in our temp dir.
  scoped_ptr<LibpngImageLibrary> src_library_;
  // A library that reads and writes in our temp dir.
  scoped_ptr<LibpngImageLibrary> tmp_library_;
  scoped_ptr<LogDelegate> delegate_;
};


TEST_F(LibpngImageLibraryTest, TestCompose) {
  //  65x70
  scoped_ptr<ImageLibraryInterface::Image> image1(ReadFromFile(kCuppa));
  // 100x100
  scoped_ptr<ImageLibraryInterface::Image> image2(ReadFromFile(kBikeCrash));
  ASSERT_TRUE(image1.get() != NULL);
  ASSERT_TRUE(image2.get() != NULL);
  scoped_ptr<ImageLibraryInterface::Canvas> canvas(CreateCanvas(100, 170));
  ASSERT_TRUE(canvas != NULL);
  ASSERT_TRUE(canvas->DrawImage(image1.get(), 0, 0));
  ASSERT_TRUE(canvas->DrawImage(image2.get(), 0, 70));
  ASSERT_TRUE(canvas->WriteToFile("out.png", PNG));
  scoped_ptr<ImageLibraryInterface::Image> image3(WriteAndRead(canvas.get()));
  ASSERT_TRUE(image3.get() != NULL);
  int width, height;
  ASSERT_TRUE(image3->GetDimensions(&width, &height));
  EXPECT_EQ(100, width);
  EXPECT_EQ(170, height);
}

}  // namespace spriter
}  // namespace net_instaweb
