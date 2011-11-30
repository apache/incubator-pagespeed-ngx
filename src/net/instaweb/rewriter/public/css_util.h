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

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

class UnicodeText;

namespace Css {
class Declarations;
}
namespace net_instaweb {
class HtmlElement;

// TODO(nforman): remove this namespace and put everything into the
// StyleExtractor class.
namespace css_util {

static const char kAllMedia[] = "all";
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

// Utility functions for handling CSS media types as vectors of strings.
// There is an argument to use StringPiece's rather than GoogleString's here,
// but CssFilter::FlattenImportsContext cannot use StringPiece's because it
// doesn't keep the original strings, so copies in GoogleString's are required.

// Convert a media string, from either a media attribute or after @import, to
// a vector of media types. If any of the input media types are 'all' then an
// empty vector is returned: 'all' means all media types are accepted so it
// subsumes all other types, and an empty vector representation is most useful.
void VectorizeMediaAttribute(const StringPiece& input_media,
                             StringVector* output_vector);

// Convert a vector of media types to a media string. If the input vector is
// empty then the answer is 'all', the inverse of the vectorizing function
// above; if you want the empty string then test the vector yourself. Otherwise
// the answer is a comma-separated list of media types.
GoogleString StringifyMediaVector(const StringVector& import_media);

// Convert a vector of UnicodeText's (from Css::Import.media) to a vector of
// UTF-8 GoogleString's for use of the above functions. Elements are trimmed
// and any empty elements are ignored.
void ConvertUnicodeVectorToStringVector(
    const std::vector<UnicodeText>& in_vector,
    StringVector* out_vector);

// Convert a vector of UTF-8 GoogleString's to UnicodeText's. Elements are
// trimmed and any empty elements are ignored.
void ConvertStringVectorToUnicodeVector(
    const StringVector& in_vector,
    std::vector<UnicodeText>* out_vector);

// Clear the given vector if it contains the media 'all'. This is required
// because Css::Parser doesn't treat 'all' specially but we do for efficiency.
void ClearVectorIfContainsMediaAll(StringVector* media);

// Eliminate all elements from the first vector that are not in the second
// vector, with the caveat that an empty vector (first or second) means 'the
// set of all possible values', meaning that if the second vector is empty
// then no elements are removed from the first vector, and if the first vector
// is empty then the second vector is copied into it. Both vectors must be
// sorted on entry.
template<typename T>
void EliminateElementsNotIn(std::vector<T>* sorted_inner,
                            const std::vector<T>& sorted_outer) {
  if (!sorted_outer.empty()) {
    if (sorted_inner->empty()) {
      *sorted_inner = sorted_outer;
    } else {
      typename std::vector<T>::const_iterator outer_iter = sorted_outer.begin();
      typename std::vector<T>::iterator inner_iter = sorted_inner->begin();

      while (inner_iter != sorted_inner->end()) {
        if (outer_iter == sorted_outer.end()) {
          // No more outer elements => delete all remaining inner elements.
          inner_iter = sorted_inner->erase(inner_iter, sorted_inner->end());
        } else if (*outer_iter == *inner_iter) {
          // This inner element is in the outer => keep it and move on.
          ++outer_iter;
          ++inner_iter;
        } else if (*outer_iter < *inner_iter) {
          // This outer element isn't in the inner => skip it, try the next.
          ++outer_iter;
        } else {
          // This inner element isn't in the outer => delete it, move on.
          inner_iter = sorted_inner->erase(inner_iter);
        }
      }
    }
  }
}

}  // css_util

}  // net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_UTIL_H_
