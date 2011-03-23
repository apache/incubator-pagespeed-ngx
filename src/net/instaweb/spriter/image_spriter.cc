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

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/spriter/public/image_spriter.h"
#include "net/instaweb/spriter/public/image_spriter.pb.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {
namespace spriter {

ImageSpriter::ImageSpriter(ImageLibraryInterface* image_lib)
    : image_lib_(image_lib) {}

SpriterResult* ImageSpriter::Sprite(
    const SpriterInput& spriter_input) {
  scoped_ptr<SpriterResult> spriter_result(new SpriterResult);

  spriter_result->set_id(spriter_input.id());
  spriter_result->set_output_base_path(
      spriter_input.options().output_base_path());
  spriter_result->set_output_image_path(
      spriter_input.options().output_image_path());
  switch (spriter_input.options().placement_method()) {
    case VERTICAL_STRIP: {
      if (!DrawImagesInVerticalStrip(spriter_input, spriter_result.get()))
        return NULL;
    } break;

    default: {
      DCHECK(0) << "Unhandled case.";
      return NULL;  // TODO(skerner): Error call to delegate.
    }
  }

  return spriter_result.release();
}

bool ImageSpriter::DrawImagesInVerticalStrip(
    const SpriterInput& spriter_input,
    SpriterResult* spriter_result) {
  typedef std::vector<ImageLibraryInterface::Image*> ImagePointerVector;
  ImagePointerVector images;
  STLElementDeleter<ImagePointerVector> images_deleter(&images);

  int max_image_width = 0;
  int total_y_offset = 0;

  // For each image, read it and compute its position based on its size.
  for (int i = 0, ie = spriter_input.input_image_set().size(); i < ie; ++i) {
    ImageLibraryInterface::FilePath image_path(
        spriter_input.input_image_set(i).path());

    scoped_ptr<ImageLibraryInterface::Image> image(
        image_lib_->ReadFromFile(image_path));

    int width, height;
    if (image.get() == NULL || !image->GetDimensions(&width, &height))
      return false;  // ReadFromFile() or GetDimensions() has called OnError.

    images.push_back(image.release());  // |images| takes ownership of |image|.

    ImagePosition* image_pos = spriter_result->add_image_position();
    image_pos->set_path(image_path);
    Rect* rect = image_pos->mutable_clip_rect();
    rect->set_width(width);
    rect->set_height(height);
    rect->set_x_pos(0);
    rect->set_y_pos(total_y_offset);

    total_y_offset += height;
    if (max_image_width < width)
      max_image_width = width;
  }

  // Write all images into a canvas, and write the canvas to a file.
  scoped_ptr<ImageLibraryInterface::Canvas> canvas(
      image_lib_->CreateCanvas(max_image_width, total_y_offset));
  if (!canvas.get())
    return false;

  for (int i = 0, ie = images.size(); i < ie; ++i) {
    const Rect& image_pos = spriter_result->image_position(i).clip_rect();
    if (!canvas->DrawImage(images[i], image_pos.x_pos(), image_pos.y_pos()))
      return false;
  }
  if (!canvas->WriteToFile(spriter_input.options().output_image_path(),
                           spriter_input.options().output_format()))
    return false;

  return true;
}

}  // namespace spriter
}  // namespace net_instaweb
