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

#ifndef NET_INSTAWEB_SPRITER_MOCK_IMAGE_LIBRARY_INTERFACE_H_
#define NET_INSTAWEB_SPRITER_MOCK_IMAGE_LIBRARY_INTERFACE_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/gmock.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/spriter/image_library_interface.h"


namespace net_instaweb {
namespace spriter {

class MockImageLibraryInterface : public ImageLibraryInterface {
 public:
  class MockImage : public ImageLibraryInterface::Image {
   public:
    MockImage() : Image(NULL) {}
    MOCK_METHOD2(GetDimensions, bool(int* out_width, int* out_height));
    virtual ~MockImage() {}
   private:
    explicit MockImage(MockImageLibraryInterface* lib) : Image(lib) {}
  };

  MockImageLibraryInterface(const FilePath& base_input_path,
                            const FilePath& base_output_path,
                            Delegate* delegate)
      : ImageLibraryInterface(base_input_path, base_output_path, delegate) {
  }

  // Read an image from disk.  Return NULL (after calling delegate
  // method) on error.  Caller owns the returned pointer.
  MOCK_METHOD1(ReadFromFile, Image* (const FilePath& path));

  // Canvases are mutable rectangles onto which a program may draw.
  // For now, we support stamping images into a canvas, and writing
  // a canvas to a file.
  class MockCanvas : public ImageLibraryInterface::Canvas {
   public:
    MockCanvas() : Canvas(NULL) {}
    virtual ~MockCanvas() {}
    MOCK_METHOD3(DrawImage, bool(const Image* image, int x, int y));
    MOCK_METHOD2(WriteToFile, bool(const FilePath& write_path,
                                   ImageFormat format));
  };

  MOCK_METHOD2(CreateCanvas, Canvas* (int width, int height));

  virtual ~MockImageLibraryInterface() {}
};

}  // namespace spriter
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SPRITER_MOCK_IMAGE_LIBRARY_INTERFACE_H_
