/*
 * Copyright 2012 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_IMAGE_TESTING_PEER_H_
#define NET_INSTAWEB_REWRITER_IMAGE_TESTING_PEER_H_

#include "net/instaweb/rewriter/public/image.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class ImageDim;

class ImageTestingPeer {
 public:
  ImageTestingPeer() { }

  static void SetResizedDimensions(const ImageDim& dim, Image* image) {
    image->SetResizedDimensions(dim);
  }

  static bool ShouldConvertToProgressive(int64 quality, Image* image) {
    return image->ShouldConvertToProgressive(quality);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTestingPeer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_IMAGE_TESTING_PEER_H_
