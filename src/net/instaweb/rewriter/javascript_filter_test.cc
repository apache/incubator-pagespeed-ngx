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

// Unit-test the javascript filter

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kHtmlFormat[] =
    "<script type='text/javascript' src='%s'></script>\n";

const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
const char kJsMinData[] = "alert('hello, world!')";
const char kFilterId[] = "jm";
const char kOrigJsName[] = "hello.js";
const char kRewrittenJsName[] = "hello.js";

}  // namespace

namespace net_instaweb {

class JavascriptFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    AddFilter(RewriteOptions::kRewriteJavascript);
    ResourceNamer namer;
    namer.set_id(kFilterId);
    namer.set_name(kRewrittenJsName);
    namer.set_ext("js");
    namer.set_hash("0");
    expected_rewritten_path_ = StrCat(kTestDomain, namer.Encode());
  }

  void InitTest(int64 ttl) {
    InitResponseHeaders(kOrigJsName, kContentTypeJavascript, kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  GoogleString GenerateHtml(const char* a) {
    return StringPrintf(kHtmlFormat, a);
  }

  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    // Do a normal rewrite test
    InitTest(100);
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));

    // Fetch messed up URL.
    GoogleString out;
    EXPECT_EQ(should_fetch_ok,
              ServeResourceUrl(StrCat(expected_rewritten_path_, junk), &out));

    // Rewrite again; should still get normal URL
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));
  }

  GoogleString expected_rewritten_path_;
};

TEST_F(JavascriptFilterTest, DoRewrite) {
  InitTest(100);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

// Temporarily test one path using the async model.
// TODO(jmarantz): remove this method and convert everything to async.
TEST_F(JavascriptFilterTest, DoAsyncRewrite) {
  rewrite_driver_.SetAsynchronousRewrites(true);
  InitTest(100);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_F(JavascriptFilterTest, RewriteAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  // But we will rewrite because we can make the data smaller.
  ValidateExpected("rewrite_despite_being_cached_properly",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_F(JavascriptFilterTest, NoRewriteOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));
}

TEST_F(JavascriptFilterTest, ServeFiles) {
  TestServeFiles(&kContentTypeJavascript, kFilterId, "js",
                 kOrigJsName, kJsData,
                 kRewrittenJsName, kJsMinData);

  // Finally, serve from a completely separate server.
  ServeResourceFromManyContexts(expected_rewritten_path_,
                                RewriteOptions::kRewriteJavascript,
                                &mock_hasher_,
                                kJsMinData);
}

TEST_F(JavascriptFilterTest, InvalidInputMimetype) {
  // Make sure we can rewrite properly even when input has corrupt mimetype.
  ContentType not_java_script = kContentTypeJavascript;
  not_java_script.mime_type_ = "text/semicolon-inserted";
  const char* kNotJsFile = "script.notjs";

  InitResponseHeaders(kNotJsFile, not_java_script, kJsData, 100);
  ValidateExpected("wrong_mime", GenerateHtml(kNotJsFile),
                   GenerateHtml(StrCat(
                       kTestDomain, kNotJsFile, ".pagespeed.jm.0.js").c_str()));
}

// Make sure bad requests do not corrupt our extension.
TEST_F(JavascriptFilterTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(JavascriptFilterTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

}  // namespace net_instaweb
