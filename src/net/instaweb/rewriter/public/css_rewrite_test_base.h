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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CssRewriteTestBase : public ResourceManagerTestBase {
 protected:
  CssRewriteTestBase() {
    RewriteDriver::Initialize(&statistics_);

    num_files_minified_ = statistics_.GetVariable(CssFilter::kFilesMinified);
    minified_bytes_saved_ =
        statistics_.GetVariable(CssFilter::kMinifiedBytesSaved);
    num_parse_failures_ = statistics_.GetVariable(CssFilter::kParseFailures);
  }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    AddFilter(RewriteOptions::kRewriteCss);
    options_.set_always_rewrite_css(true);
  }

  enum ValidationFlags {
    kExpectNoChange = 1,
    kExpectChange = 2,
    kExpectFailure = 4,
    kExpectSuccess = 8,
    kNoStatCheck = 16,
    kNoClearFetcher = 32,
    kNoOtherContexts = 64
  };

  static bool ExactlyOneTrue(bool a, bool b) {
    return a ^ b;
  }

  bool FlagSet(int flags, ValidationFlags f) {
    return (flags & f) != 0;
  }

  // Sanity check on flags passed in -- should specify exactly
  // one of kExpectChange/kExpectNoChange and kExpectFailure/kExpectSuccess
  void CheckFlags(int flags) {
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectChange),
                         FlagSet(flags, kExpectNoChange)));
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectFailure),
                         FlagSet(flags, kExpectSuccess)));
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
                                  int flags);

  void ValidateRewrite(const StringPiece& id,
                       const GoogleString& css_input,
                       const GoogleString& gold_output,
                       int flags = kExpectChange | kExpectSuccess) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, gold_output, flags);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, gold_output, flags);
  }

  void ValidateFailParse(const StringPiece& id, const GoogleString& css_input) {
    ValidateRewrite(id, css_input, css_input, kExpectNoChange | kExpectFailure);
  }

  // Helper to test for how we handle trailing junk
  void TestCorruptUrl(const char* junk, bool should_fetch_ok);

  Variable* num_files_minified_;
  Variable* minified_bytes_saved_;
  Variable* num_parse_failures_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_REWRITE_TEST_BASE_H_
