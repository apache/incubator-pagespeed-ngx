/*
 * Copyright 2013 Google Inc.
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
// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/mock_critical_images_finder.h"


#include <map>                          // for map<>::mapped_type
#include <utility>                      // for make_pair

#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

void MockCriticalImagesFinder::UpdateCriticalImagesSetInDriver(
    RewriteDriver* driver) {
  CriticalImagesInfo* info = new CriticalImagesInfo;
  if (critical_images_ != NULL) {
    info->html_critical_images = *critical_images_;
  }
  if (css_critical_images_ != NULL) {
    info->css_critical_images = *css_critical_images_;
  }

  if (rendered_images_ != NULL) {
    RenderedImageDimensionsMap map;
    for (int i = 0; i < rendered_images_->image_size(); ++i) {
      const RenderedImages_Image& images = rendered_images_->image(i);
      map[images.src()] = std::make_pair(
          images.rendered_width(), images.rendered_height());
    }
    info->rendered_images_map = map;
  }

  driver->set_critical_images_info(info);
}

RenderedImages*
MockCriticalImagesFinder::ExtractRenderedImageDimensionsFromCache(
    RewriteDriver* driver) {
  if (rendered_images_.get() != NULL) {
    return new RenderedImages(*rendered_images_.get());
  }
  return NULL;
}

MockCriticalImagesFinder::~MockCriticalImagesFinder() {}

}  // namespace net_instaweb
