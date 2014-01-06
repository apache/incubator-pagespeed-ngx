/*
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class DomainRewriteFilter;
class GoogleUrl;
class HtmlParse;
class MessageHandler;
class RewriteDriver;
class UrlLeftTrimFilter;
class Writer;

class CssTagScanner {
 public:
  // Helper class for TransformUrls to allow any URL transformation to
  // be applied to a CSS file.
  class Transformer {
   public:
    virtual ~Transformer();

    enum TransformStatus { kSuccess, kNoChange, kFailure };
    // Transforms str in-place.
    // If kSuccess -> transformation succeeded and str may have changed
    // (Generally implementers should only return kSuccess if str changed, but
    // this is merely an optimization. Functionally it doesn't matter).
    // If kNoChange -> transformation succeeded and str was unchanged.
    // If kFailure -> transformation failed. str is undefined, do not use.
    virtual TransformStatus Transform(GoogleString* str) = 0;
  };

  static const char kStylesheet[];
  static const char kAlternate[];
  static const char kUriValue[];

  explicit CssTagScanner(HtmlParse* html_parse);

  // Examines an HTML element to determine if it's a CSS link, extracting out
  // the href, the media type (if any) and the number of nonstandard attributes
  // found.  If it's not CSS, href is set to NULL, media is set to "", and
  // num_nonstandard_attributes is set to 0.
  bool ParseCssElement(HtmlElement* element,
                       HtmlElement::Attribute** href,
                       const char** media,
                       int* num_nonstandard_attributes);

  // Many callers don't care about num_nonstandard_attributes, so we provide
  // a version that discards that information.
  bool ParseCssElement(HtmlElement* element,
                       HtmlElement::Attribute** href,
                       const char** media) {
    int num_nonstandard_attributes;
    return ParseCssElement(element, href, media, &num_nonstandard_attributes);
  }

  // Scans the contents of a CSS file, looking for the pattern url(xxx).
  // Performs an arbitrary mutation on all such URLs.
  // If xxx is quoted with single-quotes or double-quotes, those are
  // retained and the URL inside is transformed.
  static bool TransformUrls(
      const StringPiece& contents, Writer* writer, Transformer* transformer,
      MessageHandler* handler);

  // Does this CSS file contain @import? If so, it cannot be combined with
  // previous CSS files. This may give false-positives, but no false-negatives.
  static bool HasImport(const StringPiece& contents, MessageHandler* handler);

  // Detemines whether this CSS contains a URI value (aka URL).
  static bool HasUrl(const StringPiece& contents);

  // Does this attribute value represent a stylesheet or alternate stylesheet?
  // Should be called with element->AttributeValue(HtmlName::kRel) as the arg.
  static bool IsStylesheetOrAlternate(const StringPiece& attribute_value);

  // Does this rel attribute value represent an alternate stylesheet?
  static bool IsAlternateStylesheet(const StringPiece& attribute_value);

 private:
  DISALLOW_COPY_AND_ASSIGN(CssTagScanner);
};

// Transform URLs by:
//   1 Resolving them against old_base_url,
//   2 Mapping them appropriately with domain_rewrite_filter and then
//   3 Trimming them against new_base_url.
class RewriteDomainTransformer : public CssTagScanner::Transformer {
 public:
  RewriteDomainTransformer(const GoogleUrl* old_base_url,
                           const GoogleUrl* new_base_url,
                           RewriteDriver* driver);
  virtual ~RewriteDomainTransformer();

  virtual TransformStatus Transform(GoogleString* str);

  void set_trim_urls(bool x) { trim_urls_ = x; }

 private:
  const GoogleUrl* old_base_url_;
  const GoogleUrl* new_base_url_;

  DomainRewriteFilter* domain_rewriter_;
  UrlLeftTrimFilter* url_trim_filter_;
  MessageHandler* handler_;
  bool trim_urls_;
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDomainTransformer);
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_
