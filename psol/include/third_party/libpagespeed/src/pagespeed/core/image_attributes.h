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

#ifndef PAGESPEED_CORE_IMAGE_ATTRIBUTES_H_
#define PAGESPEED_CORE_IMAGE_ATTRIBUTES_H_

#include "base/basictypes.h"

namespace pagespeed {

class Resource;

// ImageAttributes provides image-specific APIs for an image resource,
// such as width, height, etc.
class ImageAttributes {
 public:
  ImageAttributes();
  virtual ~ImageAttributes();

  virtual int GetImageWidth() = 0;
  virtual int GetImageHeight() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageAttributes);
};

// ImageAttributesFactory provides an API to instantiate ImageAttributes
// from a specified Resource.
class ImageAttributesFactory {
 public:
  ImageAttributesFactory();
  virtual ~ImageAttributesFactory();

  // Returns a new ImageAttributes instance, or NULL. Callers should
  // always check for NULL, even if the passed-in resource is a valid image
  // Resource. For instance, in cases where the runtime does not support a
  // given image format, this method can return NULL for a valid image
  // resource.
  virtual ImageAttributes* NewImageAttributes(
      const Resource* resource) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageAttributesFactory);
};

// Implementation of ImageAttributes that takes all attributes as
// constructor arguments.
class ConcreteImageAttributes : public ImageAttributes {
 public:
  ConcreteImageAttributes(int width, int height);
  virtual ~ConcreteImageAttributes();

  virtual int GetImageWidth();
  virtual int GetImageHeight();

 private:
  const int width_;
  const int height_;
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_IMAGE_ATTRIBUTES_H_
