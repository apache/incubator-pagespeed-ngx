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

#ifndef NET_INSTAWEB_SPRITER_IMAGE_SPRITER_H_
#define NET_INSTAWEB_SPRITER_IMAGE_SPRITER_H_

#include "net/instaweb/spriter/image_library_interface.h"

namespace net_instaweb {
namespace spriter {

class ImageSpriter {
 public:
  explicit ImageSpriter(ImageLibraryInterface* image_lib);

  // Caller takes ownership of the returned pointer.
  // A return value of NULL indicates an error in some image
  // operation.  image_lib_ has a delegate, whose error handler
  // will be called before NULL is returned.
  SpriterResult* Sprite(const SpriterInput& spriter_input);

 private:
  bool DrawImagesInVerticalStrip(
      const SpriterInput& spriter_input,
      SpriterResult* spriter_result);

  ImageLibraryInterface* image_lib_;

  DISALLOW_COPY_AND_ASSIGN(ImageSpriter);
};

}  // namespace spriter
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SPRITER_IMAGE_SPRITER_H_
