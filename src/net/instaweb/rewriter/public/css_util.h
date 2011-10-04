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
//
// Functionality for parsing css declarations.
// Currently this file deals with dimensions only, but could
// be explanded to include other types of values.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_UTIL_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"

namespace Css {
class Declarations;
}
namespace net_instaweb {
class HtmlElement;

// TODO(nforman): remove this namespace and put everything into the
// StyleExtractor class.
namespace css_util {

static const int kNoValue = -1;

enum DimensionState {
  kNoDimensions, // No dimensions found.
  kHasHeightOnly, // Found height only.
  kHasWidthOnly, // Found width only.
  kHasBothDimensions, // Found both width and height.
  kNotParsable // Found a dimension, but couldn't extract a value.
};

// Extract the width and height values out of a list of declarations.
// If a value was not found, it will be populated with kNoValue.
// This is "safe" because even if someone specifies a width:-1; it will be
// ignored:
// "If a negative length value is set on a property that does not allow
// negative length values, the declaration is ignored."
// http://www.w3.org/TR/CSS2/syndata.html#value-def-length
DimensionState GetDimensions(Css::Declarations* decls, int* width, int* height);

class StyleExtractor {
 public:
  StyleExtractor(HtmlElement* element);
  virtual ~StyleExtractor();


  DimensionState state() const { return state_; }

  // If a value was not found, it will be populated with kNoValue.
  int width() const { return width_px_; }
  int height() const { return height_px_; }

  // Returns true if there is any dimension specified in a style attribute,
  // whether or not they're parsable.
  bool HasAnyDimensions() { return (state_ != kNoDimensions); }

  DimensionState dimension_state() { return state_; }

 private:
  static Css::Declarations* GetDeclsFromElement(HtmlElement* element);
  scoped_ptr<Css::Declarations> decls_;
  int width_px_;
  int height_px_;
  DimensionState state_;
  DISALLOW_COPY_AND_ASSIGN(StyleExtractor);
};

}  // css_util

}  // net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_UTIL_H_
