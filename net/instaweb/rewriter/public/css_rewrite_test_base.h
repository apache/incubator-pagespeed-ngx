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

// Author: sligocki@google.com (Shawn Ligocki)

// Base class for tests which do rewrites within CSS.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_REWRITE_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_REWRITE_TEST_BASE_H_

#include "base/logging.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

class ResourceNamer;

class CssRewriteTestBase : public RewriteTestBase {
 protected:
  CssRewriteTestBase() {
    num_blocks_rewritten_ =
        statistics()->GetVariable(CssFilter::kBlocksRewritten);
    num_fallback_rewrites_ =
        statistics()->GetVariable(CssFilter::kFallbackRewrites);
    num_parse_failures_ = statistics()->GetVariable(CssFilter::kParseFailures);
    num_rewrites_dropped_ =
        statistics()->GetVariable(CssFilter::kRewritesDropped);
    total_bytes_saved_ = statistics()->GetUpDownCounter(
        CssFilter::kTotalBytesSaved);
    total_original_bytes_ =
        statistics()->GetVariable(CssFilter::kTotalOriginalBytes);
    num_uses_ = statistics()->GetVariable(CssFilter::kUses);
    num_flatten_imports_charset_mismatch_ =
        statistics()->GetVariable(CssFilter::kCharsetMismatch);
    num_flatten_imports_invalid_url_ =
        statistics()->GetVariable(CssFilter::kInvalidUrl);
    num_flatten_imports_limit_exceeded_ =
        statistics()->GetVariable(CssFilter::kLimitExceeded);
    num_flatten_imports_minify_failed_ =
        statistics()->GetVariable(CssFilter::kMinifyFailed);
    num_flatten_imports_recursion_ =
        statistics()->GetVariable(CssFilter::kRecursion);
    num_flatten_imports_complex_queries_ =
        statistics()->GetVariable(CssFilter::kComplexQueries);
  }
  ~CssRewriteTestBase();

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->set_always_rewrite_css(true);
    AddFilter(RewriteOptions::kRewriteCss);
  }

  enum ValidationFlags {
    kNoFlags = 0,

    kExpectSuccess = 1<<0,   // CSS parser succeeds and URL should be rewritten.
    kExpectCached = 1<<1,    // CSS parser succeeds and URL should be rewritten,
                             // but everything is from the cache so zero stats.
    kExpectNoChange = 1<<2,  // CSS parser succeeds but the URL is not rewritten
                             // because we increased the size of contents.
    kExpectFallback = 1<<3,  // CSS parser fails, fallback succeeds.
    kExpectFailure = 1<<4,   // CSS parser fails, fallback failed or disabled.

    // TODO(sligocki): Explain why we turn off stats check at each use-site.
    kNoStatCheck = 1<<5,
    // TODO(sligocki): Why would we ever want to clear fetcher?
    kNoClearFetcher = 1<<6,
    // TODO(sligocki): Explain why we turn off other contexts.
    kNoOtherContexts = 1<<7,

    kLinkCharsetIsUTF8 = 1<<8,
    kLinkScreenMedia = 1<<9,
    kLinkPrintMedia = 1<<10,

    kMetaCharsetUTF8 = 1<<11,
    kMetaCharsetISO88591 = 1<<12,
    kMetaHttpEquiv = 1<<13,
    kMetaHttpEquivUnquoted = 1<<14,

    // Flags to the check various import flattening failure statistics.
    kFlattenImportsCharsetMismatch = 1<<15,
    kFlattenImportsInvalidUrl = 1<<16,
    kFlattenImportsLimitExceeded = 1<<17,
    kFlattenImportsMinifyFailed = 1<<18,
    kFlattenImportsRecursion = 1<<19,
    kFlattenImportsComplexQueries = 1<<20,

    // Flags to allow methods to know if the HTML is the test input or output.
    kInputHtml = 1<<21,
    kOutputHtml = 1<<22,
  };

  static bool ExactlyOneTrue(bool a, bool b) {
    return a ^ b;
  }
  static bool ExactlyOneTrue(bool a, bool b, bool c) {
    return ExactlyOneTrue(a, ExactlyOneTrue(b, c));
  }
  static bool ExactlyOneTrue(bool a, bool b, bool c, bool d) {
    return ExactlyOneTrue(a, ExactlyOneTrue(b, c, d));
  }
  static bool ExactlyOneTrue(bool a, bool b, bool c, bool d, bool e) {
    return ExactlyOneTrue(a, ExactlyOneTrue(b, c, d, e));
  }

  bool FlagSet(int flags, ValidationFlags f) const {
    return (flags & f) != 0;
  }

  // Sanity check on flags passed in.
  void CheckFlags(int flags) {
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectSuccess),
                         FlagSet(flags, kExpectCached),
                         FlagSet(flags, kExpectNoChange),
                         FlagSet(flags, kExpectFallback),
                         FlagSet(flags, kExpectFailure)));
  }

  // Check that inline CSS gets rewritten correctly.
  bool ValidateRewriteInlineCss(StringPiece id,
                                StringPiece css_input,
                                StringPiece expected_css_output,
                                int flags);

  void GetNamerForCss(StringPiece id,
                      StringPiece expected_css_output,
                      ResourceNamer* namer);

  GoogleString ExpectedUrlForNamer(const ResourceNamer& namer);

  GoogleString ExpectedUrlForCss(StringPiece id,
                                 StringPiece expected_css_output);

  // Check that external CSS gets rewritten correctly.
  void ValidateRewriteExternalCss(StringPiece id,
                                  StringPiece css_input,
                                  StringPiece expected_css_output,
                                  int flags) {
    ValidateRewriteExternalCssUrl(id, StrCat(kTestDomain, id, ".css"),
                                  css_input, expected_css_output, flags);
  }

  void ValidateRewriteExternalCssUrl(StringPiece id,
                                     StringPiece css_url,
                                     StringPiece css_input,
                                     StringPiece expected_css_output,
                                     int flags);

  // Makes an HTML document with an external CSS link.
  GoogleString MakeHtmlWithExternalCssLink(StringPiece css_url, int flags,
                                           bool insert_debug_message);

  // Makes a CSS body with an external image link, with nice indentation.
  GoogleString MakeIndentedCssWithImage(StringPiece image_url);

  // Makes a minified CSS body with an external image link.
  GoogleString MakeMinifiedCssWithImage(StringPiece image_url);

  // Extract the background image from the css text
  GoogleString ExtractCssBackgroundImage(StringPiece in_css);

  void ValidateRewrite(StringPiece id,
                       StringPiece css_input,
                       StringPiece gold_output,
                       int flags) {
    if (ValidateRewriteInlineCss(StrCat(id, "-inline"),
                                 css_input, gold_output, flags)) {
      // Don't run for external CSS unless inline succeeds.
      ValidateRewriteExternalCss(StrCat(id, "-external"),
                                 css_input, gold_output, flags);
    }
  }

  void ValidateFailParse(StringPiece id, StringPiece css_input) {
    ValidateRewrite(id, css_input, css_input, kExpectFailure);
  }

  // Reset all Variables.
  void ResetStats();

  // Validate HTML rewrite as well as checking statistics.
  bool ValidateWithStats(
      StringPiece id,
      StringPiece html_input, StringPiece expected_html_output,
      StringPiece css_input, StringPiece expected_css_output,
      int flags);

  // Helper to test for how we handle trailing junk
  void TestCorruptUrl(const char* junk);

  Variable* num_blocks_rewritten_;
  Variable* num_fallback_rewrites_;
  Variable* num_parse_failures_;
  Variable* num_rewrites_dropped_;
  UpDownCounter* total_bytes_saved_;
  Variable* total_original_bytes_;
  Variable* num_uses_;
  Variable* num_flatten_imports_charset_mismatch_;
  Variable* num_flatten_imports_invalid_url_;
  Variable* num_flatten_imports_limit_exceeded_;
  Variable* num_flatten_imports_minify_failed_;
  Variable* num_flatten_imports_recursion_;
  Variable* num_flatten_imports_complex_queries_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_REWRITE_TEST_BASE_H_
