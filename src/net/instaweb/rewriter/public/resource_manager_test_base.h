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

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"


#define URL_PREFIX "http://www.example.com/"

namespace net_instaweb {

class AbstractMutex;
class Hasher;
class LRUCache;
class MessageHandler;
class ResponseHeaders;
class RewriteDriverFactory;
class RewriteFilter;
class Statistics;
class ThreadSystem;
struct ContentType;

const int kCacheSize = 100 * 1000 * 1000;

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 public:
  static void SetUpTestCase();
  static void TearDownTestCase();

  static const char kTestData[];    // Testdata directory.
  static const char kXhtmlDtd[];    // DOCTYPE string for claming XHTML

  ResourceManagerTestBase();
  ~ResourceManagerTestBase();

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

  // Sets the active context URL for purposes of XS checks of fetches
  // on the main rewrite driver.
  void SetBaseUrlForFetch(const StringPiece& url);

  // Sets whether asynchronous rewrites are on, for both main and 'other'
  // rewrite drivers we manage.
  void SetAsynchronousRewrites(bool async);

  ResourcePtr CreateResource(const StringPiece& base, const StringPiece& url);

  MockTimer* mock_timer() { return &timer_; }

  void DeleteFileIfExists(const GoogleString& filename);

  void AppendDefaultHeaders(const ContentType& content_type,
                            GoogleString* text);

  void ServeResourceFromManyContexts(const GoogleString& resource_url,
                                     const StringPiece& expected_content);

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const GoogleString& resource_url,
      const StringPiece& expected_content);

  // This definition is required by HtmlParseTestBase which defines this as
  // pure abstract, so that the test subclass can define how it instantiates
  // HtmlParse.
  virtual RewriteDriver* html_parse() { return &rewrite_driver_; }

  // Initializes a resource for mock fetching.
  void InitResponseHeaders(const StringPiece& resource_name,
                           const ContentType& content_type,
                           const StringPiece& content,
                           int64 ttl_sec);

  void AddFileToMockFetcher(const StringPiece& url,
                            const StringPiece& filename,
                            const ContentType& content_type, int64 ttl_sec);

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool ServeResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     GoogleString* content);

  bool ServeResourceUrl(const StringPiece& url, GoogleString* content);

  // Just check if we can fetch a resource successfully, ignore response.
  bool TryFetchResource(const StringPiece& url);


  // Representation for a CSS <link> tag.
  class CssLink {
   public:
    CssLink(const StringPiece& url, const StringPiece& content,
            const StringPiece& media, bool supply_mock);

    // A vector of CssLink* should know how to accumulate and add.
    class Vector : public std::vector<CssLink*> {
     public:
      ~Vector();
      void Add(const StringPiece& url, const StringPiece& content,
               const StringPiece& media, bool supply_mock);
    };

    // Parses a combined CSS elementand provides the segments from which
    // it came.
    bool DecomposeCombinedUrl(GoogleString* base, StringVector* segments,
                              MessageHandler* handler);

    GoogleString url_;
    GoogleString content_;
    GoogleString media_;
    bool supply_mock_;
  };

  // Collects the hrefs for all CSS <link>s on the page.
  void CollectCssLinks(const StringPiece& id, const StringPiece& html,
                       StringVector* css_links);

  // Collects all information about CSS links into a CssLink::Vector.
  void CollectCssLinks(const StringPiece& id, const StringPiece& html,
                       CssLink::Vector* css_links);


  // Helper function to encode a resource name from its pieces.
  GoogleString Encode(const StringPiece& path,
                      const StringPiece& filter_id, const StringPiece& hash,
                      const StringPiece& name, const StringPiece& ext);

  // Overrides the async fetcher on the primary context to be a
  // wait fetcher which permits delaying callback invocation.
  // CallFetcherCallbacks can then be called to let the fetches complete
  // and call the callbacks.
  void SetupWaitFetcher();
  void CallFetcherCallbacks();

  RewriteOptions* options() { return options_; }
  RewriteOptions* other_options() { return other_options_; }

  // Helper method to test all manner of resource serving from a filter.
  void TestServeFiles(const ContentType* content_type,
                      const StringPiece& filter_id,
                      const StringPiece& rewritten_ext,
                      const StringPiece& orig_name,
                      const StringPiece& orig_content,
                      const StringPiece& rewritten_name,
                      const StringPiece& rewritten_content);

  // These functions help test the ResourceManager::store_outputs_in_file_system
  // functionality.

  // Translates an output URL into a full file pathname.
  GoogleString OutputResourceFilename(const StringPiece& url);

  // Writes an output resource into the file system.
  void WriteOutputResourceFile(const StringPiece& url,
                               const ContentType* content_type,
                               const StringPiece& rewritten_content);

  // Removes the output resource from the file system.
  void RemoveOutputResourceFile(const StringPiece& url);

  RewriteDriverFactory* factory() { return factory_; }

  void UseMd5Hasher() {
    resource_manager_->set_hasher(&md5_hasher_);
    other_resource_manager_.set_hasher(&md5_hasher_);
  }


  void SetDefaultLongCacheHeaders(const ContentType* content_type,
                                  ResponseHeaders* header) {
    resource_manager_->SetDefaultLongCacheHeaders(content_type, header);
  }

  void SetFetchResponse(const StringPiece& url,
                        const ResponseHeaders& response_header,
                        const StringPiece& response_body) {
    mock_url_fetcher_.SetResponse(url, response_header, response_body);
  }

  void SetFetchResponse404(const StringPiece& url);

  void SetFetchFailOnUnexpected(bool fail) {
    mock_url_fetcher_.set_fail_on_unexpected(fail);
  }
  void FetcherUpdateDateHeaders() {
    mock_url_fetcher_.set_timer(mock_timer());
    mock_url_fetcher_.set_update_date_headers(true);
  }
  void ClearFetcherResponses() { mock_url_fetcher_.Clear(); }

  void EncodeFilename(const StringPiece& url, GoogleString* filename) {
    filename_encoder_.Encode(file_prefix_, url, filename);
  }

  Hasher* hasher() { return resource_manager_->hasher(); }
  LRUCache* lru_cache() { return lru_cache_; }
  Statistics* statistics() { return statistics_; }
  MemFileSystem* file_system() { return &file_system_; }
  StringPiece url_prefix() const { return url_prefix_; }
  HTTPCache* http_cache() { return &http_cache_; }
  MessageHandler* message_handler() { return &message_handler_; }

  // TODO(jmarantz): These abstractions are not satisfactory long-term
  // where we want to have driver-lifetime in tests be reflective of
  // how servers work.  But for now we use these accessors.
  RewriteDriver* rewrite_driver() { return &rewrite_driver_; }
  RewriteDriver* other_rewrite_driver() { return &other_rewrite_driver_; }

  bool ReadFile(const char* filename, GoogleString* contents) {
    return file_system_.ReadFile(filename, contents, &message_handler_);
  }
  bool WriteFile(const char* filename, const StringPiece& contents) {
    return file_system_.WriteFile(filename, contents, &message_handler_);
  }

  ResourceManager* resource_manager() { return resource_manager_; }
  CountingUrlAsyncFetcher* counting_url_async_fetcher() {
    return &counting_url_async_fetcher_;
  }

  void SetMockHashValue(const GoogleString& value) {
    mock_hasher_.set_hash_value(value);
  }

  // Connects a RewriteDriver to ResourceManager, establishing a MockScheduler
  // to advance time.
  void SetupDriver(ResourceManager* rm, RewriteDriver* rd);

 private:
  // Calls callbacks on given wait fetcher, making sure to properly synchronize
  // with async rewrite flows given driver.
  void CallFetcherCallbacksForDriver(WaitUrlAsyncFetcher* fetcher,
                                     RewriteDriver* driver);

  // Converts a potentially relative URL off kTestDomain to absolute if needed.
  GoogleString AbsolutifyUrl(const StringPiece& in);

  MockUrlFetcher mock_url_fetcher_;
  FakeUrlAsyncFetcher mock_url_async_fetcher_;
  CountingUrlAsyncFetcher counting_url_async_fetcher_;
  bool wait_for_fetches_;
  FilenameEncoder filename_encoder_;

  MockHasher mock_hasher_;
  MD5Hasher md5_hasher_;

  // We have two independent RewriteDrivers representing two completely
  // separate servers for the same domain (say behind a load-balancer).
  //
  // Server A runs rewrite_driver_ and will be used to rewrite pages and
  // served the rewritten resources.
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<AbstractMutex> timer_mutex_;
  int64 start_time_ms_;
  MockTimer timer_;
  MemFileSystem file_system_;
  MemFileSystem other_file_system_;

  GoogleString file_prefix_;
  GoogleString url_prefix_;

  LRUCache* lru_cache_;  // Owned by http_cache_
  HTTPCache http_cache_;
  FileSystemLockManager lock_manager_;
  static SimpleStats* statistics_;
  RewriteDriverFactory* factory_;
  ResourceManager* resource_manager_;  // TODO(sligocki): Make not a pointer.

  // TODO(jmarantz): the 'options_' and 'other_options_' variables should
  // be changed from references to pointers, in a follow-up CL.
  RewriteOptions* options_;  // owned by rewrite_driver_.
  RewriteDriver rewrite_driver_;

  // Server B runs other_rewrite_driver_ and will get a request for
  // resources that server A has rewritten, but server B has not heard
  // of yet. Thus, server B will have to decode the instructions on how
  // to rewrite the resource just from the request.
  LRUCache* other_lru_cache_;  // Owned by other_http_cache_
  HTTPCache other_http_cache_;
  FileSystemLockManager other_lock_manager_;
  ResourceManager other_resource_manager_;
  RewriteOptions* other_options_;  // owned by other_rewrite_driver_.
  RewriteDriver other_rewrite_driver_;
  WaitUrlAsyncFetcher wait_url_async_fetcher_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
