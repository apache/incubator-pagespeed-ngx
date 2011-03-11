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


#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"

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
  std::string GenerateHtml(const char* a) {
    return StringPrintf(kHtmlFormat, a);
  }

  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    // Do a normal rewrite test
    InitTest(100);
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));

    // Fetch messed up URL.
    std::string out;
    EXPECT_EQ(should_fetch_ok,
              ServeResourceUrl(StrCat(expected_rewritten_path_, junk), &out));

    // Rewrite again; should still get normal URL
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));
  }

  std::string expected_rewritten_path_;
};

TEST_F(JavascriptFilterTest, DoRewrite) {
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
  std::string content;

  // TODO(jmarantz): Factor some of this logic-flow out so that
  // cache_extender_test.cc can share it.

  // When we start, there are no mock fetchers, so we'll need to get it
  // from the cache or the disk.  Start with the cache.
  file_system_.Disable();
  ResponseHeaders headers;
  resource_manager_->SetDefaultHeaders(&kContentTypeJavascript, &headers);
  http_cache_.Put(expected_rewritten_path_, &headers, kJsMinData,
                  &message_handler_);
  EXPECT_EQ(0, lru_cache_->num_hits());
  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId,
                            kRewrittenJsName, "js", &content));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(std::string(kJsMinData), content);

  // Now remove it from the cache, but put it in the file system.  Make sure
  // that works.  Still there is no mock fetcher.
  file_system_.Enable();
  lru_cache_->Clear();

  // Getting the filename is kind of a drag, isn't it.  But someone's
  // gotta do it.
  // TODO(jmarantz): refactor this and share it with other filter_tests.
  std::string filename;
  FilenameEncoder* encoder = resource_manager_->filename_encoder();
  encoder->Encode(resource_manager_->filename_prefix(),
                  expected_rewritten_path_, &filename);
  std::string data = StrCat(headers.ToString(), kJsMinData);
  ASSERT_TRUE(file_system_.WriteFile(filename.c_str(), data,
                                     &message_handler_));

  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId,
                            kRewrittenJsName, "js", &content));
  EXPECT_EQ(std::string(kJsMinData), content);

  // After serving from the disk, we should have seeded our cache.  Check it.
  EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(
      expected_rewritten_path_));

  // Finally, nuke the file, nuke the cache, get it via a fetch.
  file_system_.Disable();
  ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
  lru_cache_->Clear();
  InitTest(100);
  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId,
                            kRewrittenJsName, "js", &content));
  EXPECT_EQ(std::string(kJsMinData), content);

  // Now we expect both the file and the cache entry to be there.
  EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(
      expected_rewritten_path_));
  file_system_.Enable();
  EXPECT_TRUE(file_system_.Exists(filename.c_str(), &message_handler_)
              .is_true());

  // Finally, serve from a completely separate server.
  ServeResourceFromManyContexts(expected_rewritten_path_,
                                RewriteOptions::kRewriteJavascript,
                                &mock_hasher_,
                                kJsMinData);
}

// Make sure bad requests do not corrupt our extension.
TEST_F(JavascriptFilterTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(JavascriptFilterTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

}  // namespace net_instaweb
