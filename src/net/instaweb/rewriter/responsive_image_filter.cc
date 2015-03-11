/*
 * Copyright 2014 Google Inc.
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
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/responsive_image_filter.h"

#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/data_url.h"

namespace net_instaweb {

const char ResponsiveImageFirstFilter::kPagespeedResponsiveTemp[] =
    "pagespeed_responsive_temp";

ResponsiveImageFirstFilter::ResponsiveImageFirstFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
}

ResponsiveImageFirstFilter::~ResponsiveImageFirstFilter() {
}

void ResponsiveImageFirstFilter::StartDocumentImpl() {
  element_map_.clear();
}

void ResponsiveImageFirstFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg) {
    return;
  }

  if (element->FindAttribute(kPagespeedResponsiveTemp) == NULL &&
      element->FindAttribute(HtmlName::kPagespeedNoTransform) == NULL &&
      element->FindAttribute(HtmlName::kSrcset) == NULL) {
    // On first run of this filter, split <img> element into multiple
    // elements.
    AddHiResImages(element);
  }
}

// Adds dummy images for 2x and 4x resolutions. Note: this converts:
//   <img src=foo.jpg width=w height=h>
// into:
//   <img src=foo.jpg width=2w height=2h pagespeed_responsive_temp>
//   <img src=foo.jpg width=4w height=4h pagespeed_responsive_temp>
//   <img src=foo.jpg width=w height=h>
// The order of these images doesn't really matter, but adding them before
// this image avoids some extra processing of the added dummy images by
// ResponsiveImageFirstFilter.
void ResponsiveImageFirstFilter::AddHiResImages(HtmlElement* element) {
  const HtmlElement::Attribute* src_attr =
    element->FindAttribute(HtmlName::kSrc);
  // TODO(sligocki): width and height attributes can lie. Perhaps we should
  // look at rendered image dimensions (via beaconing back from clients).
  const char* width_str = element->AttributeValue(HtmlName::kWidth);
  const char* height_str = element->AttributeValue(HtmlName::kHeight);
  if ((src_attr == NULL) || (width_str == NULL) || (height_str == NULL)) {
    return;
  }

  int width, height;
  if (StringToInt(width_str, &width) && StringToInt(height_str, &height)) {
    if (width <= 1 || height <= 1) {
      // Do not mess with tracking pixels.
      return;
    }

    // TODO(sligocki): Figure out what levels we should actually be using.
    // TODO(sligocki): Possibly use lower quality settings for 2x and 4x
    // because standard quality-85 are overkill for high density displays.
    // However, we might want high quality for zoom.
    // TODO(sligocki): Do not include images at higher than full resolution.
    HtmlElement* x2 = AddHiResVersion(element, *src_attr,
                                      2 * width, 2 * height);
    HtmlElement* x4 = AddHiResVersion(element, *src_attr,
                                      4 * width, 4 * height);
    element_map_[element] = ElementPair(x2, x4);
  }
}

HtmlElement* ResponsiveImageFirstFilter::AddHiResVersion(
    HtmlElement* img, const HtmlElement::Attribute& src_attr,
    int width, int height) {
  HtmlElement* new_img = driver()->NewElement(img->parent(), HtmlName::kImg);
  new_img->AddAttribute(src_attr);
  driver()->AddAttribute(new_img, kPagespeedResponsiveTemp, NULL);
  driver()->AddAttribute(new_img, HtmlName::kWidth, IntegerToString(width));
  driver()->AddAttribute(new_img, HtmlName::kHeight, IntegerToString(height));
  driver()->InsertNodeBeforeNode(img, new_img);
  return new_img;
}

ResponsiveImageSecondFilter::ResponsiveImageSecondFilter(
    RewriteDriver* driver, const ResponsiveImageFirstFilter* first_filter)
  : CommonFilter(driver),
    responsive_js_url_(
        driver->server_context()->static_asset_manager()->GetAssetUrl(
            StaticAssetEnum::RESPONSIVE_JS, driver->options())),
    first_filter_(first_filter),
    zoom_filter_enabled_(driver->options()->Enabled(
        RewriteOptions::kResponsiveImagesZoom)),
    srcsets_added_(false) {
}

ResponsiveImageSecondFilter::~ResponsiveImageSecondFilter() {
}

void ResponsiveImageSecondFilter::StartDocumentImpl() {
  srcsets_added_ = false;
}

void ResponsiveImageSecondFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg) {
    return;
  }

  ResponsiveImageFirstFilter::ElementMap::const_iterator p =
      first_filter_->element_map_.find(element);
  if (p != first_filter_->element_map_.end()) {
    // On second run of the filter, combine the elements back together.
    CombineHiResImages(element, p->second.first, p->second.second);
  }
}

// Combines information from dummy 2x and 4x images into the 1x srcset.
// Deletes the dummy images.
void ResponsiveImageSecondFilter::CombineHiResImages(
    HtmlElement* x1, HtmlElement* x2, HtmlElement* x4) {
  const char* x1_src = x1->AttributeValue(HtmlName::kSrc);
  const char* x2_src = x2->AttributeValue(HtmlName::kSrc);
  const char* x4_src = x4->AttributeValue(HtmlName::kSrc);
  DCHECK((x1_src != NULL) && (x2_src != NULL) && (x4_src != NULL));
  if ((x1_src != NULL) && (x2_src != NULL) && (x4_src != NULL)) {
    if (IsDataUrl(x1_src) || IsDataUrl(x2_src) || IsDataUrl(x4_src)) {
      // In case there are any data URLs, we should not add a srcset (which
      // would include many copies of the data URL).
      // TODO(sligocki): Should we change the src to the 4x version so that
      // the image still looks good up to 4x resolution?
    } else {
      // TODO(sligocki): Escape URLs appropriately? For example, we may need
      // to escape commas.
      // TODO(sligocki): Don't include different URLs if they point to the same
      // actual image. For example, if all hashes are the same, no point in
      // having a srcset at all.
      GoogleString srcset_value = StrCat(
          x1_src, " 1x,", x2_src, " 2x,", x4_src, " 4x");
      driver()->AddAttribute(x1, HtmlName::kSrcset, srcset_value);
      srcsets_added_ = true;
    }
  }
  driver()->DeleteNode(x2);
  driver()->DeleteNode(x4);
}

void ResponsiveImageSecondFilter::EndDocument() {
  if (zoom_filter_enabled_ && srcsets_added_) {
    HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kSrc, responsive_js_url_);
    InsertNodeAtBodyEnd(script);
  }
}

}  // namespace net_instaweb
