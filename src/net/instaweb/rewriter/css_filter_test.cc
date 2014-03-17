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
//     and sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"  // for ScopedMutex
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class MessageHandler;

namespace {

const char kInputStyle[] =
    ".background_blue { background-color: #f00; }\n"
    ".foreground_yellow { color: yellow; }\n";
const char kOutputStyle[] =
    ".background_blue{background-color:red}"
    ".foreground_yellow{color:#ff0}";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kUaWebp[] = "webp";
const char kUaWebpLossless[] = "webp-la";

class CssFilterTest : public CssRewriteTestBase {
 protected:
  void TestUrlAbsolutification(const StringPiece id,
                               const StringPiece css_input,
                               const StringPiece expected_output,
                               bool expect_unparseable_section,
                               bool enable_image_rewriting,
                               bool enable_proxy_mode,
                               bool enable_mapping_and_sharding) {
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    if (!enable_image_rewriting) {
      options()->DisableFilter(RewriteOptions::kRecompressJpeg);
      options()->DisableFilter(RewriteOptions::kRecompressPng);
      options()->DisableFilter(RewriteOptions::kRecompressWebp);
      options()->DisableFilter(RewriteOptions::kConvertPngToJpeg);
      options()->DisableFilter(RewriteOptions::kConvertJpegToWebp);
      options()->DisableFilter(RewriteOptions::kConvertGifToPng);
      options()->DisableFilter(RewriteOptions::kConvertToWebpLossless);
      options()->DisableFilter(RewriteOptions::kLeftTrimUrls);
      options()->DisableFilter(RewriteOptions::kExtendCacheImages);
      options()->DisableFilter(RewriteOptions::kSpriteImages);
    }

    // Set things up so that RewriteDriver::ShouldAbsolutifyUrl returns true
    // even though we are not proxying (but skip it if it has already been
    // set up by a previous call to this method).
    if (enable_mapping_and_sharding &&
        !options()->domain_lawyer()->can_rewrite_domains()) {
      DomainLawyer* domain_lawyer = options()->WriteableDomainLawyer();
      MessageHandler* handler = message_handler();
      ASSERT_TRUE(domain_lawyer->AddDomain("http://cdn.com/", handler));
      ASSERT_TRUE(domain_lawyer->AddDomain("http://test.com/", handler));
      ASSERT_TRUE(domain_lawyer->AddShard("cdn.com", "cdn1.com,cdn2.com",
                                          handler));
      EXPECT_FALSE(domain_lawyer->DoDomainsServeSameContent("cdn.com",
                                                            "test.com"));
      ASSERT_TRUE(domain_lawyer->AddRewriteDomainMapping("http://cdn.com",
                                                         "http://test.com",
                                                         handler));
      EXPECT_TRUE(domain_lawyer->DoDomainsServeSameContent("cdn.com",
                                                           "test.com"));
      EXPECT_TRUE(domain_lawyer->can_rewrite_domains());
      GoogleUrl src_base("http://test.com/foo.css");
      bool proxying = true;  // to ensure it's set to false.
      EXPECT_TRUE(rewrite_driver()->ShouldAbsolutifyUrl(src_base, src_base,
                                                        &proxying));
      EXPECT_FALSE(proxying);
      GoogleUrl dst_base("http://cdn.com/foo.css");
      proxying = true;  // again to ensure it's set to false.
      EXPECT_TRUE(rewrite_driver()->ShouldAbsolutifyUrl(src_base, dst_base,
                                                        &proxying));
      EXPECT_FALSE(proxying);
    }
    server_context()->ComputeSignature(options());

    // By default TestUrlNamer doesn't proxy but we might need it for this test.
    TestUrlNamer::SetProxyMode(enable_proxy_mode);

    SetResponseWithDefaultHeaders("foo.css", kContentTypeCss, css_input, 100);

    // Ensure that the input CSS has/has-not parse errors, as specified by the
    // expect_unparseable_section parameter, to cater for future improvements
    // in the CSS parser.
    Css::Parser parser(css_input);
    parser.set_preservation_mode(true);
    parser.set_quirks_mode(false);
    scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());
    EXPECT_TRUE(parser.errors_seen_mask() == Css::Parser::kNoError);
    EXPECT_EQ(expect_unparseable_section,
              parser.unparseable_sections_seen_mask() != Css::Parser::kNoError);

    Parse(id, CssLinkHref("foo.css"));

    // Check for CSS files in the rewritten page.
    StringVector css_urls;
    CollectCssLinks(StrCat(id, "_collect"), output_buffer_, &css_urls);
    ASSERT_LE(1UL, css_urls.size());
    StringPiece prefix;
    if (enable_mapping_and_sharding) {
      prefix = "http://cdn1.com/";
    } else if (factory()->use_test_url_namer()) {
      prefix = kTestDomain;
    } else {
      prefix = "";
    }
    EXPECT_EQ(Encode(prefix, "cf", "0", "foo.css", "css"), css_urls[0]);

    // Get absolute CSS URL.
    GoogleUrl base_url(kTestDomain);
    GoogleUrl css_url(base_url, css_urls[0]);

    // Check the content of the CSS file.
    GoogleString actual_output;
    EXPECT_TRUE(FetchResourceUrl(css_url.Spec(), &actual_output));
    EXPECT_STREQ(expected_output, actual_output);
  }

  // A helper function for webp tests. Note that we require
  // image_content_type independently of any filename extension in
  // input_image_name. The parameters output_image_name_template and
  // output_css_name_template should both include a single "%s" token
  // which is replaced by the input_image_name and the original CSS
  // resource name, respectively.
  void TestWebpRewriting(const char* input_image_name,
                         const net_instaweb::ContentType image_content_type,
                         const char* output_image_name_template,
                         const char* output_css_name_template) {
    static const char kInputCssName[] = "foo.css";
    GoogleString image_out = StringPrintf(output_image_name_template,
                                          input_image_name);
    GoogleString css_input = StringPrintf("body{background:url(%s)}",
                                          input_image_name);
    GoogleString css_output = StringPrintf("body{background:url(%s)}",
                                           image_out.c_str());
    GoogleString expected_url = StringPrintf(output_css_name_template,
                                             kInputCssName);
    AddFileToMockFetcher(StrCat(kTestDomain, input_image_name),
                         input_image_name, image_content_type, 100);
    server_context()->ComputeSignature(options());

    SetResponseWithDefaultHeaders(kInputCssName, kContentTypeCss,
                                  css_input, 100);
    Parse("webp", CssLinkHref(kInputCssName));
    // Check for CSS files in the rewritten page.
    StringVector css_urls;
    CollectCssLinks("collect", output_buffer_, &css_urls);
    ASSERT_EQ(1, css_urls.size());
    EXPECT_EQ(expected_url, css_urls[0]);

    // Check the content of the CSS file.
    GoogleString actual_output;
    EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, css_urls[0]),
                                 &actual_output));
    EXPECT_STREQ(css_output, actual_output);
  }
};

TEST_F(CssFilterTest, SimpleRewriteCssTest) {
  ValidateRewrite("rewrite_css", kInputStyle, kOutputStyle, kExpectSuccess);
}

TEST_F(CssFilterTest, SimpleRewriteCssTestExternal) {
  ValidateRewriteExternalCss("rewrite_css", kInputStyle, kOutputStyle,
                             kExpectSuccess);
}

TEST_F(CssFilterTest, SimpleRewriteCssTestExternalUnhealthy) {
  lru_cache()->set_is_healthy(false);
  ValidateRewriteExternalCss("rewrite_css", kInputStyle, kOutputStyle,
                             kExpectNoChange);
}

TEST_F(CssFilterTest, CssRewriteRandomDropAll) {
  // Test that randomized optimization doesn't rewrite when drop % set to 100
  options()->ClearSignatureForTesting();
  options()->set_rewrite_random_drop_percentage(100);
  server_context()->ComputeSignature(options());
  for (int i = 0; i < 100; ++i) {
    ValidateRewriteExternalCss("rewrite_css", kInputStyle, kOutputStyle,
                               kExpectNoChange);
    lru_cache()->Clear();
    ClearStats();
  }
}

TEST_F(CssFilterTest, CssRewriteRandomDropNone) {
  // Test that randomized optimization always rewrites when drop % set to 0.
  options()->ClearSignatureForTesting();
  options()->set_rewrite_random_drop_percentage(0);
  server_context()->ComputeSignature(options());
  for (int i = 0; i < 100; ++i) {
    ValidateRewriteExternalCss("rewrite_css", kInputStyle, kOutputStyle,
                               kExpectSuccess);
    lru_cache()->Clear();
    ClearStats();
  }
}

TEST_F(CssFilterTest, RewriteCss404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.css");
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");
}

class CssFilterTestCustomOptions : public CssFilterTest {
 protected:
  // Derived classes should add their options and then call
  // CssFilterTest::SetUp.
  virtual void SetUp() {}
};

TEST_F(CssFilterTestCustomOptions, CssPreserveUrls) {
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineCss);
  options()->SoftEnableFilterForTesting(RewriteOptions::kRewriteCss);
  options()->set_css_preserve_urls(true);
  CssFilterTest::SetUp();
  // Verify that preserve had a chance to forbid some filters.
  EXPECT_FALSE(options()->Enabled(RewriteOptions::kInlineCss));
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kInputStyle, 100);

  // The URL shouldn't change.
  ValidateNoChanges("css_preserve_urls_on", "<link rel=StyleSheet href=a.css>");

  // We should have the optimized CSS even though we didn't render the URL.
  ClearStats();
  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", "a.css", "css");
  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Was the CSS minified?
  EXPECT_EQ(kOutputStyle, out_css);
}

TEST_F(CssFilterTestCustomOptions, CssPreserveUrlsOverridingExtend) {
  scoped_ptr<RewriteOptions> global_options(options()->NewOptions());
  global_options->EnableFilter(RewriteOptions::kExtendCacheCss);

  scoped_ptr<RewriteOptions> vhost_options(options()->NewOptions());
  vhost_options->EnableFilter(RewriteOptions::kRewriteCss);
  vhost_options->SoftEnableFilterForTesting(RewriteOptions::kInlineCss);
  vhost_options->SoftEnableFilterForTesting(RewriteOptions::kRewriteCss);
  vhost_options->set_css_preserve_urls(true);  // This will win over ExtendCache
  options()->Merge(*global_options);
  options()->Merge(*vhost_options);

  CssFilterTest::SetUp();
  // Verify that preserve had a chance to forbid some filters.
  EXPECT_FALSE(options()->Enabled(RewriteOptions::kInlineCss));
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kInputStyle, 100);

  // The URL shouldn't change.
  ValidateNoChanges("css_preserve_urls_on", "<link rel=StyleSheet href=a.css>");

  // We should have the optimized CSS even though we didn't render the URL.
  ClearStats();
  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", "a.css", "css");
  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Was the CSS minified?
  EXPECT_EQ(kOutputStyle, out_css);
}

TEST_F(CssFilterTestCustomOptions, CssPreserveUrlsWithMergedCacheExtend) {
  scoped_ptr<RewriteOptions> global_options(options()->NewOptions());
  global_options->EnableFilter(RewriteOptions::kRewriteCss);
  global_options->set_css_preserve_urls(true);

  // Because we set extend_cache at a "lower level", it takes priority
  // over preserve_css_urls and enables URL-rewriting for CSS.
  scoped_ptr<RewriteOptions> vhost_options(options()->NewOptions());
  vhost_options->EnableFilter(RewriteOptions::kExtendCacheCss);
  options()->Merge(*global_options);
  options()->Merge(*vhost_options);

  CssFilterTest::SetUp();
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kInputStyle, 100);

  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", "a.css", "css");

  // The URL should be updated since by specifying "extend_cache_css" the user
  // has signaled its OK to change the URLS.
  ValidateExpected("css_preserve_urls_on",
                   "<link rel=StyleSheet href=a.css>",
                   StrCat("<link rel=StyleSheet href=",
                          ExpectedUrlForCss("a", kOutputStyle), ">"));

  // We should have the optimized CSS even though we didn't render the URL.
  ClearStats();
  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Was the CSS minified?
  EXPECT_EQ(kOutputStyle, out_css);
}

TEST_F(CssFilterTestCustomOptions, CssPreserveUrlsNoPreemptiveRewrite) {
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineCss);
  options()->set_css_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_css(false);
  CssFilterTest::SetUp();
  // Verify that preserve had a chance to forbid some filters.
  EXPECT_FALSE(options()->Enabled(RewriteOptions::kInlineCss));
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kInputStyle, 100);

  // The URL shouldn't change.
  ValidateNoChanges("css_preserve_urls_on_no_preemptive",
                    "<link rel=StyleSheet href=a.css>");

  // We should not have attempted any rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // But, if we fetch the optimized CSS directly, we should receive the
  // optimized version.
  ClearStats();
  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", "a.css", "css");
  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
  EXPECT_EQ(kOutputStyle, out_css);
}

TEST_F(CssFilterTest, LinkHrefCaseInsensitive) {
  // Make sure we check rel value case insensitively.
  // http://code.google.com/p/modpagespeed/issues/detail?id=354
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kInputStyle, 100);
  ValidateExpected(
      "case_insensitive", "<link rel=StyleSheet href=a.css>",
      StrCat("<link rel=StyleSheet href=",
             ExpectedUrlForCss("a", kOutputStyle),
             ">"));
}

TEST_F(CssFilterTest, UrlTooLong) {
  // Make the filename maximum size, so we cannot rewrite it.
  // -4 because .css will be appended.
  GoogleString filename(options()->max_url_segment_size() - 4, 'z');
  // If filename wasn't too long, this would be rewritten (like in
  // SimpleRewriteCssTest).
  ValidateRewriteExternalCss(filename, kInputStyle, kInputStyle,
                             kExpectNoChange);
}

// Make sure we can deal with 0 character nodes between open and close of style.
TEST_F(CssFilterTest, RewriteEmptyCssTest) {
  // Note: We must check stats ourselves because, for technical reasons,
  // empty inline styles are not treated as being rewritten at all.
  ValidateRewriteInlineCss("rewrite_empty_css-inline", "", "",
                           kExpectSuccess | kNoStatCheck);
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
  EXPECT_EQ(0, num_blocks_rewritten_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
  EXPECT_EQ(0, num_parse_failures_->Get());

  ValidateRewriteExternalCss("rewrite_empty_css-external", "", "",
                             kExpectSuccess | kNoStatCheck);
  EXPECT_EQ(0, total_bytes_saved_->Get());
  EXPECT_EQ(0, num_parse_failures_->Get());
}

// Make sure we do not recompute external CSS when re-processing an already
// handled page.
TEST_F(CssFilterTest, RewriteRepeated) {
  // ValidateRewriteExternalCssUrl calls FetchResourceUrl, which resets
  // the request context on rewrite_driver_. So we can test changes to the log
  // record, we keep a (ref counted) pointer to the request context before
  // running the validate.
  // TODO(marq): Make this not necessary by folding log validation into
  // ValidateWithStats().
  RequestContextPtr rctx = rewrite_driver()->request_context();
  rctx->log_record()->SetAllowLoggingUrls(true);
  ValidateRewriteExternalCss("rep", " div { } ", "div{}", kExpectSuccess);
  int inserts_before = lru_cache()->num_inserts();
  EXPECT_EQ(1, num_blocks_rewritten_->Get());  // for factory_
  EXPECT_EQ(1, num_uses_->Get());
  {
    ScopedMutex lock(rctx->log_record()->mutex());
    EXPECT_STREQ("cf", rctx->log_record()->AppliedRewritersString());
  }
  VerifyRewriterInfoEntry(rctx->log_record(), "cf", 0, 0, 1, 1,
                          "http://test.com/rep.css");
  ResetStats();

  rctx.reset(rewrite_driver()->request_context());
  rctx->log_record()->SetAllowLoggingUrls(true);
  ValidateRewriteExternalCss("rep", " div { } ", "div{}",
                             kExpectSuccess | kNoStatCheck);
  int inserts_after = lru_cache()->num_inserts();
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(inserts_before, inserts_after);
  EXPECT_EQ(0, num_blocks_rewritten_->Get());
  EXPECT_EQ(1, num_uses_->Get());
  {
    ScopedMutex lock(rctx->log_record()->mutex());
    EXPECT_STREQ("cf", rctx->log_record()->AppliedRewritersString());
  }
  VerifyRewriterInfoEntry(rctx->log_record(), "cf", 0, 0, 1, 1,
                          "http://test.com/rep.css");
}

// Make sure we do not reparse external CSS when we know it already has
// a parse error.
TEST_F(CssFilterTest, RewriteRepeatedParseError) {
  const char kInvalidCss[] = "@media }}";
  // Note: It is important that these both have the same id so that the
  // generated CSS file names are identical.
  // TODO(sligocki): This is sort of annoying for error reporting which
  // is suposed to use id to uniquely distinguish which test was running.
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "", kExpectFailure);
  // First time, we fail to parse.
  EXPECT_EQ(1, num_parse_failures_->Get());
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "",
                             kExpectFailure | kNoStatCheck);
  // Second time, we remember failure and so don't try to reparse.
  EXPECT_EQ(0, num_parse_failures_->Get());
}

// Deal nicely with non-UTF8 encodings.
TEST_F(CssFilterTest, NonUtf8) {
  // Distilled examples.
  // gb2312 (Not valid UTF-8, multi-byte).
  ValidateRewrite("font", "a { font-family: \"\xCB\xCE\xCC\xE5\"; }",
                          "a{font-family: \"\xCB\xCE\xCC\xE5\"}",
                  kExpectSuccess);
  // Windows-1252 (Not valid UTF-8, single-byte).
  ValidateRewrite("string", ".foo { content: \"r\xE9sum\xE9\"; }",
                            ".foo{content: \"r\xE9sum\xE9\"}",
                  kExpectSuccess);
  // Shift_JIS (Not valid UTF-8, multi-byte, second byte may not set high bit).
  ValidateRewrite("ident_value",
                  ".foo { -moz-charset: \x83\x56\x83\x74\x83\x67\x83\x57; }",
                  ".foo{-moz-charset: \x83\x56\x83\x74\x83\x67\x83\x57}",
                  kExpectSuccess);
  // KOI8-R (Not valid UTF-8, single-byte).
  ValidateRewrite("ident_param", ".foo { \xEB\xEF\xE9-8: standard; }",
                                 ".foo{\xEB\xEF\xE9-8: standard}",
                  kExpectSuccess);
  // EUC-KR (Not valid UTF-8, multi-byte).
  ValidateRewrite("ident_selector", ".\xB8\xC0 { color: red; }",
                                    ".\xB8\xC0 {color:red}",
                  kExpectSuccess);

  // Verbatim example from http://www.baidu.com/
  ValidateRewrite("baidu", "#lk span {font:14px \"\xCB\xCE\xCC\xE5\"}",
                           "#lk span{font:14px \"\xCB\xCE\xCC\xE5\"}",
                           kExpectSuccess);
}

// In UTF-8, all multi-byte characters have high bit set. This is not true in
// other common web encodings.
TEST_F(CssFilterTest, Non8BitEncoding) {
  // Shift_JIS can have second bytes in range 0x40-0x7F,
  // which includes ASCII chars: @ A-Z [/]^_` a-z {|}~

  // 0x83 0x7D == KATAKANA LETTER MA
  // 0x7D == RIGHT CURLY BRACKET }
  ValidateRewrite("string-ma", ".foo { font-family: \"\x83\x7D\"; color: red }",
                               ".foo{font-family: \"\x83\x7D\";color:red}",
                  kExpectSuccess);
  // Note: This text currently fails to be parsed. But if that changes,
  // update this test to the correct golden rewrite.
  ValidateFailParse("ident-ma", ".foo { -win-magic: bar\x83\x7D; color: red }");

  // 0x83 0x7B == KATAKANA LETTER BO
  // 0x7B == LEFT CURLY BRACKET {
  ValidateRewrite("string-bo", ".foo { font-family: \"\x83\x7B\"; color: red }",
                               ".foo{font-family: \"\x83\x7B\";color:red}",
                  kExpectSuccess);
  // Note: This text currently fails to be parsed. But if that changes,
  // update this test to the correct golden rewrite.
  ValidateFailParse("ident-bo", ".foo { -win-magic: bar\x83\x7B; color: red }");
}

// Make sure bad requests do not corrupt our extension.
TEST_F(CssFilterTest, NoExtensionCorruption) {
  TestCorruptUrl(".css%22");
}

TEST_F(CssFilterTest, NoQueryCorruption) {
  TestCorruptUrl(".css?query");
}

TEST_F(CssFilterTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html");
}

TEST_F(CssFilterTest, RewriteVariousCss) {
  // TODO(sligocki): Get these tests to pass with setlocale.
  // EXPECT_TRUE(setlocale(LC_ALL, "tr_TR.utf8"));
  // Distilled examples.
  const char* good_examples[] = {
    "a.b #c.d e#d,f:g>h+i>j{color:red}",  // .#,>+: in selectors
    "a{border:solid 1px #ccc}",  // Multiple values declaration
    "a{border:none!important}",  // !important
    "a{background-image:url(foo.png)}",  // url
    "a{background-position:-19px 60%}",  // negative position
    "a{margin:0}",  // 0 w/ no units
    "a{padding:.01em -.25em}",  // fractions, negative and em
    "a{-moz-border-radius-topleft:0}",  // Browser-specific (-moz)
    ".ds{display:-moz-inline-box}",
    "a{background:none}",  // CSS Parser used to expand this.
    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    "a{color:inherit}",
    // Added for code coverage.
    // TODO(sligocki): Get rid of the space at end?
    // ";" may be needed for some browsers.
    "@import url(http://www.example.com) ;",
    "@media a,b{a{color:red}}",
    "@charset \"foobar\";",
    // Unescaped string: Odd chars: \(\)\,\"\'
    "a{content:\"Odd chars: \\(\\)\\,\\\"\\\'\"}",
    // Unescaped string: Unicode: \A0\A0
    "a{content:\"Unicode: \\A0\\A0\"}",
    "img{clip:rect(0px,60px,200px,0px)}",
    // CSS3-style pseudo-elements.
    "p.normal::selection{background:#c00;color:#fff}",
    "::-moz-focus-inner{border:0}",
    "input::-webkit-input-placeholder{color:#ababab}"
    // http://code.google.com/p/modpagespeed/issues/detail?id=51
    "a{box-shadow:-1px -2px 2px rgba(0,0,0,.15)}",  // CSS3 rgba
    // http://code.google.com/p/modpagespeed/issues/detail?id=66
    "a{-moz-transform:rotate(7deg)}",
    // Microsoft syntax values.
    "a{filter:progid:DXImageTransform.Microsoft.Alpha(Opacity=80)}",
    // Make sure we keep "\," distinguished from ",".
    "body{font-family:font\\,1,font\\,2}",
    // Distinguish \. from ., etc.
    // http://code.google.com/p/modpagespeed/issues/detail?id=574
    "#MyForm\\.myfield{property:value}",
    "\\*{color:red}",
    "a{property\\:more:value\\;more}"
    // Found in the wild:
    "a{width:overflow:hidden}",
    // IE hack: \9
    "div{margin:100px\\9 }",
    "div{margin\\9 :100px}",
    "div\\9 {margin:100px}",
    "a{color:red\\9 }",
    "a{background:none\\9 }",

    // Don't replace system color names with defaults.
    "a{color:WindowText}",
    "a{color:FooBar}",

    // Recovered parse errors:
    // Slashes in value list.
    ".border8{border-radius: 36px / 12px }",

    // http://code.google.com/p/modpagespeed/issues/detail?id=220
    // See https://developer.mozilla.org/en/CSS/-moz-transition-property
    // and http://www.webkit.org/blog/138/css-animation/
    "a{-webkit-transition-property:opacity,-webkit-transform }",

    // Parameterized pseudo-selector.
    "div:nth-child(1n) {color:red}",

    // IE8 Hack \0/
    // See http://dimox.net/personal-css-hacks-for-ie6-ie7-ie8/
    "a{color: red\\0/ ;background-color:green}",
    "a{font-family: font\\0  ;color:red}",

    "a{font:bold verdana 10px }",
    "a{foo: +bar }",
    "a{color: rgb(foo,+,) }",

    // CSS3 media queries.
    // http://code.google.com/p/modpagespeed/issues/detail?id=50
    "@media screen and (max-width:290px){a{color:red}}",
    "@media only print and (color){a{color:red}}",
    // Nonsensical, but syntactic, media query.
    "@media not (-moz-dimension-constraints:20 < width < 300 and 45 < height "
    "< 1000){a{color:red}}",

    // Unexpected @-statements
    "@keyframes wiggle { 0% { transform: rotate(6deg); } }",
    "@font-face { font-family: 'Ubuntu'; font-style: normal }",

    // Invalid font declaration.
    "a{font: menu foobar }",

    // Do not remove . between 1 and em, this needs to be lexed as:
    // INT(1) DELIM(.) IDENT(em)
    "a{padding-top: 1.em }",

    // Unexpected ! uses in declarations.
    "a{color: red !ie }",
    "a{color: !important red }",
    "a{color: red !important blue }",

    // Things from Alexa-100 that we get parsing errors for. Most are illegal
    // syntax/typos. Some are CSS3 constructs.

    // kDeclarationError from Alexa-100
    // Comma in values
    "a{webkit-transition-property: color, background-color }",
    // Special chars in property
    "a{//display: inline-block }",
    ".ad_300x250{/margin-top:-120px }",
    // Properties with no value
    "a{background-repeat;no-repeat }",
    // Typos
    "a{margin-right:0;width:113px;*/ }",
    "a{z-i ndex:19 }",
    "a{width:352px;height62px ;display:block}",
    "a{color: #5552 }",
    "a{1font-family:Tahoma, Arial, sans-serif }",
    "a{text align:center }",

    // kSelectorError from Alexa-100
    // Selector list ends in comma
    ".hp .col ul, {display:inline}",
    // Parameters for pseudoclass
    "body:not(:target) {color:red}",
    "a:not(.button):hover {color:red}",
    // Typos
    "# new_results_notification{font-size:12px}",
    ".bold: {font-weight:bold}",

    // kFunctionError from Alexa-100
    // Expression
    "a{_top: expression(0+((e=document.documen))) }",
    "a{width: expression(this.width > 120 ? 120:tr) }",
    // Equals in function
    "a{progid:DXImageTransform.Microsoft.AlphaImageLoader"
    "(src=/images/lb/internet_e) }",
    "a{progid:DXImageTransform.Microsoft.AlphaImageLoader"
    "(src=\"/images/lb/internet_e\") }",
    "a{progid:DXImageTransform.Microsoft.AlphaImageLoader"
    "(src='/images/lb/internet_e') }",

    // Mismatched {}s that are acceptable because they do not fail to close {s,
    // but instead have stray }s that would not be parsed as closing previous {s
    "a[}]{color:red}",

    // Don't "fix" font properties.
    "a{font:12px,clean}",
  };

  for (int i = 0; i < arraysize(good_examples); ++i) {
    GoogleString id = StringPrintf("distilled_css_good%d", i);
    ValidateRewrite(id, good_examples[i], good_examples[i], kExpectSuccess);
  }

  const char* fail_examples[] = {
    // Malformed @import statements.
    "@import styles.css; a { color: red; }",
    "@import \"styles.css\", \"other.css\"; a { color: red; }",
    "@import url(styles.css), url(other.css); a { color: red; }",
    "@import \"styles.css\"...; a { color: red; }",

    // Should fail, mismatched {}s.
    "{",
    "}",
    "}{",
    "a { color: red; }}}",
    // Mismatch in unparseable at-rule.
    "@foobar {",
    "@foobar }",
    // Unclosed at-rule that will break the first statement in the next CSS
    // file if combined.
    "@foobar ",
    // Mismatch in unparseable selector.
    "a[{] { color: red; }",
    "a {",
    "a }",
    // Mismatch in unparseable declaration.
    "a { *color{: red; }",
    "a { *color}: red; }",
    "a { filter: progid:DXImageTransform.Microsoft.AlphaImageLoader"
    "(src=/images/{lb/internet_e); }",
    // Missing end }s.
    "a { color: red",
    "a { color: red;\n .foo { margin: 10px; }",
    "a { color: red;\n h1 { margin: 10px; }",

    // Don't "fix" by adding space between 'and' and '('.
    "@media only screen and(min-resolution:240dpi){ .bar{ background: red; }}",

    // Things from Alexa-100 that we get parsing errors for. Most are illegal
    // syntax/typos. Some are CSS3 constructs.

    // kSelectorError from Alexa-100
    // Typos
    // Note: These fail because of the if (Done()) return NULL call in
    // ParseRuleset
    // Should fail (at least we should make sure CssCombineFilter does not
    // combine them, since they do not contain full statements).
    "a",
    "a { color: red }\n */",
    "a { color: red }\n // Comment",
    "a { color: red } .foo",
  };

  for (int i = 0; i < arraysize(fail_examples); ++i) {
    GoogleString id = StringPrintf("distilled_css_fail%d", i);
    ValidateFailParse(id, fail_examples[i]);
  }
}

// Things we could be optimizing.
// This test will fail when we start optimizing these thing.
TEST_F(CssFilterTest, ToOptimize) {
  const char* examples[][2] = {
    // Noticed from YUI minification.
    { ".gb1, .gb3 {}",
      // Could be: ""
      ".gb1,.gb3{}", },
    { ".lst:focus { outline:none; }",
      // Could be: ".lst:focus{outline:0}"
      ".lst:focus{outline:none}", },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    GoogleString id = StringPrintf("to_optimize_%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1], kExpectSuccess);
  }
}

// Test more complicated CSS.
TEST_F(CssFilterTest, ComplexCssTest) {
  // Real-world examples. Picked out of Wikipedia's CSS.
  const char* examples[][2] = {
    { "#userlogin, #userloginForm {\n"
      "  border: solid 1px #cccccc;\n"
      "  padding: 1.2em;\n"
      "  float: left;\n"
      "}\n",

      "#userlogin,#userloginForm{border:solid 1px #ccc;padding:1.2em;"
      "float:left}"},

    { "h3 .editsection { font-size: 76%; font-weight: normal; }\n",
      "h3 .editsection{font-size:76%;font-weight:normal}"},

    { "div.magnify a, div.magnify img {\n"
      "  display: block;\n"
      "  border: none !important;\n"
      "  background: none !important;\n"
      "}\n",

      "div.magnify a,div.magnify img{display:block;border:none!important;"
      "background:none!important}"},

    { "#ca-watch.icon a:hover {\n"
      "  background-image: url('images/watch-icons.png?1');\n"
      "  background-position: -19px 60%;\n"
      "}\n",

      "#ca-watch.icon a:hover{background-image:url(images/watch-icons.png?1);"
      "background-position:-19px 60%}"},

    { "body {\n"
      "  background: White;\n"
      "  /*font-size: 11pt !important;*/\n"
      "  color: Black;\n"
      "  margin: 0;\n"
      "  padding: 0;\n"
      "}\n",

      "body{background:#fff;color:#000;margin:0;padding:0}"},

    { ".suggestions-result{\n"
      "  color:black;\n"
      "  color:WindowText;\n"
      "  padding:0.01em 0.25em;\n"
      "}\n",

      ".suggestions-result{color:#000;color:WindowText;padding:.01em .25em}" },

    { ".ui-corner-tl { -moz-border-radius-topleft: 0; -webkit-border-top-left"
      "-radius: 0; }\n",

      ".ui-corner-tl{-moz-border-radius-topleft:0;-webkit-border-top-left"
      "-radius:0}"},

    { ".ui-tabs .ui-tabs-nav li.ui-tabs-selected a, .ui-tabs .ui-tabs-nav li."
      "ui-state-disabled a, .ui-tabs .ui-tabs-nav li.ui-state-processing a { "
      "cursor: pointer; }\n",

      ".ui-tabs .ui-tabs-nav li.ui-tabs-selected a,.ui-tabs .ui-tabs-nav "
      "li.ui-state-disabled a,.ui-tabs .ui-tabs-nav li.ui-state-processing a{"
      "cursor:pointer}"},

    { ".ui-datepicker-cover {\n"
      "  display: none; /*sorry for IE5*/\n"
      "  display/**/: block; /*sorry for IE5*/\n"
      "  position: absolute; /*must have*/\n"
      "  z-index: -1; /*must have*/\n"
      "  filter: mask(); /*must have*/\n"
      "  top: -4px; /*must have*/\n"
      "  left: -4px; /*must have*/\n"
      "  width: 200px; /*must have*/\n"
      "  height: 200px; /*must have*/\n"
      "}\n",

      // TODO(sligocki): Should we preserve the dispaly/**/:?
      //".ui-datepicker-cover{display:none;display/**/:block;position:absolute;"
      //"z-index:-1;filter:mask();top:-4px;left:-4px;width:200px;height:200px}"

      ".ui-datepicker-cover{display:none;display:block;position:absolute;"
      "z-index:-1;filter:mask();top:-4px;left:-4px;width:200px;height:200px}" },

    { ".shift {\n"
      "  -moz-transform: rotate(7deg);\n"
      "  -webkit-transform: rotate(7deg);\n"
      "  -moz-transform: skew(-25deg);\n"
      "  -webkit-transform: skew(-25deg);\n"
      "  -moz-transform: scale(0.5);\n"
      "  -webkit-transform: scale(0.5);\n"
      "  -moz-transform: translate(3em, 0);\n"
      "  -webkit-transform: translate(3em, 0);\n"
      "}\n",

      ".shift{-moz-transform:rotate(7deg);-webkit-transform:rotate(7deg);"
      "-moz-transform:skew(-25deg);-webkit-transform:skew(-25deg);"
      "-moz-transform:scale(.5);-webkit-transform:scale(.5);"
      "-moz-transform:translate(3em,0);-webkit-transform:translate(3em,0)}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=5
    // Keep space between trebuchet and ms.
    // TODO(sligocki): Print as font-family:"trebuchet ms" instead.
    // According to the CSS spec:
    //   To avoid mistakes in escaping, it is recommended to quote font
    //   family names that contain white space, digits, or punctuation
    //   characters other than hyphens.
    // It also seems more likely that a bad browser would be more likely
    // to fail to read the escaped space than a proper string.
    { "a { font-family: trebuchet ms; }", "a{font-family:trebuchet\\ ms}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    { "body { font: 2em sans-serif; }", "body{font:2em sans-serif}" },
    { "body { font: 0.75em sans-serif; }", "body{font:.75em sans-serif}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=128
    { "#breadcrumbs ul { list-style-type: none; }",
      "#breadcrumbs ul{list-style-type:none}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=126
    // Extra spaces assure that we actually rewrite the first arg even if
    // font: is expanded by parser.
    { ".menu { font: menu; }               ", ".menu{font:menu}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=211
    { "#some_id {\n"
      "background: #cccccc url(images/picture.png) 50% 50% repeat-x;\n"
      "}\n",

      "#some_id{background:#ccc url(images/picture.png) 50% 50% repeat-x}" },

    { ".gac_od { border-color: -moz-use-text-color #E7E7E7 #E7E7E7 "
      "-moz-use-text-color; }",

      ".gac_od{border-color:-moz-use-text-color #e7e7e7 #e7e7e7 "
      "-moz-use-text-color}" },

    // Star/Underscore hack
    // See: http://developer.yahoo.com/yui/compressor/css.html
    { "a { *padding-bottom: 0px; }",
      "a{*padding-bottom: 0px}" },

    { "#element { width: 1px; _width: 3px; }",
      "#element{width:1px;_width:3px}" },

    // Complex nested functions
    { "body {\n"
      "  background-image:-webkit-gradient(linear, 50% 0%, 50% 100%,"
      " from(rgb(232, 237, 240)), to(rgb(252, 252, 253)));\n"
      "  color: red;\n"
      "}\n"
      ".foo { color: rgba(1, 2, 3, 0.4); }\n",

      "body{background-image:-webkit-gradient(linear,50% 0%,50% 100%,"
      "from(#e8edf0),to(#fcfcfd));color:red}.foo{color:rgba(1,2,3,.4)}" },

    // Counters
    // http://www.w3schools.com/CSS/tryit.asp?filename=trycss_gen_counter-reset
    { "body {counter-reset:section;}\n"
      "h1 {counter-reset:subsection;}\n"
      "h1:before\n"
      "{\n"
      "counter-increment:section;\n"
      "content:\"Section \" counter(section) \". \";\n"
      "}\n"
      "h2:before \n"
      "{\n"
      "counter-increment:subsection;\n"
      "content:counter(section) \".\" counter(subsection) \" \";\n"
      "}\n",

      "body{counter-reset:section}"
      "h1{counter-reset:subsection}"
      "h1:before{counter-increment:section;"
      "content:\"Section \" counter(section) \". \"}"
      "h2:before{counter-increment:subsection;"
      "content:counter(section) \".\" counter(subsection) \" \"}" },

    // Don't lowercase font names.
    { "a { font-family: Arial; }",
      "a{font-family:Arial}" },

    // Don't drop precision on large integers (this is 2^31 + 1 which is
    // just larger than larges z-index accepted by chrome, 2^31 - 1).
    { "#foo { z-index: 2147483649; }",
      // Not "#foo{z-index:2.14748e+09}"
      "#foo{z-index:2147483649}" },

    { "#foo { z-index: 123456789012345678901234567890; }",
      "#foo{z-index:123456789012345678901234567890}" },

    // Don't drop precision on long floating point numbers.
    { ".ad-contain .ad-jump {\n"
      "  color: #000;\n"
      "  font: bold 1.54545455em/0.823529412 \"Benton Sans Bold\", Arial, "
      "Helvetica, sans-serif;   /* 17px / 11px; 14px / 17px */\n"
      "  margin-bottom: 1em;\n"
      "}",

      // Note: we drop the leading 0 from 0.823... but not any of the
      // digits of precision.
      ".ad-contain .ad-jump{color:#000;font:bold 1.54545455em/.823529412 "
      "\"Benton Sans Bold\",Arial,Helvetica,sans-serif;margin-bottom:1em}" },

    // Parse and serialize "\n" correctly as "n" and "\A " correctly as newline.
    // But leave the original string without messing with escaping.
    { "a { content: \"Special chars: \\n\\r\\t\\A \\D \\9\" }",
      "a{content:\"Special chars: \\n\\r\\t\\A \\D \\9\"}" },

    // Test some interesting combinations of @media.
    { "@media screen {"
      "  body { counter-reset:section }"
      "  h1 { counter-reset:subsection }"
      "}"
      "@media screen,printer { a { color:red } }"
      "@media screen,printer { b { color:green } }"
      "@media screen,printer { c { color:blue } }"
      "@media screen         { d { color:black } }"
      "@media screen,printer { e { color:white } }",

      "@media screen{"
      "body{counter-reset:section}"
      "h1{counter-reset:subsection}"
      "}"
      "@media screen,printer{"
      "a{color:red}"
      "b{color:green}"
      "c{color:#00f}"
      "}"
      "@media screen{d{color:#000}}"
      "@media screen,printer{e{color:#fff}}",
    },

    // Charsets
    { "@charset \"UTF-8\";\n"
      "a { color: red }\n",

      "@charset \"UTF-8\";a{color:red}" },

    // Recovered parse errors:
    // http://code.google.com/p/modpagespeed/issues/detail?id=220
    { ".mui-navbar-wrap, .mui-navbar-clone {"
      "opacity:1;-webkit-transform:translateX(0);"
      "-webkit-transition-property:opacity,-webkit-transform;"
      "-webkit-transition-duration:400ms;}",

      ".mui-navbar-wrap,.mui-navbar-clone{"
      "opacity:1;-webkit-transform:translateX(0);"
      "-webkit-transition-property:opacity,-webkit-transform;"
      "-webkit-transition-duration:400ms}" },

    // IE 8 hack \0/.
    { ".gbxms{background-color:#ccc;display:block;position:absolute;"
      "z-index:1;top:-1px;left:-2px;right:-2px;bottom:-2px;opacity:.4;"
      "-moz-border-radius:3px;"
      "filter:progid:DXImageTransform.Microsoft.Blur(pixelradius=5);"
      "*opacity:1;*top:-2px;*left:-5px;*right:5px;*bottom:4px;"
      "-ms-filter:\"progid:DXImageTransform.Microsoft.Blur(pixelradius=5)\";"
      "opacity:1\\0/;top:-4px\\0/;left:-6px\\0/;right:5px\\0/;bottom:4px\\0/}",

      ".gbxms{background-color:#ccc;display:block;position:absolute;"
      "z-index:1;top:-1px;left:-2px;right:-2px;bottom:-2px;opacity:.4;"
      "-moz-border-radius:3px;"
      "filter:progid:DXImageTransform.Microsoft.Blur(pixelradius=5);"
      "*opacity:1;*top:-2px;*left:-5px;*right:5px;*bottom:4px;"
      "-ms-filter:\"progid:DXImageTransform.Microsoft.Blur(pixelradius=5)\";"
      "opacity:1\\0/;top:-4px\\0/;left:-6px\\0/;right:5px\\0/;bottom:4px\\0/}"},

    // Alexa-100 with parse errors (illegal syntax or CSS3).
    // Comma in values
    { ".cnn_html_slideshow_controls > .cnn_html_slideshow_pager_container >"
      " .cnn_html_slideshow_pager > li\n"
      "{\n"
      "  font-size: 16px;\n"
      "  -webkit-transition-property: color, background-color;\n"
      "  -webkit-transition-duration: 0.5s;\n"
      "}\n",

      ".cnn_html_slideshow_controls>.cnn_html_slideshow_pager_container>"
      ".cnn_html_slideshow_pager>li{"
      "font-size:16px;-webkit-transition-property: color, background-color;"
      "-webkit-transition-duration:.5s}" },

    { "a.login,a.home{position:absolute;right:15px;top:15px;display:block;"
      "float:right;height:29px;line-height:27px;font-size:15px;"
      "font-weight:bold;color:rgba(255,255,255,0.7)!important;color:#fff;"
      "text-shadow:0 -1px 0 rgba(0,0,0,0.2);background:#607890;padding:0 12px;"
      "opacity:.9;text-decoration:none;border:1px solid #2e4459;"
      "-moz-border-radius:6px;-webkit-border-radius:6px;border-radius:6px;"
      "-moz-box-shadow:0 1px 0 rgba(255,255,255,0.15),0 1px 0"
      " rgba(255,255,255,0.15) inset;-webkit-box-shadow:0 1px 0 "
      "rgba(255,255,255,0.15),0 1px 0 rgba(255,255,255,0.15) inset;"
      "box-shadow:0 1px 0 rgba(255,255,255,0.15),0 1px 0 "
      "rgba(255,255,255,0.15) inset}",

      // Note: We do not strip leading 0s from 0.15s below because those
      // sections are passed through verbatim rather than being parsed as
      // decimal numbers.
      "a.login,a.home{position:absolute;right:15px;top:15px;display:block;"
      "float:right;height:29px;line-height:27px;font-size:15px;"
      "font-weight:bold;color:rgba(255,255,255,.7)!important;color:#fff;"
      "text-shadow:0 -1px 0 rgba(0,0,0,.2);background:#607890;padding:0 12px;"
      "opacity:.9;text-decoration:none;border:1px solid #2e4459;"
      "-moz-border-radius:6px;-webkit-border-radius:6px;border-radius:6px;"
      "-moz-box-shadow:0 1px 0 rgba(255,255,255,0.15),0 1px 0"
      " rgba(255,255,255,0.15) inset;-webkit-box-shadow:0 1px 0 "
      "rgba(255,255,255,0.15),0 1px 0 rgba(255,255,255,0.15) inset;"
      "box-shadow:0 1px 0 rgba(255,255,255,0.15),0 1px 0 "
      "rgba(255,255,255,0.15) inset}" },

    // Special chars in property
    { ".authorization .mail .login input, .authorization .pswd input {"
      "float: left; width: 100%; font-size: 75%; -moz-box-sizing: border-box; "
      "-webkit-box-sizing: border-box; box-sizing: border-box; height: 21px; "
      "padding: 2px; #height: 13px}\n"
      ".authorization .mail .domain select {float: right; width: 97%; "
      "#width: 88%; font-size: 75%; height: 21px; -moz-box-sizing: border-box; "
      "-webkit-box-sizing: border-box; box-sizing: border-box}\n"
      ".weather_review .main img.attention {position: absolute; z-index: 5; "
      "left: -10px; top: 6px; width: 29px; height: 26px; \n"
      "background: url('http://limg3.imgsmail.ru/r/weather_new/ico_attention."
      "png'); \n"
      "//background-image: none; \n"
      "filter: progid:DXImageTransform.Microsoft.AlphaImageLoader("
      "src=\"http://limg3.imgsmail.ru/r/weather_new/ico_attention.png\", "
      "sizingMethod=\"crop\"); \n"
      "} \n"
      ".rb_body {font-size: 12px; padding: 0 0 0 10px; overflow: hidden; "
      "text-align: left; //display: inline-block;}\n"
      ".rb_h4 {border-bottom: 1px solid #0857A6; color: #0857A6; "
      "font-size: 17px; font-weight: bold; text-decoration: none;}\n",

      ".authorization .mail .login input,.authorization .pswd input{"
      "float:left;width:100%;font-size:75%;-moz-box-sizing:border-box;"
      "-webkit-box-sizing:border-box;box-sizing:border-box;height:21px;"
      "padding:2px;#height: 13px}"
      ".authorization .mail .domain select{float:right;width:97%;"
      "#width: 88%;font-size:75%;height:21px;-moz-box-sizing:border-box;"
      "-webkit-box-sizing:border-box;box-sizing:border-box}"
      ".weather_review .main img.attention{position:absolute;z-index:5;"
      "left:-10px;top:6px;width:29px;height:26px;"
      "background:url(http://limg3.imgsmail.ru/r/weather_new/ico_attention."
      "png);"
      "//background-image: none;"
      "filter: progid:DXImageTransform.Microsoft.AlphaImageLoader("
      "src=\"http://limg3.imgsmail.ru/r/weather_new/ico_attention.png\", "
      "sizingMethod=\"crop\")}"
      ".rb_body{font-size:12px;padding:0 0 0 10px;overflow:hidden;"
      "text-align:left;//display: inline-block}"
      ".rb_h4{border-bottom:1px solid #0857a6;color:#0857a6;"
      "font-size:17px;font-weight:bold;text-decoration:none}" },

    // Expression
    { ".file_manager .loading { _position: absolute;_top: expression(0+((e=doc"
      "ument.documentElement.scrollTop)?e:document.body.scrollTop)+'px'); "
      "color: red; }\n"
      ".connect_widget .page_stream img{max-width:120px;"
      "width:expression(this.width > 120 ? 120:true); color: red; }\n",

      ".file_manager .loading{_position:absolute;_top: expression(0+((e=doc"
      "ument.documentElement.scrollTop)?e:document.body.scrollTop)+'px');"
      "color:red}"
      ".connect_widget .page_stream img{max-width:120px;"
      "width:expression(this.width > 120 ? 120:true);color:red}" },

    // Equals in function
    { ".imdb_lb .header{width:726px;width=728px;height:12px;padding:1px;"
      "border-bottom:1px #000000 solid;background:#eeeeee;font-size:10px;"
      "text-align:left;}"
      ".cboxIE #cboxTopLeft{background:transparent;filter:progid:"
      "DXImageTransform.Microsoft.AlphaImageLoader(src=/images/lb/"
      "internet_explorer/borderTopLeft.png, sizingMethod='scale');}",

      ".imdb_lb .header{width:726px;width=728px;height:12px;padding:1px;"
      "border-bottom:1px #000 solid;background:#eee;font-size:10px;"
      "text-align:left}"
      ".cboxIE #cboxTopLeft{background:transparent;filter:progid:"
      "DXImageTransform.Microsoft.AlphaImageLoader(src=/images/lb/"
      "internet_explorer/borderTopLeft.png, sizingMethod='scale')}" },

    // Special chars in values
    { ".login-form .input-text{ width:144px;padding:6px 3px; "
      "background-color:#fff;background-position:0 -170px;"
      "background-repeat;no-repeat}"
      "td.pop_content .dialog_body{padding:10px;border-bottom:1px# solid #ccc}",

      ".login-form .input-text{width:144px;padding:6px 3px;"
      "background-color:#fff;background-position:0 -170px;"
      "background-repeat;no-repeat}"
      "td.pop_content .dialog_body{padding:10px;border-bottom:1px# solid #ccc}"
    },

    // kSelectorError from Alexa-100
    // Selector list ends in comma
    { ".hp .col ul, {\n"
      "  display: inline !important;\n"
      "  zoom: 1;\n"
      "  vertical-align: top;\n"
      "  margin-left: -10px;\n"
      "  position: relative;\n"
      "}\n",

      ".hp .col ul, {display:inline!important;zoom:1;vertical-align:top;"
      "margin-left:-10px;position:relative}" },

    // Invalid comment type ("//").
    { ".ciuNoteEditBox .topLeft\n"
      "{\n"
      "        background-position:left top;\n"
      "\tbackground-repeat:no-repeat;\n"
      "\tfont-size:4px;\n"
      "\t\n"
      "\t\n"
      "\tpadding: 0px 0px 0px 1px; \n"
      "\t\n"
      "\twidth:7px;\n"
      "}\n"
      "\n"
      "// css hack to make font-size 0px in only ff2.0 and older "
      "(http://pornel.net/firefoxhack)\n"
      ".ciuNoteBox .topLeft,\n"
      ".ciuNoteEditBox .topLeft, x:-moz-any-link {\n"
      "\tfont-size: 0px;\n"
      "}\n",

      ".ciuNoteEditBox .topLeft{background-position:left top;"
      "background-repeat:no-repeat;font-size:4px;padding:0px 0px 0px 1px;"
      "width:7px}// css hack to make font-size 0px in only ff2.0 and older "
      "(http://pornel.net/firefoxhack)\n"
      ".ciuNoteBox .topLeft,\n"
      ".ciuNoteEditBox .topLeft, x:-moz-any-link {font-size:0px}" },

    // Parameters for pseudoclass
    { "/* Opera（＋Firefox、Safari） */\n"
      "body:not(:target) .sh_heading_main_b, body:not(:target) "
      ".sh_heading_main_b_wide{\n"
      "  background:url(\"data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAAoCAYAAAA/tpB3AAAAQ0lEQVR42k3EMQLAIAg"
      "EMP//WkRQVMB2YLgMae/XMhOLCMzdq3svds7B9t6VmWFrLWzOWakqJiLYGKNiZqz3jh"
      "HR+wBZbpvd95zR6QAAAABJRU5ErkJggg==\") repeat-x left top;\n"
      "}\n"
      "/* Firefox（＋Google Chrome2） */\n"
      "html:not([lang*=""]) .sh_heading_main_b,\n"
      "html:not([lang*=""]) .sh_heading_main_b_wide{\n"
      "\t/* For Mozilla/Gecko (Firefox etc) */\n"
      "\tbackground:-moz-linear-gradient(top, #FFFFFF, #F0F0F0);\n"
      "\t/* For WebKit (Safari, Google Chrome etc) */\n"
      "\tbackground:-webkit-gradient(linear, left top, left bottom, "
      "from(#FFFFFF), to(#F0F0F0));\n"
      "}\n"
      "/* Safari */\n"
      "html:not(:only-child:only-child) .sh_heading_main_b,\n"
      "html:not(:only-child:only-child) .sh_heading_main_b_wide{\n"
      "\t/* For WebKit (Safari, Google Chrome etc) */\n"
      "\tbackground: -webkit-gradient(linear, left top, left bottom, "
      "from(#FFFFFF), to(#F0F0F0));\n"
      "}\n",

      "body:not(:target) .sh_heading_main_b, body:not(:target) "
      ".sh_heading_main_b_wide{background:url(data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAAoCAYAAAA/tpB3AAAAQ0lEQVR42k3EMQLAIAg"
      "EMP//WkRQVMB2YLgMae/XMhOLCMzdq3svds7B9t6VmWFrLWzOWakqJiLYGKNiZqz3jh"
      "HR+wBZbpvd95zR6QAAAABJRU5ErkJggg==) repeat-x left top}"
      "html:not([lang*=""]) .sh_heading_main_b,\n"
      "html:not([lang*=""]) .sh_heading_main_b_wide{"
      "background:-moz-linear-gradient(top,#fff,#f0f0f0);"
      "background:-webkit-gradient(linear,left top,left bottom,"
      "from(#fff),to(#f0f0f0))}"
      "html:not(:only-child:only-child) .sh_heading_main_b,\n"
      "html:not(:only-child:only-child) .sh_heading_main_b_wide{"
      "background:-webkit-gradient(linear,left top,left bottom,"
      "from(#fff),to(#f0f0f0))}" },

    // @import stuff
    { "@import \"styles.css\"foo; a { color: red; }",
      "@import url(styles.css) foo;a{color:red}" },

    // @media with no contents
    { "@media; a { color: red; }", "a{color:red}" },
    { "@media screen, print; a { color: red; }", "a{color:red}" },

    // Unexpected @-statements
    { "@-webkit-keyframes wiggle {\n"
      "  0% {-webkit-transform:rotate(6deg);}\n"
      "  50% {-webkit-transform:rotate(-6deg);}\n"
      "  100% {-webkit-transform:rotate(6deg);}\n"
      "}\n"
      "@-moz-keyframes wiggle {\n"
      "  0% {-moz-transform:rotate(6deg);}\n"
      "  50% {-moz-transform:rotate(-6deg);}\n"
      "  100% {-moz-transform:rotate(6deg);}\n"
      "}\n"
      "@keyframes wiggle {\n"
      "  0% {transform:rotate(6deg);}\n"
      "  50% {transform:rotate(-6deg);}\n"
      "  100% {transform:rotate(6deg);}\n"
      "}\n",

      // Rewritten version only has newlines stripped between @-rules.
      "@-webkit-keyframes wiggle {\n"
      "  0% {-webkit-transform:rotate(6deg);}\n"
      "  50% {-webkit-transform:rotate(-6deg);}\n"
      "  100% {-webkit-transform:rotate(6deg);}\n"
      "}"
      "@-moz-keyframes wiggle {\n"
      "  0% {-moz-transform:rotate(6deg);}\n"
      "  50% {-moz-transform:rotate(-6deg);}\n"
      "  100% {-moz-transform:rotate(6deg);}\n"
      "}"
      "@keyframes wiggle {\n"
      "  0% {transform:rotate(6deg);}\n"
      "  50% {transform:rotate(-6deg);}\n"
      "  100% {transform:rotate(6deg);}\n"
      "}" },

    { "@font-face{font-family:'Ubuntu';font-style:normal;font-weight:normal;"
      "src:local('Ubuntu'), url('http://themes.googleusercontent.com/static/"
      "fonts/ubuntu/v2/2Q-AW1e_taO6pHwMXcXW5w.ttf') format('truetype')}"
      "@font-face{font-family:'Ubuntu';font-style:normal;font-weight:bold;"
      "src:local('Ubuntu Bold'), local('Ubuntu-Bold'), url('http://themes."
      "googleusercontent.com/static/fonts/ubuntu/v2/0ihfXUL2emPh0ROJezvraKCWc"
      "ynf_cDxXwCLxiixG1c.ttf') format('truetype')}",

      "@font-face{font-family:'Ubuntu';font-style:normal;font-weight:normal;"
      "src:local('Ubuntu'), url('http://themes.googleusercontent.com/static/"
      "fonts/ubuntu/v2/2Q-AW1e_taO6pHwMXcXW5w.ttf') format('truetype')}"
      "@font-face{font-family:'Ubuntu';font-style:normal;font-weight:bold;"
      "src:local('Ubuntu Bold'), local('Ubuntu-Bold'), url('http://themes."
      "googleusercontent.com/static/fonts/ubuntu/v2/0ihfXUL2emPh0ROJezvraKCWc"
      "ynf_cDxXwCLxiixG1c.ttf') format('truetype')}" },

    // CSS3 media queries.
    // http://code.google.com/p/modpagespeed/issues/detail?id=50
    { "@media only screen and (min-device-width: 320px) and"
      " (max-device-width: 480px) {\n"
      "        body {"
      "                padding: 0;\n"
      "        }\n"
      "        #page {\n"
      "                margin-top: 0;\n"
      "        }\n"
      "        #branding {\n"
      "                border-top: none;\n"
      "        }\n"
      "\n"
      "}\n",

      "@media only screen and (min-device-width:320px) and (max-device-width:"
      "480px){body{padding:0}#page{margin-top:0}#branding{border-top:none}}" },

    // Make sure we distinguish similar media queries.
    { "@media screen { .a { color: red; } }\n"
      "@media screen and (color) { .b { color: green; } }\n"
      "@media not screen { .c { color: blue; } }\n"
      "@media only screen { .d { color: cyan; } }\n",

      "@media screen{.a{color:red}}"
      "@media screen and (color){.b{color:green}}"
      "@media not screen{.c{color:#00f}}"
      "@media only screen{.d{color:#0ff}}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=575
    { "[class^=\"icon-\"],[class*=\" icon-\"] { color: red }",
      "[class^=\"icon-\"],[class*=\" icon-\"]{color:red}" },

    // Don't "fix" quirks-mode colors.
    // Note: DECAFB is not converted to a more correct #decafb.
    { "body { color: DECAFB }", "body{color: DECAFB }" },

    { "#post_content, #post_content p{\n"
      " font-family:Helvetica;\n"
      " font-size:16px;\n"
      " color:333;\n"
      " line-height:24px;\n"
      " margin-bottom:15px;\n"
      "}",

      // Note: 333 is not converted to a more correct #333.
      "#post_content,#post_content p{font-family:Helvetica;font-size:16px;"
      "color:333;line-height:24px;margin-bottom:15px}" },

    // Properly deal with space in URL.
    { "#ac { background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgA"
      "AAG4AAAAfCAA AAAAjTqdDAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZS\") no-repeat; }",

      // Note we unquote the url() and escape the space with a backslash.
      // It's fine if we choose a different strategy in the future. We just
      // need to make sure that the space is not left verbatim in the
      // unquoted url().
      "#ac{background:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgA"
      "AAG4AAAAfCAA\\ AAAAjTqdDAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZS) no-repeat}" },

    // Noticed from YUI minification.
    // https://code.google.com/p/modpagespeed/issues/detail?id=614
    { "td { line-height: 0.8em; margin: -0.9in; }",
      "td{line-height:.8em;margin:-.9in}" },

    // ::- in selectors
    { "::-moz-selection {background: #f36921 ; color: #fff ; text-shadow:none}",
      "::-moz-selection{background:#f36921;color:#fff;text-shadow:none}" },

    // Sloppy color syntax
    // Note that according to the CSS spec: 0000ff should be parsed as
    // DIM(0, ff) and then serialized as 0ff, but browsers will parse both
    // of these values as quirks-mode colors and thus we would be changing
    // the links from blue to cyan.
    // https://code.google.com/p/modpagespeed/issues/detail?id=639
    { "A:link, A:visited { color: 0000ff }",
      "A:link,A:visited{color: 0000ff }" },

    // Unexpected ! uses in declarations.
    { ".filehistory a img,#file img:hover{background:white url(data:image/png;"
      "base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAAGElEQVQYV2N4DwX/"
      "oYBhgARgDJjEAAkAAEC99wFuu0VFAAAAAElFTkSuQmCC) repeat;"
      "background:white url(http://static.uncyc.org/skins/common/images/Checke"
      "r-16x16.png?2012-02-15T12:25:00Z) repeat!ie}",

      ".filehistory a img,#file img:hover{background:#fff url(data:image/png;"
      "base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAAGElEQVQYV2N4DwX/"
      "oYBhgARgDJjEAAkAAEC99wFuu0VFAAAAAElFTkSuQmCC) repeat;"
      "background:white url(http://static.uncyc.org/skins/common/images/Checke"
      "r-16x16.png?2012-02-15T12:25:00Z) repeat!ie}" },

    { "body{font:13px/1.231,clean;*font-size:small;*font:x-small;"
      "font-family:Arial !important}",

      "body{font:13px/1.231,clean;*font-size:small;*font:x-small;"
      "font-family:Arial!important}" },

    // https://code.google.com/p/modpagespeed/issues/detail?id=722
    { ".a { color: red; }\n"
      "@import url('foo.css');\n"
      ".b { color: blue; }\n",

      ".a{color:red}@import url('foo.css');.b{color:#00f}" },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    GoogleString id = StringPrintf("complex_css%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1], kExpectSuccess);
  }

  const char* parse_fail_examples[] = {
    // Bad syntax
    "}}",
    "@foobar this is totally wrong CSS syntax }",
    "@media (color) and screen { .a { color: red; } }",

    // Do not "fix" by putting a space between 'and' and '('.
    "@media only screen and(-webkit-min-device-pixel-ratio:1.5),"
    "only screen and(min--moz-device-pixel-ratio:1.5),"
    "only screen and(min-resolution:240dpi){"
    ".ui-icon-plus,.ui-icon-minus,.ui-icon-delete,.ui-icon-arrow-r,"
    ".ui-icon-arrow-l,.ui-icon-arrow-u,.ui-icon-arrow-d,.ui-icon-check,"
    ".ui-icon-gear,.ui-icon-refresh,.ui-icon-forward,.ui-icon-back,"
    ".ui-icon-grid,.ui-icon-star,.ui-icon-alert,.ui-icon-info,.ui-icon-home,"
    ".ui-icon-search,.ui-icon-searchfield:after,.ui-icon-checkbox-off,"
    ".ui-icon-checkbox-on,.ui-icon-radio-off,.ui-icon-radio-on{"
    "background-image:url(images/icons-36-white.png);"
    "-moz-background-size:776px 18px;-o-background-size:776px 18px;"
    "-webkit-background-size:776px 18px;background-size:776px 18px;}"
    ".ui-icon-alt{background-image:url(images/icons-36-black.png);}}",

    // Things discovered in the wild by shanemc:

    // No space between "and" and "(".
    "@media all and(-webkit-max-device-pixel-ratio:10000),\n"
    "   not all and(-webkit-min-device-pixel-ratio:0) {\n"
    "\n"
    "\t:root .RadTreeView_rtl .rtPlus,\n"
    "\t:root .RadTreeView_rtl .rtMinus\n"
    "\t{\n"
    "\t\tposition: relative;\n"
    "\t\tmargin-left: 2px;\n"
    "\t\tmargin-right: -13px;\n"
    "\t\tright: -15px;\n"
    "\t}\n"
    "}\n",

    // Ignoring chars at end of function.
    "* html #profilePhoto {\n"
    "  height: expression((this.flag == undefined) ? (this.scrollHeight > 500) "
    "? this.flag = '500px' : 'auto' : '');\n"
    "}",

    // Zero width space <U+200B> = \xE2\x80\x8B
    // http://zenpencils.com/wp-content/plugins/zpcustomselectmenu/ddSlick.css
    "   .dd-container { \n"
    "           position:relative; \n"
    "           margin:0px;\n"
    "   }\xE2\x80\x8B \n"
    "   .dd-selected-text { \n"
    "           font-weight:bold;\n"
    "           cursor:pointer !important;\n"
    "   }\xE2\x80\x8B",

    // Nonsense: (rgba(...)) (last line).
    // From http://yandex.st/www/1.473/touch-bem/pages-touch/index/_index.css
    ".b-search__button{position:relative;min-width:50px;margin:0 0 0 -1px;"
    "padding:1px 0 1px 1px;-webkit-border-top-left-radius:2px;"
    "border-top-left-radius:2px;-webkit-border-bottom-left-radius:2px;"
    "border-bottom-left-radius:2px;background:-webkit-gradient(linear,0 0,"
    "0 100%,from(rgba(192,192,192,.6)),to(rgba(49,49,49,.3)));"
    "background:-o-linear-gradient(top,(rgba(192,192,192,.6)),"
    "(rgba(49,49,49,.3)))}",

    // !important in selectors.
    // From http://mstatic.allegrostatic.pl/allegro/touch/css/style.min.css
    "-moz-appearance:none!important;html{height:100%}",
    "-moz-foo { -webkit-bar: -ie-quuz }",
  };

  for (int i = 0; i < arraysize(parse_fail_examples); ++i) {
    GoogleString id = StringPrintf("complex_css_parse_fail%d", i);
    ValidateFailParse(id, parse_fail_examples[i]);
  }
}

// Most tests are run with set_always_rewrite_css(true),
// but all production use has set_always_rewrite_css(false).
// This test makes sure that setting to false still does what we intend.
TEST_F(CssFilterTest, NoAlwaysRewriteCss) {
  // When we force always_rewrite_css, we can expand some statements.
  // Note: when this example is fixed in the minifier, this test will break :/
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(true);
  server_context()->ComputeSignature(options());
  ValidateRewrite("expanding_example",
                  "@import url(http://www.example.com)",
                  "@import url(http://www.example.com) ;",
                  kExpectSuccess);

  // With it set false, we do not expand CSS (as long as we didn't do anything
  // else, like rewrite sub-resources.
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(false);
  server_context()->ComputeSignature(options());
  ValidateRewrite("non_expanding_example",
                  "@import url(http://www.example.com)",
                  "@import url(http://www.example.com)",
                  kExpectNoChange);

  // When we force always_rewrite_css, we allow rewriting something to nothing.
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(true);
  server_context()->ComputeSignature(options());
  ValidateRewrite("contracting_example", "  ", "", kExpectSuccess);

  // We still contract it with set_always_rewrite_css(false).
  // Note: In the past we did not allow rewrites that resulted in empty output.
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(false);
  server_context()->ComputeSignature(options());
  ValidateRewrite("contracting_example2", "  ", "", kExpectSuccess);
}

TEST_F(CssFilterTest, RemoveComments) {
  ValidateRewrite("remove_comments",
                  " /* This comment will be removed. */ ", "", kExpectSuccess);
}

TEST_F(CssFilterTest, NoQuirksModeFixes) {
  const char in_css[]  = "body {color:DECAFB}";
  const char out_css[] = "body{color:DECAFB}";

  // Test that we don't parse the CSS in quirks-mode in HTML.
  ValidateRewrite("quirks_html", in_css, out_css, kExpectSuccess);

  // Test that the same thing happens in XHTML.
  SetDoctype(kXhtmlDtd);
  ValidateRewrite("quirks_xhtml", in_css, out_css,
                  kExpectSuccess | kNoStatCheck);
  // Note: We must use kNoStatCheck, because this will use the cached result
  // from the HTML case and thus not record accurate savings.
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_F(CssFilterTest, RetainExtraHeaders) {
  GoogleString url = StrCat(kTestDomain, "retain.css");
  SetResponseWithDefaultHeaders(url, kContentTypeCss, kInputStyle, 300);
  TestRetainExtraHeaders("retain.css", "cf", "css");
}

TEST_F(CssFilterTest, RewriteStyleAttribute) {
  // Test that nothing happens if rewriting is disabled (default).
  ValidateNoChanges("RewriteStyleAttribute",
                    "<div style='background-color: #f00; color: yellow;'/>");

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  // Test no rewriting.
  ValidateNoChanges("no-rewriting",
                    "<div style='background-color:red;color:#ff0'/>");

  // Test successful rewriting.
  ValidateExpected("rewrite-simple",
                   "<div style='background-color: #f00; color: yellow;'/>",
                   "<div style='background-color:red;color:#ff0'/>");
  // Rewritten elements with style attributes don't have any log entries.
  EXPECT_EQ(0, rewrite_driver()->request_context()->log_record()->
            logging_info()->rewriter_info().size());


  SetFetchResponse404("404.css");
  static const char kMixedInput[] =
      "<div style=\""
      "  background-image: url('images/watch-icons.png?1');\n"
      "  background-position: -19px 60%;\""
      ">\n"
      "<link rel=stylesheet href='404.css'>\n"
      "<span style=\"font-family: Verdana\">Verdana</span>\n"
      "</div>";
  static const char kMixedOutput[] =
      "<div style=\""
      "background-image:url(images/watch-icons.png?1);"
      "background-position:-19px 60%\""
      ">\n"
      "<link rel=stylesheet href='404.css'>\n"
      "<span style=\"font-family:Verdana\">Verdana</span>\n"
      "</div>";
  ValidateExpected("rewrite-mixed", kMixedInput, kMixedOutput);

  // Test that nothing happens if we have a style attribute on a style element,
  // which is actually invalid.
  ValidateNoChanges("rewrite-style-with-style",
                   "<style style='background-color: #f00; color: yellow;'/>");
}

TEST_F(CssFilterTest, RewriteStyleAttributeDifferentDirsNoUrl) {
  // Make sure we don't pointlessly produce different cache keys for
  // attribute CSS w/o URLs in different directories.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  const char kCss[] = "color : yellow;  ";
  const char kMinCss[] = "color:#ff0";

  ValidateExpected("main_page",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  ValidateExpected("subdir/file",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  EXPECT_EQ(1, statistics()->GetVariable(CssFilter::kBlocksRewritten)->Get());
}

TEST_F(CssFilterTest, RewriteStyleAttributeDifferentDirsAbsUrl) {
  // Make sure we don't pointlessly produce different cache keys for
  // attribute CSS with same absolute URLs in different directories.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  const char kCss[] = "background : url(http://example.com/artful.png); ";
  const char kMinCss[] = "background:url(http://example.com/artful.png)";

  ValidateExpected("main_page",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  ValidateExpected("subdir/file",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  EXPECT_EQ(1, statistics()->GetVariable(CssFilter::kBlocksRewritten)->Get());
}

TEST_F(CssFilterTest, RewriteStyleAttributeDifferentDirsRelUrl) {
  // When URLs inside the inline CSS are relative (and resolve to different
  // bases) we should get 2 separate rewrites.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  const char kCss[] = "background : url(artful.png); ";
  const char kMinCss[] = "background:url(artful.png)";

  ValidateExpected("main_page",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  ValidateExpected("subdir/file",
                   StrCat("<div style='", kCss, "'/>"),
                   StrCat("<div style='", kMinCss, "'/>"));

  EXPECT_EQ(2, statistics()->GetVariable(CssFilter::kBlocksRewritten)->Get());
}

TEST_F(CssFilterTest, DontAbsolutifyCssImportUrls) {
  // Since we are not using a proxy URL namer (TestUrlNamer) nor any
  // domain rewriting/sharding, we expect the relative URLs in
  // the @import's to be passed though untouched.
  const char styles_filename[] = "styles.css";
  const char styles_css[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";
  const GoogleString css_in = StrCat(
      "@import url(media/print.css) print;",
      "@import url(media/screen.css) screen;",
      styles_css);
  SetResponseWithDefaultHeaders(styles_filename, kContentTypeCss, css_in, 100);

  static const char html_prefix[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Style starts here -->\n"
      "  <style type='text/css'>";
  static const char html_suffix[] = "</style>\n"
      "  <!-- Style ends here -->\n"
      "</head>";

  GoogleString html = StrCat(html_prefix, css_in,  html_suffix);

  ValidateNoChanges("dont_absolutify_css_import_urls", html);
}

TEST_F(CssFilterTest, DontAbsolutifyEmptyUrl) {
  // Ensure that an empty URL is left as-is and is not absolutified.
  const char kEmptyUrlRule[] = "#gallery { list-style: none outside url(''); }";
  const char kNoUrlRule[] = "#gallery{list-style:none outside url()}";
  ValidateRewrite("empty_url_in_rule", kEmptyUrlRule, kNoUrlRule,
                  kExpectSuccess);

  const char kEmptyUrlImport[] = "@import url('');";
  const char kNoUrlImport[] = "@import url() ;";
  ValidateRewrite("empty_url_in_import", kEmptyUrlImport, kNoUrlImport,
                  kExpectSuccess);
}

TEST_F(CssFilterTest, WebpRewriting) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebp);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpLaRewriting) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpLaWithFlagRewriting) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, NoWebpRewritingFromJpgIfDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebp);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.jpg",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpRewritingFromJpgWithWebpFlagWebpLaUa) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpRewritingFromJpgWithWebpFlagWebpUa) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebp);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, NoWebpLaRewritingFromJpgIfDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kPuzzleJpgFile, kContentTypeJpeg,
                    "x%s.pagespeed.ic.0.jpg",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, NoWebpRewritingFromPngIfDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebp);

  TestWebpRewriting(kBikePngFile, kContentTypePng,
                    "x%s.pagespeed.ic.0.png",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpRewritingFromPngWithWebpFlagWebpLaUa) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kBikePngFile, kContentTypePng,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, WebpRewritingFromPngWithWebpFlagWebpUa) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebp);

  TestWebpRewriting(kBikePngFile, kContentTypePng,
                    "x%s.pagespeed.ic.0.webp",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, NoWebpLaRewritingFromPngIfDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent(kUaWebpLossless);

  TestWebpRewriting(kBikePngFile, kContentTypePng,
                    "x%s.pagespeed.ic.0.png",
                    "A.%s.pagespeed.cf.0.css");
}

TEST_F(CssFilterTest, DontAbsolutifyUrlsIfNoDomainMapping) {
  // We are not using a proxy URL namer (TestUrlNamer) nor any domain
  // rewriting/sharding, so relative URLs can stay relative.
  // Note: the CSS with multiple urls is valid CSS3 but not valid CSS2.1.
  const char css_input[] =
      "body{background:url(a.png)}"
      "body{background: url(a.png), url( http://test.com/b.png ), "
      "url('sub/c.png'), url( \"/sub/d.png\"  )}";
  // with image rewriting
  TestUrlAbsolutification("dont_absolutify_unparseable_urls_etc_with",
                          css_input, css_input,
                          true  /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("dont_absolutify_unparseable_urls_etc_without",
                          css_input, css_input,
                          true  /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTest, AbsolutifyUnparseableUrlsWithDomainMapping) {
  // We are not using a proxy URL namer (TestUrlNamer) but we ARE mapping and
  // sharding domains, so we expect the relative URLs to be absolutified.
  // Note: the CSS with multiple urls is valid CSS3 but not valid CSS2.1.
  const char css_input[] =
      "body{background:url(a.png)}"
      "body{background: url(a.png), url( http://test.com/b.png ), "
      "url('sub/c.png'), url( \"/sub/d.png\"  )}";
  const char css_output[] =
      "body{background:url(http://cdn2.com/a.png)}"
      "body{background: url(http://cdn2.com/a.png), "
      "url(http://cdn1.com/b.png), "
      "url('http://cdn1.com/sub/c.png'), "
      "url(\"http://cdn2.com/sub/d.png\")}";
  // with image rewriting
  TestUrlAbsolutification("absolutify_unparseable_urls_etc_with",
                          css_input, css_output,
                          true  /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          true  /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("absolutify_unparseable_urls_etc_without",
                          css_input, css_output,
                          true  /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          true  /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTest, DontAbsolutifyCursorUrlsWithoutDomainMapping) {
  // Ensure that cursor URLs are left alone when there's nothing to do.
  const char css_input[] =
      ":link,:visited { cursor: url(example.svg) pointer }";
  const char expected_output[] =
      ":link,:visited{cursor:url(example.svg) pointer}";
  // with image rewriting
  TestUrlAbsolutification("dont_absolutify_cursor_urls_etc_with",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("dont_absolutify_cursor_urls_etc_without",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTest, AbsolutifyCursorUrlsWithDomainMapping) {
  // Ensure that cursor URLs are correctly absolutified.
  const char css_input[] =
      ":link,:visited { cursor: url(example.svg) pointer }";
  const char expected_output[] =
      ":link,:visited{cursor:url(http://cdn2.com/example.svg) pointer}";
  TestUrlAbsolutification("absolutify_cursor_urls_with_domain_mapping",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          false /* enable_proxy_mode */,
                          true  /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTest, SimpleFetch) {
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "style.css"),
                                kContentTypeCss, kInputStyle, 100);
  GoogleString output;
  ASSERT_TRUE(FetchResourceUrl(
      Encode(kTestDomain, "cf", "0", "style.css", "css"), &output));
  EXPECT_EQ(kOutputStyle, output);
}

TEST_F(CssFilterTest, SimpleFetchUnhealthy) {
  lru_cache()->set_is_healthy(false);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "style.css"),
                                kContentTypeCss, kInputStyle, 100);
  GoogleString output;
  ASSERT_TRUE(FetchResourceUrl(
      Encode(kTestDomain, "cf", "0", "style.css", "css"), &output));
  EXPECT_EQ(kOutputStyle, output);
}

// Make sure we correctly decode the previously unexpected I.. format.
// http://code.google.com/p/modpagespeed/issues/detail?id=427
TEST_F(CssFilterTest, EmptyLeafFetch) {
  // CSS URL ends in /
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "style/"),
                                kContentTypeCss, kInputStyle, 100);

  GoogleString output;
  // Note: We intentionally do not use Encode() to make this test as explicit
  // as possible. We just want to test that we correctly deal with the
  // unexpected I.. format. EmptyLeafFull tests the full flow and thus
  // will continue to test the right thing if the encoding changes.
  ASSERT_TRUE(FetchResourceUrl(
      StrCat(kTestDomain, "style/I..pagespeed.cf.Hash.css"), &output));
  EXPECT_EQ(kOutputStyle, output);
}

// Make sure we correctly rewrite, encode and decode a CSS URL with empty leaf.
// http://code.google.com/p/modpagespeed/issues/detail?id=427
TEST_F(CssFilterTest, EmptyLeafFull) {
  // CSS URL ends in /
  ValidateRewriteExternalCssUrl("empty_leaf", StrCat(kTestDomain, "style/"),
                                kInputStyle, kOutputStyle, kExpectSuccess);
}

TEST_F(CssFilterTest, UnauthorizedCssResource) {
  ValidateRewriteExternalCssUrl("unauth", "http://unauth.example.com/style.css",
                                kInputStyle, kInputStyle, kExpectNoChange);
}

TEST_F(CssFilterTest, FlushInInlineCss) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html><body><style>.a { co");
  // Flush in middle of inline CSS.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("lor: red; }</style></body></html>");
  rewrite_driver()->FinishParse();

  // Expect text to be rewritten because it is coalesced.
  // HtmlParse will send events like this to filter:
  //   StartElement style
  //   Flush
  //   Characters ...
  //   EndElement style
  EXPECT_EQ("<html><body><style>.a{color:red}</style></body></html>",
            output_buffer_);
  // Inlined rewritten css don't have any log entries.
  EXPECT_EQ(0, rewrite_driver()->request_context()->log_record()->
            logging_info()->rewriter_info().size());
}

TEST_F(CssFilterTest, InlineCssWithExternalUrlAndDelayCache) {
  GoogleString img_url = StrCat(kTestDomain, "a.jpg");
  AddFileToMockFetcher(img_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  server_context()->ComputeSignature(options());

  // Delay the http cache lookup for the image so that it is not rewritten.
  delay_cache()->DelayKey(HttpCacheKey(img_url));

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<html><body>"
      "<style>body{background:url(a.jpg)}</style>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("</body></html>");
  delay_cache()->ReleaseKey(HttpCacheKey(img_url));
  rewrite_driver()->FinishParse();

  EXPECT_EQ("<html><body><style>body{background:url(a.jpg)}</style>"
            "</body></html>", output_buffer_);
  // There was previously a bug where we were logging this as a successful
  // application of the css filter. Make sure that this case isn't logged.
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_F(CssFilterTest, FlushInEndTag) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html><body><style>.a { color: red; }</st");
  // Flush in middle of closing </style> tag.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("yle></body></html>");
  rewrite_driver()->FinishParse();

  // Expect text to be rewritten because it is coalesced.
  // HtmlParse will send events like this to filter:
  //   StartElement style
  //   Characters ...
  //   Flush
  //   EndElement style
  EXPECT_EQ("<html><body><style>.a{color:red}</style></body></html>",
            output_buffer_);
}

// See: http://www.alistapart.com/articles/alternate/
//  and http://www.w3.org/TR/html4/present/styles.html#h-14.3.1
TEST_F(CssFilterTest, AlternateStylesheet) {
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss, kInputStyle, 100);

  const char html_format[] = "<link rel='%s' href='%s' title='foo'>";
  const GoogleString new_url = Encode("", "cf", "0", "foo.css", "css");

  ValidateExpected("preferred_stylesheet",
                   StringPrintf(html_format, "stylesheet", "foo.css"),
                   StringPrintf(html_format, "stylesheet", new_url.c_str()));

  ValidateExpected("alternate_stylesheet",
                   StringPrintf(html_format, "alternate stylesheet", "foo.css"),
                   StringPrintf(html_format, "alternate stylesheet",
                                new_url.c_str()));

  ValidateExpected("alternate_stylesheet2",
                   StringPrintf(html_format, " StyleSheet alterNATE  ",
                                "foo.css"),
                   StringPrintf(html_format, " StyleSheet alterNATE  ",
                                new_url.c_str()));

  ValidateExpected("alternate_stylesheet_and_more",
                   StringPrintf(html_format, "  foo stylesheet alternate bar ",
                                "foo.css"),
                   StringPrintf(html_format, "  foo stylesheet alternate bar ",
                                new_url.c_str()));

  ValidateNoChanges("alternate_not_stylesheet",
                    StringPrintf(html_format, "alternate snowflake",
                                 "foo.css"));
}

class CssFilterTestUrlNamer : public CssFilterTest {
 public:
  CssFilterTestUrlNamer() {
    // We need a subclass to do this because of the timing of construction
    // and SetUp calls, and doing it after all that doesn't inject it in all
    // right places.
    SetUseTestUrlNamer(true);
  }
};

TEST_F(CssFilterTestUrlNamer, AbsolutifyUnparseableUrls) {
  // Here we ARE using a proxy URL namer (TestUrlNamer) so the URLs in
  // unparseable CSS must be absolutified.
  // This CSS is valid CSS3 but not valid CSS2.1 because of the multiple urls.
  const char css_input[] =
      "body { background: url(a.png), url( http://test.com/b.png ), "
      "url('sub/c.png'), url( \"/sub/d.png\"  ); }\n";
  const char expected_output[] =
      "body{background: "
      "url(http://test.com/a.png), "
      "url( http://test.com/b.png ), "  // already absolute means no change
      "url('http://test.com/sub/c.png'), "
      "url(\"http://test.com/sub/d.png\")}";
  // with image rewriting
  TestUrlAbsolutification("absolutify_unparseable_urls_with",
                          css_input, expected_output,
                          true  /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("do_absolutify_unparseable_urls_without",
                          css_input, expected_output,
                          true  /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTestUrlNamer, AbsolutifyParseableUrls) {
  // Here we are using a proxy URL namer (TestUrlNamer) but the URLs in the
  // CSS isn't rewritten by the image rewriter, but we still must absolutify.
  const char css_input[] =
      "body { background: url(a.png); }\n";
  const char expected_output[] =
      "body{background:url(http://test.com/a.png)}";
  // with image rewriting
  TestUrlAbsolutification("absolutify_parseable_urls_with",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("absolutify_parseable_urls_without",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTestUrlNamer, AbsolutifyOtherUrlsWithProxy) {
  // Ensure that non-rewritten URLs are correctly absolutified.
  const char css_input[] =
      ":link,:visited { cursor: url(example.svg) pointer }\n"
      ".png .itab_prev { behavior: url(/js/iepngfix.htc) }\n"
      ".foo { bar: url('baz.ext'); }";
  const char expected_output[] =
      ":link,:visited{cursor:url(http://test.com/example.svg) pointer}"
      ".png .itab_prev{behavior:url(http://test.com/js/iepngfix.htc)}"
      ".foo{bar:url(http://test.com/baz.ext)}";
  TestUrlAbsolutification("absolutify_other_urls_with_proxy",
                          css_input, expected_output,
                          false /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

TEST_F(CssFilterTestUrlNamer, AbsolutifyWithBom) {
  // We ARE using a proxy URL namer (TestUrlNamer) so the URLs in unparseable
  // CSS must be absolutified. The CSS is unparseable because of the BOM.
  const char css_input[] =
      "\xEF\xBB\xBF"
      "@import url(x.ss);\n"
      "body { background: url(a.png); }\n";
  const char expected_output[] =
      "\xEF\xBB\xBF"
      "@import url(http://test.com/x.ss) ;"
      "body{background:url(http://test.com/a.png)}";
  // with image rewriting
  TestUrlAbsolutification("absolutify_with_bom_with",
                          css_input, expected_output,
                          true  /* expect_unparseable_section */,
                          true  /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
  // without image rewriting
  TestUrlAbsolutification("do_absolutify_with_bom_without",
                          css_input, expected_output,
                          true  /* expect_unparseable_section */,
                          false /* enable_image_rewriting */,
                          true  /* enable_proxy_mode */,
                          false /* enable_mapping_and_sharding */);
}

}  // namespace

}  // namespace net_instaweb
