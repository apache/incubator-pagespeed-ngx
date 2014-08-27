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
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_url_extractor.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/wildcard.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

CssRewriteTestBase::~CssRewriteTestBase() {}

// Check that inline CSS gets rewritten correctly.
bool CssRewriteTestBase::ValidateRewriteInlineCss(
    StringPiece id, StringPiece css_input, StringPiece expected_css_output,
    int flags) {
  static const char prefix[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Style starts here -->\n"
      "  <style type='text/css'>";
  static const char suffix1[] = "</style>";
  static const char suffix2[] =
      "\n"
      "  <!-- Style ends here -->\n"
      "</head>";

  GoogleString html_url = StrCat(kTestDomain, id, ".html");

  CheckFlags(flags);
  GoogleString html_input  = StrCat(prefix, css_input, suffix1, suffix2);
  GoogleString html_output = StrCat(prefix, expected_css_output, suffix1,
                                    DebugMessage(html_url), suffix2);

  return ValidateWithStats(id, html_input, html_output,
                           css_input, expected_css_output, flags);
}

void CssRewriteTestBase::ResetStats() {
  num_blocks_rewritten_->Clear();
  num_fallback_rewrites_->Clear();
  num_parse_failures_->Clear();
  num_rewrites_dropped_->Clear();
  total_bytes_saved_->Clear();
  total_original_bytes_->Clear();
  num_uses_->Clear();
  num_flatten_imports_charset_mismatch_->Clear();
  num_flatten_imports_invalid_url_->Clear();
  num_flatten_imports_limit_exceeded_->Clear();
  num_flatten_imports_minify_failed_->Clear();
  num_flatten_imports_recursion_->Clear();
  num_flatten_imports_complex_queries_->Clear();
}

bool CssRewriteTestBase::ValidateWithStats(
    StringPiece id,
    StringPiece html_input, StringPiece expected_html_output,
    StringPiece css_input, StringPiece expected_css_output,
    int flags) {
  ResetStats();

  // Rewrite
  bool success = ValidateExpected(id, html_input, expected_html_output);

  // Check stats
  if (success && !FlagSet(flags, kNoStatCheck)) {
    if (FlagSet(flags, kExpectSuccess)) {
      EXPECT_EQ(1, num_blocks_rewritten_->Get()) << css_input;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << css_input;
      EXPECT_EQ(0, num_parse_failures_->Get()) << css_input;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << css_input;
      EXPECT_EQ(static_cast<int>(css_input.size()) -
                static_cast<int>(expected_css_output.size()),
                total_bytes_saved_->Get()) << css_input;
      EXPECT_EQ(css_input.size(), total_original_bytes_->Get()) << css_input;
      EXPECT_EQ(1, num_uses_->Get()) << css_input;
    } else if (FlagSet(flags, kExpectCached)) {
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << css_input;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << css_input;
      EXPECT_EQ(0, num_parse_failures_->Get()) << css_input;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << css_input;
      EXPECT_EQ(0, total_original_bytes_->Get()) << css_input;
      EXPECT_EQ(1, num_uses_->Get()) << css_input;  // The only non-zero value.
    } else if (FlagSet(flags, kExpectNoChange)) {
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << css_input;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << css_input;
      EXPECT_EQ(0, num_parse_failures_->Get()) << css_input;
      // TODO(sligocki): Test num_rewrites_dropped_. Currently a couple tests
      // have kExpectNoChange, but fail at a different place in the code, so
      // they do not trigger the num_rewrites_dropped_ variable.
      // EXPECT_EQ(1, num_rewrites_dropped_->Get()) << css_input;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << css_input;
      EXPECT_EQ(0, total_original_bytes_->Get()) << css_input;
      EXPECT_EQ(0, num_uses_->Get()) << css_input;
    } else if (FlagSet(flags, kExpectFallback)) {
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << css_input;
      EXPECT_EQ(1, num_fallback_rewrites_->Get()) << css_input;
      EXPECT_EQ(1, num_parse_failures_->Get()) << css_input;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << css_input;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << css_input;
      EXPECT_EQ(0, total_original_bytes_->Get()) << css_input;
      EXPECT_EQ(1, num_uses_->Get()) << css_input;
    } else {
      CHECK(FlagSet(flags, kExpectFailure));
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << css_input;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << css_input;
      EXPECT_EQ(1, num_parse_failures_->Get()) << css_input;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << css_input;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << css_input;
      EXPECT_EQ(0, total_original_bytes_->Get()) << css_input;
      EXPECT_EQ(0, num_uses_->Get()) << css_input;
    }
  }

  // Check each of the import flattening statistics. Since each of these
  // is controlled individually they are not gated by kNoStatCheck above,
  // although if the results were fetched from the cache the flattener
  // doesn't count these as new errors so skip this check in that case.
  if (!FlagSet(flags, kExpectCached)) {
    EXPECT_EQ(FlagSet(flags, kFlattenImportsCharsetMismatch) ? 1 : 0,
              num_flatten_imports_charset_mismatch_->Get()) << css_input;
    EXPECT_EQ(FlagSet(flags, kFlattenImportsInvalidUrl) ? 1 : 0,
              num_flatten_imports_invalid_url_->Get()) << css_input;
    EXPECT_EQ(FlagSet(flags, kFlattenImportsLimitExceeded) ? 1 : 0,
              num_flatten_imports_limit_exceeded_->Get()) << css_input;
    EXPECT_EQ(FlagSet(flags, kFlattenImportsMinifyFailed) ? 1 : 0,
              num_flatten_imports_minify_failed_->Get()) << css_input;
    EXPECT_EQ(FlagSet(flags, kFlattenImportsRecursion) ? 1 : 0,
              num_flatten_imports_recursion_->Get()) << css_input;
    EXPECT_EQ(FlagSet(flags, kFlattenImportsComplexQueries) ? 1 : 0,
              num_flatten_imports_complex_queries_->Get()) << css_input;
  }

  // TODO(sligocki): This success value does not reflect failures in the
  // stats checks. Perhaps it should.
  return success;
}

void CssRewriteTestBase::GetNamerForCss(StringPiece leaf_name,
                                        StringPiece expected_css_output,
                                        ResourceNamer* namer) {
  namer->set_id(RewriteOptions::kCssFilterId);
  namer->set_hash(hasher()->Hash(expected_css_output));
  namer->set_ext("css");
  namer->set_name(leaf_name);
}

GoogleString CssRewriteTestBase::ExpectedUrlForNamer(
    const ResourceNamer& namer) {
  return Encode("", namer.id(), namer.hash(), namer.name(), namer.ext());
}

GoogleString CssRewriteTestBase::ExpectedUrlForCss(
    StringPiece id, StringPiece expected_css_output) {
  ResourceNamer namer;
  GetNamerForCss(StrCat(id, ".css"), expected_css_output, &namer);
  return ExpectedUrlForNamer(namer);
}

GoogleString CssRewriteTestBase::MakeHtmlWithExternalCssLink(
    StringPiece css_url, int flags, bool insert_debug_message) {
  GoogleString link_extras("");
  if (FlagSet(flags, kLinkCharsetIsUTF8)) {
    link_extras = " charset='utf-8'";
  }
  if (FlagSet(flags, kLinkScreenMedia) && FlagSet(flags, kLinkPrintMedia)) {
    StrAppend(&link_extras, " media='screen,print'");
  } else if (FlagSet(flags, kLinkScreenMedia)) {
    StrAppend(&link_extras, " media='screen'");
  } else if (FlagSet(flags, kLinkPrintMedia)) {
    StrAppend(&link_extras, " media='print'");
  }
  GoogleString meta_tag("");
  if (FlagSet(flags, kMetaCharsetUTF8)) {
    StrAppend(&meta_tag, "  <meta charset=\"utf-8\">");
  }
  if (FlagSet(flags, kMetaCharsetISO88591)) {
    StrAppend(&meta_tag, "  <meta charset=ISO-8859-1>");
  }
  if (FlagSet(flags, kMetaHttpEquiv)) {
    StrAppend(&meta_tag,
              "  <meta http-equiv=\"Content-Type\" "
              "content=\"text/html; charset=UTF-8\">");
  }
  if (FlagSet(flags, kMetaHttpEquivUnquoted)) {
    // Same as the previous one but content's value isn't quoted!
    StrAppend(&meta_tag,
              "  <meta http-equiv=\"Content-Type\" "
              "content=text/html; charset=ISO-8859-1>");
  }

  // This helper method is used to produce both input and expected_output HTML.
  // For input HTML we do not want to insert a debug message.
  // For expected_output HTML we do.
  GoogleString debug_message;
  if (insert_debug_message) {
    debug_message = DebugMessage(css_url);
  }

  return StringPrintf("<head>\n"
                      "  <title>Example style outline</title>\n"
                      "%s"
                      "  <!-- Style starts here -->\n"
                      "  <link rel='stylesheet' type='text/css' href='%.*s'%s>"
                      "%s\n"
                      "  <!-- Style ends here -->\n"
                      "</head>",
                      meta_tag.c_str(),
                      static_cast<int>(css_url.size()), css_url.data(),
                      link_extras.c_str(), debug_message.c_str());
}

GoogleString CssRewriteTestBase::MakeIndentedCssWithImage(
    StringPiece image_url) {
  return StrCat("body {\n"
                "  background-image: url(", image_url, ");\n"
                "}\n");
}

GoogleString CssRewriteTestBase::MakeMinifiedCssWithImage(
    StringPiece image_url) {
  return StrCat("body{background-image:url(", image_url, ")}");
}

GoogleString CssRewriteTestBase::ExtractCssBackgroundImage(
    StringPiece in_css) {
  const char css_template[] = "*{background-image:url(*)}*";
  GoogleString image_url;
  if (!Wildcard(css_template).Match(in_css)) {
    return image_url;
  }
  StringVector extracted_urls;
  CssUrlExtractor url_extractor;
  url_extractor.ExtractUrl(in_css, &extracted_urls);
  // Although the CssUrlExtractor returns a StringVector, we expect only one
  // url in the input string.
  if (extracted_urls.size() == 1) {
    image_url = extracted_urls[0];
  }

  return image_url;
}

// Check that external CSS gets rewritten correctly.
void CssRewriteTestBase::ValidateRewriteExternalCssUrl(
    StringPiece id, StringPiece css_url,
    StringPiece css_input, StringPiece expected_css_output, int flags) {
  CheckFlags(flags);

  // Set input file.
  if (!FlagSet(flags, kNoClearFetcher)) {
    ClearFetcherResponses();
  }
  SetResponseWithDefaultHeaders(css_url, kContentTypeCss, css_input, 300);
  GoogleString html_input = MakeHtmlWithExternalCssLink(css_url, flags, false);

  // Do we expect the URL to be rewritten?
  bool rewrite_url = (FlagSet(flags, kExpectSuccess) ||
                      FlagSet(flags, kExpectCached) ||
                      FlagSet(flags, kExpectFallback));

  GoogleString expected_new_url;
  if (rewrite_url) {
    ResourceNamer namer;
    GoogleUrl css_gurl(css_url);
    GetNamerForCss(css_gurl.LeafWithQuery(), expected_css_output, &namer);
    expected_new_url = Encode(css_gurl.AllExceptLeaf(), namer.id(),
                              namer.hash(), namer.name(), namer.ext());
  } else {
    css_url.CopyToString(&expected_new_url);
  }

  GoogleString expected_html_output =
      MakeHtmlWithExternalCssLink(expected_new_url, flags, true);
  ValidateWithStats(id, html_input, expected_html_output,
                    css_input, expected_css_output, flags);

  if (rewrite_url) {
    // Check the new output resource.
    GoogleString actual_output;
    // TODO(sligocki): This will only work with mock_hasher.
    ResponseHeaders headers_out;
    EXPECT_TRUE(FetchResourceUrl(expected_new_url, &actual_output,
                                 &headers_out)) << css_url;
    EXPECT_EQ(expected_css_output, actual_output) << css_url;

    // Non-fallback CSS should have very long caching headers
    if (!FlagSet(flags, kExpectFallback)) {
      EXPECT_TRUE(headers_out.IsProxyCacheable());
      EXPECT_LE(Timer::kYearMs, headers_out.cache_ttl_ms());
    }

    // Serve from new context.
    if (!FlagSet(flags, kNoOtherContexts)) {
      ServeResourceFromManyContexts(expected_new_url, expected_css_output);
    }
  }
}

// Helper to test for how we handle trailing junk
void CssRewriteTestBase::TestCorruptUrl(const char* new_suffix) {
  DebugWithMessage("");
  const char kInput[] = " div { } ";
  const char kOutput[] = "div{}";
  // Compute normal version
  ValidateRewriteExternalCss("rep", kInput, kOutput, kExpectSuccess);

  // Fetch with messed up extension
  GoogleString css_url = ExpectedUrlForCss("rep", kOutput);
  ASSERT_TRUE(StringCaseEndsWith(css_url, ".css"));
  GoogleString munged_url =
      ChangeSuffix(css_url, false /*replace*/, ".css", new_suffix);

  GoogleString output;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, munged_url), &output));

  // Now see that output is correct
  ValidateRewriteExternalCss(
      "rep", kInput, kOutput,
      kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

}  // namespace net_instaweb
