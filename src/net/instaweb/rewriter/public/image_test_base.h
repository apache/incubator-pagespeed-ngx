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

// Some common routines and constants for tests dealing with Images

#include "net/instaweb/rewriter/public/image.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class ImageTestBase : public testing::Test {
 protected:
  static const char kTestData[];
  static const char kCuppa[];
  static const char kBikeCrash[];
  static const char kIronChef[];
  static const char kCradle[];
  static const char kPuzzle[];
  static const char kLarge[];
  static const char kScenery[];

  typedef scoped_ptr<Image> ImagePtr;

  ImageTestBase() {}
  virtual ~ImageTestBase();

  // We use the output_type (ultimate expected output type after image
  // processing) to set up rewrite permissions for the resulting Image object.
  Image* ImageFromString(Image::Type output_type,
                         const GoogleString& name,
                         const GoogleString& contents,
                         bool progressive);

  // Read an image with given filename and compression options. This also
  // transfers the ownership of options to image object.
  Image* ReadFromFileWithOptions(
      const char* name, GoogleString* contents,
      Image::CompressionOptions* options);

  // We use the output_type (ultimate expected output type after image
  // processing) to set up rewrite permissions for the resulting Image object.
  Image* ReadImageFromFile(Image::Type output_type,
                           const char* filename, GoogleString* buffer,
                           bool progressive);

  StdioFileSystem file_system_;
  GoogleMessageHandler handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTestBase);
};

}  // namespace net_instaweb
