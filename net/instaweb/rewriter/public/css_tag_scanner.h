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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;
class ServerContext;
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

  // An instance of CssTagScanner should be created to use
  // TransformUrlsStreaming; other APIs do not need this.
  // *transformer will be used to to transform URLs in CSS, and *handler for
  // logging.
  CssTagScanner(Transformer* transformer, MessageHandler* handler);

  // Examines an HTML element to determine if it's a CSS link, extracting out
  // the href, the media type (if any) and any nonstandard attributes.  If it's
  // not CSS, href is set to NULL, media is set to "", and no nonstandard
  // attributes are identified.  NULL may be passed for nonstandard_attributes
  // to indicate the caller doesn't need them collected.
  static bool ParseCssElement(HtmlElement* element,
                              HtmlElement::Attribute** href,
                              const char** media,
                              StringPieceVector* nonstandard_attributes);

  // Many callers don't care about nonstandard attributes, so we provide a
  // version that discards that information.
  static bool ParseCssElement(HtmlElement* element,
                              HtmlElement::Attribute** href,
                              const char** media) {
    return ParseCssElement(element, href, media,
                           NULL /* nonstandard attributes */);
  }

  // When parsing streaming input, we need to be told if the given input
  // portion goes up to end-of-file (since it affects whether something
  // may continue the last token of the input).
  enum InputPortion {
    kInputIncludesEnd,
    kInputDoesNotIncludeEnd
  };

  // Scans the contents of a CSS file, looking for the pattern url(xxx).
  // Performs an arbitrary mutation on all such URLs.
  // If xxx is quoted with single-quotes or double-quotes, those are
  // retained and the URL inside is transformed.
  static bool TransformUrls(
      StringPiece contents, Writer* writer, Transformer* transformer,
      MessageHandler* handler);

  // Like above, but handles incomplete input. In such a case, all chunks
  // other than the last one should be served with input_portion ==
  // kInputDoesNotIncludeEnd. Note that this method store some state for
  // reparsing, so you can't concurrently run two streams through the same
  // CssTagScanner object.
  bool TransformUrlsStreaming(
      StringPiece contents, InputPortion input_portion, Writer* writer);

  // Returns what was retained by TransformUrlsStreaming for reparsing.
  // Meant for use in tests.
  GoogleString RetainedForReparse() const { return reparse_; }

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
  enum UrlKind {
    kNone,
    kImport,
    kUrl
  };

  void SerializeUrlUse(UrlKind kind, const GoogleString& url,
                       bool is_quoted, bool have_term_quote, char quote,
                       bool have_term_paren,
                       Writer* writer, bool* ok);

  Transformer* transformer_;
  MessageHandler* handler_;
  GoogleString reparse_;

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
                           const ServerContext* server_context,
                           const RewriteOptions* options,
                           MessageHandler* handler);
  virtual ~RewriteDomainTransformer();

  virtual TransformStatus Transform(GoogleString* str);

  void set_trim_urls(bool x) { trim_urls_ = x; }

 private:
  const GoogleUrl* old_base_url_;
  const GoogleUrl* new_base_url_;

  const ServerContext* server_context_;
  const RewriteOptions* options_;
  MessageHandler* handler_;

  bool trim_urls_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDomainTransformer);
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_
