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

// Author: nforman@google.com (Naomi Forman)

#include <cstddef>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/value.h"

namespace net_instaweb {

namespace css_util {

// Extract the numerical value from a values vector.
// TODO(nforman): Allow specification what what style of numbers we can handle.
int GetValueDimension(const Css::Values* values) {
  for (Css::Values::const_iterator value_iter = values->begin();
       value_iter != values->end(); ++value_iter) {
    Css::Value* value = *value_iter;
    if ((value->GetLexicalUnitType() == Css::Value::NUMBER)
        && (value->GetDimension() == Css::Value::PX)) {
      return value->GetIntegerValue();
    }
  }
  return kNoValue;
}

DimensionState GetDimensions(Css::Declarations* decls,
                             int* width, int* height) {
  bool has_width = false;
  bool has_height = false;
  *width = kNoValue;
  *height = kNoValue;
  for (Css::Declarations::iterator decl_iter = decls->begin();
       decl_iter != decls->end() && (!has_width || !has_height); ++decl_iter) {
    Css::Declaration* decl = *decl_iter;
    switch (decl->prop()) {
      case Css::Property::WIDTH: {
        *width = GetValueDimension(decl->values());
        has_width = true;
        break;
      }
      case Css::Property::HEIGHT: {
        *height = GetValueDimension(decl->values());
        has_height = true;
        break;
      }
      default:
        break;
    }
  }
  if (has_width && has_height && *width != kNoValue && *height != kNoValue) {
    return kHasBothDimensions;
  } else if ((has_width && *width == kNoValue) ||
             (has_height && *height == kNoValue)) {
    return kNotParsable;
  } else if (has_width) {
    return kHasWidthOnly;
  } else if (has_height) {
    return kHasHeightOnly;
  }
  return kNoDimensions;
}

StyleExtractor::StyleExtractor(HtmlElement* element)
    : decls_(GetDeclsFromElement(element)),
      width_px_(kNoValue),
      height_px_(kNoValue) {
  if (decls_.get() != NULL) {
    state_ = GetDimensions(decls_.get(), &width_px_, &height_px_);
  } else {
    state_ = kNoDimensions;
  }
}

StyleExtractor::~StyleExtractor() {}

// Return a Declarations* from the style attribute of an element.  If
// there is no style, return NULL.
Css::Declarations* StyleExtractor::GetDeclsFromElement(HtmlElement* element) {
  HtmlElement::Attribute* style = element->FindAttribute(HtmlName::kStyle);
  if (style != NULL) {
    Css::Parser parser(style->value());
    return parser.ParseDeclarations();
  }
  return NULL;
}

}  // css_util

}  // net_instaweb
