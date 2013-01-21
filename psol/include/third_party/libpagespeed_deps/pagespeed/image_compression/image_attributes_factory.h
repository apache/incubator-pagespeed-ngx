// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_IMAGE_COMPRESSION_IMAGE_ATTRIBUTES_FACTORY_H_
#define PAGESPEED_IMAGE_COMPRESSION_IMAGE_ATTRIBUTES_FACTORY_H_

#include "base/basictypes.h"

#include "pagespeed/core/image_attributes.h"

namespace pagespeed {
namespace image_compression {

// Implementation of pagespeed::ImageAttributesFactory that knows how to
// generate ImageAttributes instances for all of the major image formats
// (PNG, GIF, JPEG, etc).
class ImageAttributesFactory : public pagespeed::ImageAttributesFactory {
 public:
  ImageAttributesFactory();
  virtual ~ImageAttributesFactory();

  virtual ImageAttributes* NewImageAttributes(
      const pagespeed::Resource* resource) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageAttributesFactory);
};

}  // namespace image_compression
}  // namespace pagespeed

#endif  // PAGESPEED_IMAGE_COMPRESSION_IMAGE_ATTRIBUTES_FACTORY_H_
