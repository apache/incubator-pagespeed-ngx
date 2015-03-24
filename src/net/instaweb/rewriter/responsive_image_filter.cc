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

#include <memory>
#include <utility>                      // for pair

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/data_url.h"

namespace net_instaweb {

ResponsiveImageFirstFilter::ResponsiveImageFirstFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
}

ResponsiveImageFirstFilter::~ResponsiveImageFirstFilter() {
}

void ResponsiveImageFirstFilter::StartDocumentImpl() {
  candidate_map_.clear();
}

void ResponsiveImageFirstFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg) {
    return;
  }

  if (element->FindAttribute(HtmlName::kPagespeedNoTransform) != NULL) {
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because of "
        "pagespeed_no_transform attribute.", element);
  } else if (element->FindAttribute(HtmlName::kSrcset) != NULL) {
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because image already "
        "has one.", element);
  } else if (element->FindAttribute(HtmlName::kDataPagespeedResponsiveTemp) ==
             NULL) {
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
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because image does not "
        "have dimensions (or a src URL).", element);
    return;
  }

  int orig_width, orig_height;
  if (StringToInt(width_str, &orig_width) &&
      StringToInt(height_str, &orig_height)) {
    if (orig_width <= 1 || orig_height <= 1) {
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset to tracking pixel.",
          element);
      return;
    }

    // TODO(sligocki): Figure out what levels we should actually be using.
    // For example, many android phones use 1.5x.
    // TODO(sligocki): Possibly use lower quality settings for 2x and 4x
    // because standard quality-85 are overkill for high density displays.
    // However, we might want high quality for zoom.
    // Note: These must be listed in ascending order.
    ResponsiveImageCandidateVector candidate_list;
    candidate_list.push_back(
        AddHiResVersion(element, *src_attr, orig_width, orig_height, 2));
    candidate_list.push_back(
        AddHiResVersion(element, *src_attr, orig_width, orig_height, 4));
    candidate_map_[element] = candidate_list;
    // Mark this element as responsive as well, so that ImageRewriteFilter will
    // add actual final dimensions to the tag.
    driver()->AddAttribute(element, HtmlName::kDataPagespeedResponsiveTemp,
                           StringPiece(NULL));
  }
}

ResponsiveImageCandidate ResponsiveImageFirstFilter::AddHiResVersion(
    HtmlElement* img, const HtmlElement::Attribute& src_attr,
    int orig_width, int orig_height, double resolution) {
  HtmlElement* new_img = driver()->NewElement(img->parent(), HtmlName::kImg);
  new_img->AddAttribute(src_attr);
  driver()->AddAttribute(new_img, HtmlName::kDataPagespeedResponsiveTemp,
                         StringPiece(NULL));
  // Note: We truncate width and height to integers here.
  driver()->AddAttribute(new_img, HtmlName::kWidth,
                         IntegerToString(orig_width * resolution));
  driver()->AddAttribute(new_img, HtmlName::kHeight,
                         IntegerToString(orig_height * resolution));
  driver()->InsertNodeBeforeNode(img, new_img);
  ResponsiveImageCandidate candidate(new_img, resolution);
  return candidate;
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

  ResponsiveImageCandidateMap::const_iterator p =
      first_filter_->candidate_map_.find(element);
  if (p != first_filter_->candidate_map_.end()) {
    // On second run of the filter, combine the elements back together.
    const ResponsiveImageCandidateVector& candidates = p->second;
    CombineHiResImages(element, candidates);
    Cleanup(element, candidates);
  }
}

namespace {

// Get actual dimensions. These are inserted by ImageRewriteFilter as
// attributes on all images involved in the responsive flow.
ImageDim ActualDims(const HtmlElement* element) {
  ImageDim dims;

  int height;
  const char* height_str = element->AttributeValue(HtmlName::kDataActualHeight);
  if (height_str != NULL && StringToInt(height_str, &height)) {
    dims.set_height(height);
  }

  int width;
  const char* width_str = element->AttributeValue(HtmlName::kDataActualWidth);
  if (width_str != NULL && StringToInt(width_str, &width)) {
    dims.set_width(width);
  }

  return dims;
}

}  // namespace

// Combines information from dummy 2x and 4x images into the 1x srcset.
// Deletes the dummy images.
void ResponsiveImageSecondFilter::CombineHiResImages(
    HtmlElement* orig_element,
    const ResponsiveImageCandidateVector& candidates) {
  const char* x1_src = orig_element->AttributeValue(HtmlName::kSrc);

  if (x1_src == NULL) {
    // Should not happen, we explicitly checked that <img> had a src= attribute
    // in ResponsiveImageFirstFilter::AddHiResImages().
    LOG(DFATAL) << "Original responsive image has no URL.";
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because original image has "
        "no src URL.", orig_element);
    return;
  } else if (IsDataUrl(x1_src)) {
    // In case there are any data URLs, we should not add a srcset (which
    // would include many copies of the data URL).
    // TODO(sligocki): Should we change the src to the 4x version so that
    // the image still looks good up to 4x resolution?
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because original image was "
        "inlined.", orig_element);
    return;
  }

  GoogleString srcset_value = StrCat(x1_src, " 1x");

  // Keep track of last candidate's URL. If next candidate has same URL,
  // don't include it in the srcset.
  StringPiece last_src = x1_src;
  // Keep track of actual final dimensions of last candidate. If next candidate
  // has same actual dimensions, we don't include it in the srcset.
  ImageDim last_dims = ActualDims(orig_element);
  bool added_hi_res = false;

  for (int i = 0, n = candidates.size(); i < n; ++i) {
    const char* src = candidates[i].element->AttributeValue(HtmlName::kSrc);

    if (src == NULL) {
      // Should not happen, we explicitly created a src= attribute in
      // ResponsiveImageFirstFilter::AddHiResVersion().
      LOG(DFATAL) << "Virtual responsive image has no URL.";
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset because virtual image has "
          "no src URL.", orig_element);
      return;
    } else if (IsDataUrl(src)) {
      // In case there are any data URLs, we should not add a srcset (which
      // would include many copies of the data URL).
      // TODO(sligocki): Should we change the src to the 4x version so that
      // the image still looks good up to 4x resolution?
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset because virtual image was "
          "inlined.", orig_element);
      return;
    }

    ImageDim dims = ActualDims(candidates[i].element);
    if (src == last_src) {
      if (driver()->DebugMode()) {
        driver()->InsertDebugComment(StringPrintf(
            "ResponsiveImageFilter: Not adding %.16gx candidate to srcset "
            "because it is the same as previous candidate.",
            candidates[i].resolution),
                                     orig_element);
      }
    } else if (dims.height() == last_dims.height() &&
               dims.width() == last_dims.width()) {
      if (driver()->DebugMode()) {
        driver()->InsertDebugComment(StringPrintf(
            "ResponsiveImageFilter: Not adding %.16gx candidate to srcset "
            "because native image was not high enough resolution.",
            candidates[i].resolution),
                                     orig_element);
      }
    } else {
      GoogleString resolution_string =
          StringPrintf("%.16g", candidates[i].resolution);
      // TODO(sligocki): Escape URLs appropriately? For example, we may need
      // to escape commas. Which are used in both Data URLs and Pagespeed
      // rewritten URLs as escape characters.
      StrAppend(&srcset_value, ",", src, " ", resolution_string, "x");

      last_src = src;
      last_dims = dims;
      added_hi_res = true;
    }
  }

  if (added_hi_res) {
    driver()->AddAttribute(orig_element, HtmlName::kSrcset, srcset_value);
    srcsets_added_ = true;
  }
}

void ResponsiveImageSecondFilter::Cleanup(
    HtmlElement* orig_element,
    const ResponsiveImageCandidateVector& candidates) {
  for (int i = 0, n = candidates.size(); i < n; ++i) {
    driver()->DeleteNode(candidates[i].element);
  }

  orig_element->DeleteAttribute(HtmlName::kDataPagespeedResponsiveTemp);
  orig_element->DeleteAttribute(HtmlName::kDataActualHeight);
  orig_element->DeleteAttribute(HtmlName::kDataActualWidth);
}

void ResponsiveImageSecondFilter::EndDocument() {
  if (zoom_filter_enabled_ && srcsets_added_) {
    HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kSrc, responsive_js_url_);
    InsertNodeAtBodyEnd(script);
  }
}

}  // namespace net_instaweb
