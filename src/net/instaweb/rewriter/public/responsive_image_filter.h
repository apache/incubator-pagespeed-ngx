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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESPONSIVE_IMAGE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESPONSIVE_IMAGE_FILTER_H_

#include <map>
#include <utility>                      // for pair

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

// Filter which converts <img> tags into responsive srcset= counterparts by
// rewriting the images at multiple resolutions.
// The filter actually runs twice. Once before rewrite_images to split an <img>
// element into multiple elements (one for each resolution). The second time,
// it runs after rewrite_images have been rendered and it combines the elements
// back together into a single <img> element with srcset=.
// It also adds JavaScript to polyfill and add zoom responsiveness to srcset.
class ResponsiveImageFirstFilter : public CommonFilter {
 public:
  explicit ResponsiveImageFirstFilter(RewriteDriver* driver);
  virtual ~ResponsiveImageFirstFilter();

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void StartDocumentImpl();
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "ResponsiveImageFirst"; }

  static const char kPagespeedResponsiveTemp[];

 private:
  void AddHiResImages(HtmlElement* element);
  HtmlElement* AddHiResVersion(HtmlElement* img,
                               const HtmlElement::Attribute& src_attr,
                               int width, int height);


  typedef std::pair<HtmlElement*, HtmlElement*> ElementPair;
  typedef std::map<HtmlElement*, ElementPair> ElementMap;
  friend class ResponsiveImageSecondFilter;

  ElementMap element_map_;

  DISALLOW_COPY_AND_ASSIGN(ResponsiveImageFirstFilter);
};

class ResponsiveImageSecondFilter : public CommonFilter {
 public:
  explicit ResponsiveImageSecondFilter(
      RewriteDriver* driver, const ResponsiveImageFirstFilter* first_filter);
  virtual ~ResponsiveImageSecondFilter();

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void StartDocumentImpl();
  virtual void EndElementImpl(HtmlElement* element);
  virtual void EndDocument();

  virtual const char* Name() const { return "ResponsiveImageSecond"; }

 private:
  void CombineHiResImages(HtmlElement* x1, HtmlElement* x2, HtmlElement* x4);

  const GoogleString responsive_js_url_;
  const ResponsiveImageFirstFilter* first_filter_;
  // Is the ResponsiveImagesZoom filter enabled?
  bool zoom_filter_enabled_;
  // Was at least one srcset added? If not we don't insert zoom script.
  bool srcsets_added_;

  DISALLOW_COPY_AND_ASSIGN(ResponsiveImageSecondFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESPONSIVE_IMAGE_FILTER_H_
