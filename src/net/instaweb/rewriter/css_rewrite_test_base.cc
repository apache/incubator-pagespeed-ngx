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

#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

CssRewriteTestBase::~CssRewriteTestBase() {}

// Check that inline CSS gets rewritten correctly.
void CssRewriteTestBase::ValidateRewriteInlineCss(
    const StringPiece& id,
    const StringPiece& css_input,
    const StringPiece& expected_css_output,
    int flags) {
  static const char prefix[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Style starts here -->\n"
      "  <style type='text/css'>";
  static const char suffix[] = "</style>\n"
      "  <!-- Style ends here -->\n"
      "</head>";

  CheckFlags(flags);
  GoogleString html_input  = StrCat(prefix, css_input, suffix);
  GoogleString html_output = StrCat(prefix, expected_css_output, suffix);

  // Reset stats
  num_files_minified_->Set(0);
  minified_bytes_saved_->Set(0);
  num_parse_failures_->Set(0);

  // Rewrite
  ValidateExpected(id, html_input, html_output);

  // Check stats
  if (!(flags & kNoStatCheck)) {
    if (flags & kExpectChange) {
      EXPECT_EQ(1, num_files_minified_->Get()) << id;
      EXPECT_EQ(css_input.size() - expected_css_output.size(),
                minified_bytes_saved_->Get()) << id;
      EXPECT_EQ(0, num_parse_failures_->Get()) << id;
    } else {
      EXPECT_EQ(0, num_files_minified_->Get()) << id;
      EXPECT_EQ(0, minified_bytes_saved_->Get()) << id;
      if (flags & kExpectFailure) {
        EXPECT_EQ(1, num_parse_failures_->Get()) << id;
      } else {
        EXPECT_EQ(0, num_parse_failures_->Get()) << id;
      }
    }
  }
}

GoogleString CssRewriteTestBase::ExpectedRewrittenUrl(
    const StringPiece& original_url,
    const StringPiece& expected_contents,
    const StringPiece& filter_id,
    const ContentType& content_type) {
  GoogleUrl original_gurl(original_url);
  DCHECK(original_gurl.is_valid());
  return EncodeWithBase(original_gurl.Origin(),
                        original_gurl.AllExceptLeaf(), filter_id,
                        hasher()->Hash(expected_contents),
                        original_gurl.LeafWithQuery(),
                        content_type.file_extension() + 1);  // +1 to skip '.'
}


void CssRewriteTestBase::GetNamerForCss(const StringPiece& id,
                                        const GoogleString& expected_css_output,
                                        ResourceNamer* namer) {
  namer->set_id(RewriteOptions::kCssFilterId);
  namer->set_hash(hasher()->Hash(expected_css_output));
  namer->set_ext("css");
  namer->set_name(StrCat(id, ".css"));
}

GoogleString CssRewriteTestBase::ExpectedUrlForNamer(
    const ResourceNamer& namer) {
  return Encode(kTestDomain, namer.id(), namer.hash(), namer.name(),
                namer.ext());
}

GoogleString CssRewriteTestBase::ExpectedUrlForCss(
    const StringPiece& id,
    const GoogleString& expected_css_output) {
  ResourceNamer namer;
  GetNamerForCss(id, expected_css_output, &namer);
  return ExpectedUrlForNamer(namer);
}

// Check that external CSS gets rewritten correctly.
void CssRewriteTestBase::ValidateRewriteExternalCss(
    const StringPiece& id,
    const GoogleString& css_input,
    const GoogleString& expected_css_output,
    int flags) {
  CheckFlags(flags);

  // TODO(sligocki): Allow arbitrary URLs.
  GoogleString css_url = StrCat(kTestDomain, id, ".css");

  // Set input file.
  if ((flags & kNoClearFetcher) == 0) {
    ClearFetcherResponses();
  }
  InitResponseHeaders(StrCat(id, ".css"), kContentTypeCss, css_input, 300);

  GoogleString link_extras("");
  if ((flags & kLinkCharsetIsUTF8) != 0) {
    link_extras = " charset='utf-8'";
  }
  if ((flags & kLinkScreenMedia) != 0 && (flags & kLinkPrintMedia) != 0) {
    StrAppend(&link_extras, " media='screen,print'");
  } else if ((flags & kLinkScreenMedia) != 0) {
    StrAppend(&link_extras, " media='screen'");
  } else if ((flags & kLinkPrintMedia) != 0) {
    StrAppend(&link_extras, " media='print'");
  }

  static const char html_template[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Style starts here -->\n"
      "  <link rel='stylesheet' type='text/css' href='%s'%s>\n"
      "  <!-- Style ends here -->\n"
      "</head>";

  GoogleString html_input  = StringPrintf(html_template, css_url.c_str(),
                                          link_extras.c_str());

  GoogleString html_output;

  ResourceNamer namer;
  GetNamerForCss(id, expected_css_output, &namer);
  GoogleString expected_new_url = ExpectedUrlForNamer(namer);

  if (flags & kExpectChange) {
    html_output = StringPrintf(html_template, expected_new_url.c_str(),
                               link_extras.c_str());
  } else {
    html_output = html_input;
  }

  // Reset stats
  num_files_minified_->Set(0);
  minified_bytes_saved_->Set(0);
  num_parse_failures_->Set(0);

  // Rewrite
  ValidateExpected(id, html_input, html_output);

  // Check stats, if requested
  if (!(flags & kNoStatCheck)) {
    if (flags & kExpectChange) {
      EXPECT_EQ(1, num_files_minified_->Get()) << id;
      EXPECT_EQ(css_input.size() - expected_css_output.size(),
                minified_bytes_saved_->Get()) << id;
      EXPECT_EQ(0, num_parse_failures_->Get()) << id;
    } else {
      EXPECT_EQ(0, num_files_minified_->Get()) << id;
      EXPECT_EQ(0, minified_bytes_saved_->Get()) << id;
      if (flags & kExpectFailure) {
        EXPECT_EQ(1, num_parse_failures_->Get()) << id;
      } else {
        EXPECT_EQ(0, num_parse_failures_->Get()) << id;
      }
    }
  }

  // If we produced a new output resource, check it.
  if (flags & kExpectChange) {
    GoogleString actual_output;
    // TODO(sligocki): This will only work with mock_hasher.
    EXPECT_TRUE(ServeResourceUrl(expected_new_url, &actual_output)) << id;
    EXPECT_EQ(expected_css_output, actual_output) << id;

    // Serve from new context.
    if ((flags & kNoOtherContexts) == 0) {
      ServeResourceFromManyContexts(expected_new_url, expected_css_output);
    }
  }
}

// Helper to test for how we handle trailing junk
void CssRewriteTestBase::TestCorruptUrl(const char* junk,
                                        bool should_fetch_ok) {
  const char kInput[] = " div { } ";
  const char kOutput[] = "div{}";
  // Compute normal version
  ValidateRewriteExternalCss("rep", kInput, kOutput,
                             kExpectChange | kExpectSuccess);

  // Fetch with messed up extension
  GoogleString css_url = ExpectedUrlForCss("rep", kOutput);
  GoogleString output;
  EXPECT_EQ(should_fetch_ok,
            ServeResourceUrl(StrCat(css_url, junk), &output));

  // Now see that output is correct
  ValidateRewriteExternalCss(
      "rep", kInput, kOutput,
      kExpectChange | kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

}  // namespace net_instaweb
