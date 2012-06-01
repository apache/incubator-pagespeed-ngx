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

  ValidateWithStats(id, html_input, html_output,
                    css_input, expected_css_output, flags);
}

void CssRewriteTestBase::ResetStats() {
  num_blocks_rewritten_->Set(0);
  num_fallback_rewrites_->Set(0);
  num_parse_failures_->Set(0);
  num_rewrites_dropped_->Set(0);
  total_bytes_saved_->Set(0);
  total_original_bytes_->Set(0);
  num_uses_->Set(0);
}

void CssRewriteTestBase::ValidateWithStats(
    const StringPiece& id,
    const GoogleString& html_input, const GoogleString& expected_html_output,
    const StringPiece& css_input, const StringPiece& expected_css_output,
    int flags) {
  ResetStats();

  // Rewrite
  ValidateExpected(id, html_input, expected_html_output);

  // Check stats
  if (!FlagSet(flags, kNoStatCheck)) {
    if (FlagSet(flags, kExpectSuccess)) {
      EXPECT_EQ(1, num_blocks_rewritten_->Get()) << id;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << id;
      EXPECT_EQ(0, num_parse_failures_->Get()) << id;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << id;
      EXPECT_EQ(css_input.size() - expected_css_output.size(),
                total_bytes_saved_->Get()) << id;
      EXPECT_EQ(css_input.size(), total_original_bytes_->Get()) << id;
      EXPECT_EQ(1, num_uses_->Get()) << id;
    } else if (FlagSet(flags, kExpectNoChange)) {
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << id;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << id;
      EXPECT_EQ(0, num_parse_failures_->Get()) << id;
      // TODO(sligocki): Test num_rewrites_dropped_. Currently a couple tests
      // have kExpectNoChange, but fail at a different place in the code, so
      // they do not trigger the num_rewrites_dropped_ variable.
      // EXPECT_EQ(1, num_rewrites_dropped_->Get()) << id;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << id;
      EXPECT_EQ(0, total_original_bytes_->Get()) << id;
      EXPECT_EQ(0, num_uses_->Get()) << id;
    } else if (FlagSet(flags, kExpectFallback)) {
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << id;
      EXPECT_EQ(1, num_fallback_rewrites_->Get()) << id;
      EXPECT_EQ(1, num_parse_failures_->Get()) << id;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << id;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << id;
      EXPECT_EQ(0, total_original_bytes_->Get()) << id;
      EXPECT_EQ(1, num_uses_->Get()) << id;
    } else {
      CHECK(FlagSet(flags, kExpectFailure));
      EXPECT_EQ(0, num_blocks_rewritten_->Get()) << id;
      EXPECT_EQ(0, num_fallback_rewrites_->Get()) << id;
      EXPECT_EQ(1, num_parse_failures_->Get()) << id;
      EXPECT_EQ(0, num_rewrites_dropped_->Get()) << id;
      EXPECT_EQ(0, total_bytes_saved_->Get()) << id;
      EXPECT_EQ(0, total_original_bytes_->Get()) << id;
      EXPECT_EQ(0, num_uses_->Get()) << id;
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


void CssRewriteTestBase::GetNamerForCss(const StringPiece& leaf_name,
                                        const GoogleString& expected_css_output,
                                        ResourceNamer* namer) {
  namer->set_id(RewriteOptions::kCssFilterId);
  namer->set_hash(hasher()->Hash(expected_css_output));
  namer->set_ext("css");
  namer->set_name(leaf_name);
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
  GetNamerForCss(StrCat(id, ".css"), expected_css_output, &namer);
  return ExpectedUrlForNamer(namer);
}

// Check that external CSS gets rewritten correctly.
void CssRewriteTestBase::ValidateRewriteExternalCssUrl(
    const StringPiece& css_url,
    const GoogleString& css_input,
    const GoogleString& expected_css_output,
    int flags) {
  CheckFlags(flags);

  // Set input file.
  if (!FlagSet(flags, kNoClearFetcher)) {
    ClearFetcherResponses();
  }
  SetResponseWithDefaultHeaders(css_url, kContentTypeCss, css_input, 300);

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

  static const char html_template[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "%s"
      "  <!-- Style starts here -->\n"
      "  <link rel='stylesheet' type='text/css' href='%s'%s>\n"
      "  <!-- Style ends here -->\n"
      "</head>";

  GoogleString html_input  = StringPrintf(html_template, meta_tag.c_str(),
                                          css_url.as_string().c_str(),
                                          link_extras.c_str());
  GoogleString html_output;

  ResourceNamer namer;
  GoogleUrl css_gurl(css_url);
  GetNamerForCss(css_gurl.LeafWithQuery(), expected_css_output, &namer);
  GoogleString expected_new_url =
      Encode(css_gurl.AllExceptLeaf(), namer.id(), namer.hash(),
             namer.name(), namer.ext());

  if (FlagSet(flags, kExpectSuccess) || FlagSet(flags, kExpectFallback)) {
    html_output = StringPrintf(html_template, meta_tag.c_str(),
                               expected_new_url.c_str(), link_extras.c_str());
  } else {
    html_output = html_input;
  }

  ValidateWithStats(css_url, html_input, html_output,
                    css_input, expected_css_output, flags);

  // If we produced a new output resource, check it.
  if (FlagSet(flags, kExpectSuccess) || FlagSet(flags, kExpectFallback)) {
    GoogleString actual_output;
    // TODO(sligocki): This will only work with mock_hasher.
    EXPECT_TRUE(FetchResourceUrl(expected_new_url, &actual_output)) << css_url;
    EXPECT_EQ(expected_css_output, actual_output) << css_url;

    // Serve from new context.
    if (!FlagSet(flags, kNoOtherContexts)) {
      ServeResourceFromManyContexts(expected_new_url, expected_css_output);
    }
  }
}

// Helper to test for how we handle trailing junk
void CssRewriteTestBase::TestCorruptUrl(const char* new_suffix) {
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
  EXPECT_TRUE(FetchResourceUrl(munged_url, &output));

  // Now see that output is correct
  ValidateRewriteExternalCss(
      "rep", kInput, kOutput,
      kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

}  // namespace net_instaweb
