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
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ResourceNamer;
struct ContentType;

class CssRewriteTestBase : public ResourceManagerTestBase {
 protected:
  CssRewriteTestBase() {
    num_blocks_rewritten_ =
        statistics()->GetVariable(CssFilter::kBlocksRewritten);
    num_fallback_rewrites_ =
        statistics()->GetVariable(CssFilter::kFallbackRewrites);
    num_parse_failures_ = statistics()->GetVariable(CssFilter::kParseFailures);
    num_rewrites_dropped_ =
        statistics()->GetVariable(CssFilter::kRewritesDropped);
    total_bytes_saved_ = statistics()->GetVariable(CssFilter::kTotalBytesSaved);
    total_original_bytes_ =
        statistics()->GetVariable(CssFilter::kTotalOriginalBytes);
    num_uses_ = statistics()->GetVariable(CssFilter::kUses);
  }
  ~CssRewriteTestBase();

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    options()->set_always_rewrite_css(true);
    AddFilter(RewriteOptions::kRewriteCss);
  }

  enum ValidationFlags {
    kExpectSuccess = 1,   // CSS parser succeeds and URL should be rewritten.
    kExpectNoChange = 2,  // CSS parser succeeds but URL not rewritten because
                          // we increased the size of contents.
    kExpectFallback = 4,  // CSS parser fails, fallback succeeds.
    kExpectFailure = 8,   // CSS parser fails, fallback failed or disabled.

    // TODO(sligocki): Explain why we turn off stats check at each use-site.
    kNoStatCheck = 16,
    // TODO(sligocki): Why would we ever want to clear fetcher?
    kNoClearFetcher = 32,
    // TODO(sligocki): Explain why we turn off other contexts.
    kNoOtherContexts = 64,

    kLinkCharsetIsUTF8 = 128,
    kLinkScreenMedia = 256,
    kLinkPrintMedia = 512,

    kMetaCharsetUTF8 = 1024,
    kMetaCharsetISO88591 = 2048,
    kMetaHttpEquiv = 4096,
    kMetaHttpEquivUnquoted = 8192,
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

  bool FlagSet(int flags, ValidationFlags f) {
    return (flags & f) != 0;
  }

  // Sanity check on flags passed in.
  void CheckFlags(int flags) {
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectSuccess),
                         FlagSet(flags, kExpectNoChange),
                         FlagSet(flags, kExpectFallback),
                         FlagSet(flags, kExpectFailure)));
  }

  // Check that inline CSS gets rewritten correctly.
  void ValidateRewriteInlineCss(const StringPiece& id,
                                const StringPiece& css_input,
                                const StringPiece& expected_css_output,
                                int flags);

  // Return the expected new URL with hash and all based on necessary data.
  GoogleString ExpectedRewrittenUrl(const StringPiece& original_url,
                                    const StringPiece& expected_contents,
                                    const StringPiece& filter_id,
                                    const ContentType& content_type);

  void GetNamerForCss(const StringPiece& id,
                      const GoogleString& expected_css_output,
                      ResourceNamer* namer);

  GoogleString ExpectedUrlForNamer(const ResourceNamer& namer);

  GoogleString ExpectedUrlForCss(const StringPiece& id,
                                 const GoogleString& expected_css_output);

  // Check that external CSS gets rewritten correctly.
  void ValidateRewriteExternalCss(const StringPiece& id,
                                  const GoogleString& css_input,
                                  const GoogleString& expected_css_output,
                                  int flags) {
    ValidateRewriteExternalCssUrl(StrCat(kTestDomain, id, ".css"),
                                  css_input, expected_css_output, flags);
  }

  void ValidateRewriteExternalCssUrl(const StringPiece& css_url,
                                     const GoogleString& css_input,
                                     const GoogleString& expected_css_output,
                                     int flags);


  void ValidateRewrite(const StringPiece& id,
                       const GoogleString& css_input,
                       const GoogleString& gold_output,
                       int flags) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, gold_output, flags);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, gold_output, flags);
  }

  void ValidateFailParse(const StringPiece& id, const GoogleString& css_input) {
    ValidateRewrite(id, css_input, css_input, kExpectFailure);
  }

  // Reset all Variables.
  void ResetStats();

  // Validate HTML rewrite as well as checking statistics.
  void ValidateWithStats(
      const StringPiece& id,
      const GoogleString& html_input, const GoogleString& expected_html_output,
      const StringPiece& css_input, const StringPiece& expected_css_output,
      int flags);

  // Helper to test for how we handle trailing junk
  void TestCorruptUrl(const char* junk);

  Variable* num_blocks_rewritten_;
  Variable* num_fallback_rewrites_;
  Variable* num_parse_failures_;
  Variable* num_rewrites_dropped_;
  Variable* total_bytes_saved_;
  Variable* total_original_bytes_;
  Variable* num_uses_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_REWRITE_TEST_BASE_H_
