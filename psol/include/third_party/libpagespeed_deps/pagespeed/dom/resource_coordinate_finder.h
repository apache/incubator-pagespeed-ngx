// Copyright 2011 Google Inc.
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

#ifndef PAGESPEED_DOM_RESOURCE_COORDINATE_FINDER_H_
#define PAGESPEED_DOM_RESOURCE_COORDINATE_FINDER_H_

#include "pagespeed/core/dom.h"

#include <map>
#include <vector>

namespace pagespeed {

class PagespeedInput;
class Resource;

namespace dom {

// DOM visitor that finds the coordinates of resources (e.g. images)
// in the coordinate space of the top-level document.
class ResourceCoordinateFinder
    : public pagespeed::ExternalResourceDomElementVisitor {
 public:
  ResourceCoordinateFinder(
      const pagespeed::PagespeedInput* input,
      std::map<const pagespeed::Resource*, std::vector<pagespeed::DomRect> >*
      resource_to_rect_map)
      : input_(input),
        resource_to_rect_map_(resource_to_rect_map),
        x_translate_(0),
        y_translate_(0) {}

  ResourceCoordinateFinder(
      const pagespeed::PagespeedInput* input,
      std::map<const pagespeed::Resource*, std::vector<pagespeed::DomRect> >*
          resource_to_rect_map,
      int x_translate, int y_translate)
      : input_(input),
        resource_to_rect_map_(resource_to_rect_map),
        x_translate_(x_translate),
        y_translate_(y_translate) {}

  virtual void VisitUrl(const pagespeed::DomElement& node,
                        const std::string& url);
  virtual void VisitDocument(const pagespeed::DomElement& node,
                             const pagespeed::DomDocument& document);

 private:
  const pagespeed::PagespeedInput *const input_;
  std::map<const pagespeed::Resource*, std::vector<pagespeed::DomRect> >*
      resource_to_rect_map_;

  // Offset of the current document in the root document's coordinate
  // space.
  int x_translate_;
  int y_translate_;
};

// Traverses the DOM to find all references to images. For each image,
// determines if it is on or offscreen, using the viewport width and
// height specified in the PagespeedInput. If an image is referenced
// multiple times and appears both on and offscreen, it is inserted into
// the onscreen resource vector only.
bool FindOnAndOffscreenImageResources(
    const PagespeedInput& input,
    std::vector<const pagespeed::Resource*>* out_onscreen_resources,
    std::vector<const pagespeed::Resource*>* out_offscreen_resources);

}  // namespace dom
}  // namespace pagespeed

#endif  // PAGESPEED_DOM_RESOURCE_COORDINATE_FINDER_H_
