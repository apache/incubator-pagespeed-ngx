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
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>
#include "net/instaweb/http/public/wait_url_async_fetcher.h"

#define URL_PREFIX "http://www.example.com/"

namespace net_instaweb {

const int kCacheSize = 100 * 1000 * 1000;

class RewriteFilter;

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 protected:
  static const char kTestData[];    // Testdata directory.
  static const char kXhtmlDtd[];    // DOCTYPE string for claming XHTML

  ResourceManagerTestBase();

  virtual void SetUp();
  virtual void TearDown();

  // In this set of tests, we will provide explicit body tags, so
  // the test harness should not add them in for our convenience.
  // It can go ahead and add the <html> and </html>, however.
  virtual bool AddBody() const { return false; }

  // Add a single rewrite filter to rewrite_driver_.
  void AddFilter(RewriteOptions::Filter filter);

  // Add a single rewrite filter to other_rewrite_driver_.
  void AddOtherFilter(RewriteOptions::Filter filter);

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
  Resource* CreateResource(const StringPiece& base, const StringPiece& url);

  MockTimer* mock_timer() { return file_system_.timer(); }

  void DeleteFileIfExists(const std::string& filename);

  void AppendDefaultHeaders(const ContentType& content_type,
                            ResourceManager* resource_manager,
                            std::string* text);

  void ServeResourceFromManyContexts(const std::string& resource_url,
                                     RewriteOptions::Filter filter,
                                     Hasher* hasher,
                                     const StringPiece& expected_content);

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const std::string& resource_url,
      RewriteOptions::Filter filter,
      Hasher* hasher,
      const StringPiece& expected_content);

  // This definition is required by HtmlParseTestBase which defines this as
  // pure abstract, so that the test subclass can define how it instantiates
  // HtmlParse.
  virtual RewriteDriver* html_parse() { return &rewrite_driver_; }

  // Initializes a resource for mock fetching.
  void InitResponseHeaders(const StringPiece& resource_name,
                           const ContentType& content_type,
                           const StringPiece& content,
                           int64 ttl);

  // TODO(sligocki): Take a ttl and share code with InitResponseHeaders.
  void AddFileToMockFetcher(const StringPiece& url,
                            const std::string& filename,
                            const ContentType& content_type);

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
                     std::string* content);

  bool ServeResourceUrl(const StringPiece& url, std::string* content);

  // Just check if we can fetch a resource successfully, ignore response.
  bool TryFetchResource(const StringPiece& url);

  // Helper function to encode a resource name from its pieces.
  std::string Encode(const StringPiece& path,
                      const StringPiece& filter_id, const StringPiece& hash,
                      const StringPiece& name, const StringPiece& ext);

  // Overrides the async fetcher on the primary context to be a
  // wait fetcher which permits delaying callback invocation, and returns a
  // pointer to the new fetcher.
  WaitUrlAsyncFetcher* SetupWaitFetcher();

  virtual void PostParseHook() {
    // This mocks the behavior of a system-integration with an event-loop.
    rewrite_driver_.Scan();

    // TODO(jmarantz): When the cache is made asynchronous, this is
    // where we'd mock a delay to allow the caches to return, and
    // to run its callbacks.
  }

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

  SimpleStats statistics_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
