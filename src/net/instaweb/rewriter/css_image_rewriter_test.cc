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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_image_rewriter.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

const char kImageData[] = "Invalid PNG but it does not matter for this test";

class CssImageRewriterTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options_.EnableFilter(RewriteOptions::kExtendCache);
    CssRewriteTestBase::SetUp();
  }
};

TEST_F(CssImageRewriterTest, CacheExtendsImages) {
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("bar.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("baz.png", kContentTypePng, kImageData, 100);

  static const char before[] =
      "<head><style>"
      "body {\n"
      "  background-image: url(foo.png);\n"
      "  list-style-image: url('bar.png');\n"
      "}\n"
      ".titlebar p.cfoo, #end p {\n"
      "  background: url(\"baz.png\");\n"
      "  list-style: url('foo.png');\n"
      "}\n"
      ".other {\n"
      "  background-image:url(data:image/;base64,T0sgYjAxZGJhYTZmM2Y1NTYyMQ==);"
      "  -proprietary-background-property: url(foo.png);\n"
      "}\n"
      "</style></head>";
  static const char after[] =
      "<head><style>"
      "body{background-image:url(http://test.com/foo.png.pagespeed.ce.0.png);"
      "list-style-image:url(http://test.com/bar.png.pagespeed.ce.0.png)}"
      ".titlebar p.cfoo,#end p{"
      "background:url(http://test.com/baz.png.pagespeed.ce.0.png);"
      "list-style:url(http://test.com/foo.png.pagespeed.ce.0.png)}"
      ".other{"  // data: URLs and unknown properties are not rewritten.
      "background-image:url(data:image/;base64\\,T0sgYjAxZGJhYTZmM2Y1NTYyMQ==);"
      "-proprietary-background-property:url(foo.png)}"
      "</style></head>";

  ValidateExpected("cache_extends_images", before, after);
}


// Note that these values of "10" and "20" are very tight.  This is a
// feature.  It serves as an early warning system because extra cache
// lookups will induce time-advancement from
// MemFileSystem::UpdateAtime, which can make these resources expire
// before they are used.  So if you find tests in this module failing
// unexpectedly, you may be tempted to bump up these values.  Don't.
// Figure out how to make fewer cache lookups.
static const int kMinExpirationTimeMs = 10 * Timer::kSecondMs;
static const int kExpireAPngSec = 10;
static const int kExpireBPngSec = 20;

// These tests are to make sure our TTL considers that of subresources.
class CssFilterSubresourceTest : public CssRewriteTestBase {
 public:

  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options_.EnableFilter(RewriteOptions::kExtendCache);
    options_.EnableFilter(RewriteOptions::kRewriteImages);
    CssRewriteTestBase::SetUp();

    // We want a real hasher here so that subresources get separate locks.
    resource_manager_->set_hasher(&md5_hasher_);

    // As we use invalid payloads, we expect image rewriting to
    // fail but cache extension to succeed.
    InitResponseHeaders("a.png", kContentTypePng, "notapng", kExpireAPngSec);
    InitResponseHeaders("b.png", kContentTypePng, "notbpng", kExpireBPngSec);
  }

  void ValidateExpirationTime(const char* id, const char* output,
                              int64 expected_expire_ms) {
    std::string css_url = ExpectedUrlForCss(id, output);

    // See what cache information we have
    scoped_ptr<OutputResource> output_resource(
        rewrite_driver_.CreateOutputResourceWithPath(
            kTestDomain, RewriteDriver::kCssFilterId, StrCat(id, ".css"),
            &kContentTypeCss, OutputResource::kRewrittenResource));
    ASSERT_TRUE(output_resource.get() != NULL);
    EXPECT_EQ(css_url, output_resource->url());
    ASSERT_TRUE(output_resource->cached_result() != NULL);

    EXPECT_EQ(expected_expire_ms,
              output_resource->cached_result()->origin_expiration_time_ms());
  }

  std::string ExpectedUrlForPng(const StringPiece& name,
                                 const std::string& expected_output) {
    return Encode(kTestDomain, RewriteDriver::kCacheExtenderId,
                  resource_manager_->hasher()->Hash(expected_output),
                  name, "png");
  }

};

// Test to make sure expiration time for cached result is the
// smallest of subresource and CSS times, not just CSS time.
TEST_F(CssFilterSubresourceTest, SubResourceDepends) {
  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";

  // Figure out where cache-extended PNGs will go.
  std::string img_url1 = ExpectedUrlForPng("a.png", "notapng");
  std::string img_url2 = ExpectedUrlForPng("b.png", "notbpng");
  std::string output = StrCat("div{background-image:url(", img_url1, ")}",
                               "span{background-image:url(", img_url2, ")}");

  // Here we don't use the other contexts since it has different
  // synchronicity, and we presently do best-effort for loaded subresources
  // even in Fetch.
  ValidateRewriteExternalCss(
      "ext", kInput, output, kNoOtherContexts | kNoClearFetcher |
                             kExpectChange | kExpectSuccess);

  // 10 is the smaller of expiration times of a.png, b.png and ext.css
  ValidateExpirationTime("ext", output.c_str(), kMinExpirationTimeMs);
}

// Test to make sure we don't cache for long if the rewrite was based
// on not-yet-loaded resources.
TEST_F(CssFilterSubresourceTest, SubResourceDependsNotYetLoaded) {
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher(SetupWaitFetcher());

  // Disable atime simulation so that the clock doesn't move on us.
  file_system_.set_atime_enabled(false);

  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";
  const char kOutput[] = "div{background-image:url(a.png)}"
                        "span{background-image:url(b.png)}";

  // At first try, not even the CSS gets loaded, so nothing gets
  // changed at all.
  ValidateRewriteExternalCss(
      "wip", kInput, kInput, kNoOtherContexts | kNoClearFetcher |
                             kExpectNoChange | kExpectSuccess);

  // Get the CSS to load (resources are still unavailable).
  wait_fetcher->CallCallbacks();
  ValidateRewriteExternalCss(
      "wip", kInput, kOutput, kNoOtherContexts | kNoClearFetcher |
                              kExpectChange | kExpectSuccess);

  // Since resources haven't loaded, the output cache should have a very small
  // expiration time.
  ValidateExpirationTime("wip", kOutput, Timer::kSecondMs);

  // Make sure the subresource callbacks fire for leak cleanliness
  wait_fetcher->CallCallbacks();
}

}  // namespace

}  // namespace net_instaweb
