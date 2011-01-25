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

// Author: abliss@google.com (Adam Bliss)

// Base class for tests which want a ResourceManager.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>
#include "net/instaweb/http/public/wait_url_async_fetcher.h"

#define URL_PREFIX "http://www.example.com/"

namespace net_instaweb {

const int kCacheSize = 100 * 1000 * 1000;
const char kTestDomain[] = "http://test.com/";

class RewriteFilter;

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 protected:
  ResourceManagerTestBase()
      : mock_url_async_fetcher_(&mock_url_fetcher_),
        file_prefix_(StrCat(GTestTempDir(), "/")),
        url_prefix_(URL_PREFIX),

        lru_cache_(new LRUCache(kCacheSize)),
        http_cache_(lru_cache_, file_system_.timer()),
        // TODO(jmaessen): Pull timer out of file_system_ and make it
        // standalone.
        lock_manager_(&file_system_, file_system_.timer(), &message_handler_),
        // TODO(sligocki): Why can't I init it here ...
        // resource_manager_(new ResourceManager(
        //    file_prefix_, &file_system_,
        //    &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
        //    &http_cache_)),
        rewrite_driver_(&message_handler_, &file_system_,
                        &mock_url_async_fetcher_, options_),

        other_lru_cache_(new LRUCache(kCacheSize)),
        other_http_cache_(other_lru_cache_, other_file_system_.timer()),
        other_lock_manager_(
            &other_file_system_, other_file_system_.timer(), &message_handler_),
        other_resource_manager_(
            file_prefix_, &other_file_system_,
            &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
            &other_http_cache_, &other_lock_manager_),
        other_rewrite_driver_(&message_handler_, &other_file_system_,
                              &mock_url_async_fetcher_, other_options_) {
    // rewrite_driver_.SetResourceManager(resource_manager_);
    other_rewrite_driver_.SetResourceManager(&other_resource_manager_);
  }

  virtual void SetUp() {
    HtmlParseTestBaseNoAlloc::SetUp();
    // TODO(sligocki): Init this in constructor.
    resource_manager_ = new ResourceManager(
        file_prefix_, &file_system_,
        &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
        &http_cache_, &lock_manager_);
    rewrite_driver_.SetResourceManager(resource_manager_);
  }

  virtual void TearDown() {
    delete resource_manager_;
    HtmlParseTestBaseNoAlloc::TearDown();
  }

  // In this set of tests, we will provide explicit body tags, so
  // the test harness should not add them in for our convenience.
  // It can go ahead and add the <html> and </html>, however.
  virtual bool AddBody() const {
    return false;
  }

  // Add a single rewrite filter to rewrite_driver_.
  void AddFilter(RewriteOptions::Filter filter) {
    options_.EnableFilter(filter);
    rewrite_driver_.AddFilters();
  }

  // Add a single rewrite filter to other_rewrite_driver_.
  void AddOtherFilter(RewriteOptions::Filter filter) {
    other_options_.EnableFilter(filter);
    other_rewrite_driver_.AddFilters();
  }

  // Add a custom rewrite filter (one without a corresponding option)
  // to rewrite_driver and enable it.
  void AddRewriteFilter(RewriteFilter* filter);

  // Add a custom rewrite filter (one without a corresponding option)
  // to other_rewrite_driver and enable it.
  void AddOtherRewriteFilter(RewriteFilter* filter);

  // The async fetchers in these tests are really fake async fetchers, and
  // will call their callbacks directly.  Hence we don't really need
  // any functionality in the async callback.
  class DummyCallback : public UrlAsyncFetcher::Callback {
   public:
    explicit DummyCallback(bool expect_success)
        : done_(false),
          expect_success_(expect_success) {}
    virtual ~DummyCallback() {
      EXPECT_TRUE(done_);
    }
    virtual void Done(bool success) {
      EXPECT_FALSE(done_) << "Already Done; perhaps you reused without Reset()";
      done_ = true;
      EXPECT_EQ(expect_success_, success);
    }
    void Reset() {
      done_ = false;
    }

    bool done_;
    bool expect_success_;

   private:
    DISALLOW_COPY_AND_ASSIGN(DummyCallback);
  };

  MockTimer* mock_timer() { return file_system_.timer(); }

  void DeleteFileIfExists(const std::string& filename) {
    if (file_system_.Exists(filename.c_str(), &message_handler_).is_true()) {
      ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
    }
  }

  void AppendDefaultHeaders(const ContentType& content_type,
                            ResourceManager* resource_manager,
                            std::string* text) {
    ResponseHeaders header;
    int64 time = mock_timer()->NowUs();
    // Reset mock timer so synthetic headers match original.
    mock_timer()->set_time_us(0);
    resource_manager->SetDefaultHeaders(&content_type, &header);
    // Then set it back
    mock_timer()->set_time_us(time);
    StringWriter writer(text);
    header.WriteAsHttp(&writer, &message_handler_);
  }

  void ServeResourceFromManyContexts(const std::string& resource_url,
                                     RewriteOptions::Filter filter,
                                     Hasher* hasher,
                                     const StringPiece& expected_content) {
    // TODO(sligocki): Serve the resource under several contexts. For example:
    //   1) With output-resource cached,
    //   2) With output-resource not cached, but in a file,
    //   3) With output-resource unavailable, but input-resource cached,
    //   4) With output-resource unavailable and input-resource not cached,
    //      but still fetchable,
    ServeResourceFromNewContext(resource_url, filter, hasher, expected_content);
    //   5) With nothing available (failure).
  }

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const std::string& resource_url,
      RewriteOptions::Filter filter,
      Hasher* hasher,
      const StringPiece& expected_content);

  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

  // Initializes a resource for mock fetching.
  void InitResponseHeaders(const StringPiece& resource_name,
                    const ContentType& content_type,
                    const StringPiece& content,
                    int64 ttl) {
    std::string name = StrCat(kTestDomain, resource_name);
    ResponseHeaders response_headers;
    resource_manager_->SetDefaultHeaders(&content_type, &response_headers);
    response_headers.RemoveAll(HttpAttributes::kCacheControl);
    response_headers.Add(
        HttpAttributes::kCacheControl,
        StringPrintf("public, max-age=%ld", static_cast<long>(ttl)).c_str());
    response_headers.ComputeCaching();
    mock_url_fetcher_.SetResponse(name, response_headers, content);
  }

  // TODO(sligocki): Take a ttl and share code with InitResponseHeaders.
  void AddFileToMockFetcher(const StringPiece& url,
                            const std::string& filename,
                            const ContentType& content_type) {
    // TODO(sligocki): There's probably a lot of wasteful copying here.

    // We need to load a file from the testdata directory. Don't use this
    // physical filesystem for anything else, use file_system_ which can be
    // abstracted as a MemFileSystem instead.
    std::string contents;
    StdioFileSystem stdio_file_system;
    ASSERT_TRUE(stdio_file_system.ReadFile(filename.c_str(), &contents,
                                           &message_handler_));

    // Put file into our fetcher.
    ResponseHeaders default_header;
    resource_manager_->SetDefaultHeaders(&content_type, &default_header);
    mock_url_fetcher_.SetResponse(url, default_header, contents);
  }

  // Callback that can be used for testing resource fetches.  As all the
  // async fetchers in unit-tests call their callbacks immediately, it
  // is safe to put this on the stack, rather than having it self-delete.
  // TODO(sligocki): Do we need this and DummyCallback, could we give this
  // a more descriptive name? MockCallback?
  class FetchCallback : public UrlAsyncFetcher::Callback {
   public:
    FetchCallback() : success_(false), done_(false) {}
    virtual void Done(bool success) {
      success_ = success;
      done_ = true;
    }

    bool success() const { return success_; }
    bool done() const { return done_; }

   private:
    bool success_;
    bool done_;
  };

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool ServeResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     std::string* content) {
    std::string url = Encode(path, filter_id, "0", name, ext);
    return ServeResourceUrl(url, content);
  }

  bool ServeResourceUrl(const StringPiece& url, std::string* content) {
    content->clear();
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    StringWriter writer(content);
    FetchCallback callback;
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &message_handler_,
        &callback);
    // The callback should be called if and only if FetchResource
    // returns true.
    EXPECT_EQ(fetched, callback.done());
    return fetched && callback.success();
  }

  // Just check if we can fetch a resource successfully, ignore response.
  bool TryFetchResource(const StringPiece& url) {
    std::string contents;
    return ServeResourceUrl(url, &contents);
  }

  // Helper function to encode a resource name from its pieces.
  std::string Encode(const StringPiece& path,
                      const StringPiece& filter_id, const StringPiece& hash,
                      const StringPiece& name, const StringPiece& ext);

  // Testdata directory.
  static const char kTestData[];

  MockUrlFetcher mock_url_fetcher_;
  FakeUrlAsyncFetcher mock_url_async_fetcher_;
  FilenameEncoder filename_encoder_;

  MockHasher mock_hasher_;
  MD5Hasher md5_hasher_;

  std::string file_prefix_;
  std::string url_prefix_;

  // We have two independent RewriteDrivers representing two completely
  // separate servers for the same domain (say behind a load-balancer).
  //
  // Server A runs rewrite_driver_ and will be used to rewrite pages and
  // served the rewritten resources.
  MemFileSystem file_system_;
  LRUCache* lru_cache_;  // Owned by http_cache_
  HTTPCache http_cache_;
  FileSystemLockManager lock_manager_;
  ResourceManager* resource_manager_;  // TODO(sligocki): Make not a pointer.
  RewriteOptions options_;
  RewriteDriver rewrite_driver_;

  // Server B runs other_rewrite_driver_ and will get a request for
  // resources that server A has rewritten, but server B has not heard
  // of yet. Thus, server B will have to decode the instructions on how
  // to rewrite the resource just from the request.
  MemFileSystem other_file_system_;
  LRUCache* other_lru_cache_;  // Owned by other_http_cache_
  HTTPCache other_http_cache_;
  FileSystemLockManager other_lock_manager_;
  ResourceManager other_resource_manager_;
  RewriteOptions other_options_;
  RewriteDriver other_rewrite_driver_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
