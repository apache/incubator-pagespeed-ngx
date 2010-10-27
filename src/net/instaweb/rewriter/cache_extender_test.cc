/**
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

// Unit-test the cache extender.


#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"

namespace {

const char kHtmlFormat[] =
    "<link rel='stylesheet' href='%s.css' type='text/css'>\n"
    "<img src='%s.jpg'/>\n"
    "<script type='text/javascript' src='%s.js'></script>\n";

const char kCssData[] = ".blue {color: blue;}";
const char kImageData[] = "Invalid JPEG but it does not matter for this test";
const char kJsData[] = "alert('hello, world!')";
const char kFilterId[] = "ce";

}  // namespace

namespace net_instaweb {

class CacheExtenderTest : public ResourceManagerTestBase {
 protected:

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    rewrite_driver_.AddFilter(RewriteOptions::kExtendCache);
  }

  void InitMetaData(const StringPiece& resource_name,
                    const ContentType& content_type,
                    const StringPiece& content,
                    int64 ttl,
                    MetaData* meta_data) {
    std::string name = StrCat("http://test.com/", resource_name);
    resource_manager_->SetDefaultHeaders(&content_type, meta_data);
    meta_data->RemoveAll(HttpAttributes::kCacheControl);
    meta_data->Add(
        HttpAttributes::kCacheControl,
        StringPrintf("public, max-age=%ld", static_cast<long>(ttl)).c_str());
    mock_url_fetcher_.SetResponse(name, *meta_data, content);
  }

  void InitTest(int64 ttl) {
    InitMetaData("a.css", kContentTypeCss, kCssData, ttl, &css_header_);
    InitMetaData("b.jpg", kContentTypeJpeg, kImageData, ttl, &img_header_);
    InitMetaData("c.js", kContentTypeJavascript, kJsData, ttl, &js_header_);
  }

  // Generate HTML loading 3 resources with the specified URLs
  std::string GenerateHtml(const char* a, const char* b, const char* c) {
    return StringPrintf(kHtmlFormat, a, b, c);
  }

  class FetchCallback : public UrlAsyncFetcher::Callback {
   public:
    FetchCallback() : success_(false), done_(false) {}
    virtual void Done(bool success) { success_ = success; done_ = true; }
    bool success() const { return success_; }
   private:
    bool success_;
    bool done_;
  };

  bool ServeResource(const StringPiece& name, const char* ext,
                     std::string* content) {
    SimpleMetaData request_headers, response_headers;
    content->clear();
    StringWriter writer(content);
    FetchCallback callback;
    ResourceNamer namer;
    namer.set_id(kFilterId);
    namer.set_name(name);
    namer.set_hash("0");
    namer.set_ext(ext);
    std::string url = StrCat(
        resource_manager_->UrlPrefixFor(namer), namer.PrettyName());
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &message_handler_,
        &callback);
    return fetched && callback.success();
  }

  SimpleMetaData css_header_;
  SimpleMetaData img_header_;
  SimpleMetaData js_header_;
  GoogleMessageHandler message_handler_;
};

TEST_F(CacheExtenderTest, DoExtend) {
  InitTest(100);
  ValidateExpected("do_extend",
                   GenerateHtml("a", "b", "c").c_str(),
                   GenerateHtml("http://mysite/ce.0.,htest,c,_a,s",
                                "http://mysite/ce.0.,htest,c,_b,j",
                                "http://mysite/ce.0.,htest,c,_c,l").c_str());
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  ValidateExpected("no_extend_cached_properly",
                   GenerateHtml("a", "b", "c").c_str(),
                   GenerateHtml("a", "b", "c").c_str());
}

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml("a", "b", "c").c_str(),
                   GenerateHtml("a", "b", "c").c_str());
}

TEST_F(CacheExtenderTest, ServeFiles) {
  std::string content;

  InitTest(100);
  ASSERT_TRUE(ServeResource(",htest,c,_a,s", "css", &content));
  EXPECT_EQ(std::string(kCssData), content);
  ASSERT_TRUE(ServeResource(",htest,c,_b,j", "jpg", &content));
  EXPECT_EQ(std::string(kImageData), content);
  ASSERT_TRUE(ServeResource(",htest,c,_c,l", "js", &content));
  EXPECT_EQ(std::string(kJsData), content);

  // TODO(jmarantz): make 3 variations of this test:
  //  1. Gets the data from the cache, with no mock fetchers, null file system
  //  2. Gets the data from the file system, with no cache, no mock fetchers.
  //  3. Gets the data from the mock fetchers: no cache, no file system.
}

}  // namespace net_instaweb
