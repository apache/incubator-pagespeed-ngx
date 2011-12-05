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
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
// We need to include rewrite_driver.h due to covariant return of html_parse()
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"


namespace net_instaweb {

class CountingUrlAsyncFetcher;
class HTTPCache;
class Hasher;
class LRUCache;
class MessageHandler;
class MockScheduler;
class MockTimer;
class ResourceNamer;
class ResponseHeaders;
class RewriteFilter;
class Statistics;
class UrlNamer;
class WaitUrlAsyncFetcher;
struct ContentType;

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 public:
  static const char kTestData[];    // Testdata directory.
  static const char kXhtmlDtd[];    // DOCTYPE string for claming XHTML

  ResourceManagerTestBase();
  ResourceManagerTestBase(TestRewriteDriverFactory* factory,
                          TestRewriteDriverFactory* other_factory);
  virtual ~ResourceManagerTestBase();

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

  ResourcePtr CreateResource(const StringPiece& base, const StringPiece& url);

  MockTimer* mock_timer() { return factory_->mock_timer(); }

  void DeleteFileIfExists(const GoogleString& filename);

  void AppendDefaultHeaders(const ContentType& content_type,
                            GoogleString* text);

  void ServeResourceFromManyContexts(const GoogleString& resource_url,
                                     const StringPiece& expected_content,
                                     UrlNamer* new_rms_url_namer = NULL);

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const GoogleString& resource_url,
      const StringPiece& expected_content,
      UrlNamer* new_rms_url_namer = NULL);

  // This definition is required by HtmlParseTestBase which defines this as
  // pure abstract, so that the test subclass can define how it instantiates
  // HtmlParse.
  virtual RewriteDriver* html_parse() { return rewrite_driver_; }

  // Set default headers for a resource with content_type and Cache ttl_sec.
  void DefaultResponseHeaders(const ContentType& content_type, int64 ttl_sec,
                              ResponseHeaders* response_headers);

  // Add content to mock fetcher (with default headers).
  void InitResponseHeaders(const StringPiece& relative_url,
                           const ContentType& content_type,
                           const StringPiece& content,
                           int64 ttl_sec);

  // Add the contents of a file to mock fetcher (with default headers).
  void AddFileToMockFetcher(const StringPiece& url,
                            const StringPiece& filename,
                            const ContentType& content_type, int64 ttl_sec);

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool ServeResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     GoogleString* content);
  bool ServeResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     GoogleString* content, ResponseHeaders* response);


  bool ServeResourceUrl(const StringPiece& url, GoogleString* content,
                        ResponseHeaders* response);
  bool ServeResourceUrl(const StringPiece& url, GoogleString* content);

  // Just check if we can fetch a resource successfully, ignore response.
  bool TryFetchResource(const StringPiece& url);

  // Use managed rewrite drivers for the test so that we see the same behavior
  // in tests that we see in real servers. By default, tests use unmanaged
  // drivers so that _test.cc files can add options after the driver was created
  // and before the filters are added.
  void SetUseManagedRewriteDrivers(bool use_managed_rewrite_drivers);

  GoogleString CssLinkHref(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }

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

  // Encode the given name (path + leaf) using the given pagespeed attributes.
  void EncodePathAndLeaf(const StringPiece& filter_id,
                         const StringPiece& hash,
                         const StringVector& name_vector,
                         const StringPiece& ext,
                         ResourceNamer* namer);

  StringVector MultiUrl(const StringPiece& url1) {
    StringVector v;
    v.push_back(url1.as_string());
    return v;
  }

  StringVector MultiUrl(const StringPiece& url1, const StringPiece& url2) {
    StringVector v;
    v.push_back(url1.as_string());
    v.push_back(url2.as_string());
    return v;
  }

  StringVector MultiUrl(const StringPiece& url1, const StringPiece& url2,
                        const StringPiece& url3) {
    StringVector v;
    v.push_back(url1.as_string());
    v.push_back(url2.as_string());
    v.push_back(url3.as_string());
    return v;
  }

  StringVector MultiUrl(const StringPiece& url1, const StringPiece& url2,
                        const StringPiece& url3, const StringPiece& url4) {
    StringVector v;
    v.push_back(url1.as_string());
    v.push_back(url2.as_string());
    v.push_back(url3.as_string());
    v.push_back(url4.as_string());
    return v;
  }

  // Helper function to encode a resource name from its pieces using whatever
  // encoding we are testing, either UrlNamer or TestUrlNamer.
  GoogleString Encode(const StringPiece& path,
                      const StringPiece& filter_id,
                      const StringPiece& hash,
                      const StringPiece& name,
                      const StringPiece& ext) {
    return Encode(path, filter_id, hash, MultiUrl(name), ext);
  }
  GoogleString Encode(const StringPiece& path,
                      const StringPiece& filter_id,
                      const StringPiece& hash,
                      const StringVector& name_vector,
                      const StringPiece& ext);

  // Same as Encode but specifically using UrlNamer not TestUrlNamer.
  GoogleString EncodeNormal(const StringPiece& path,
                            const StringPiece& filter_id,
                            const StringPiece& hash,
                            const StringPiece& name,
                            const StringPiece& ext) {
    return EncodeNormal(path, filter_id, hash, MultiUrl(name), ext);
  }
  GoogleString EncodeNormal(const StringPiece& path,
                            const StringPiece& filter_id,
                            const StringPiece& hash,
                            const StringVector& name_vector,
                            const StringPiece& ext);

  // Same as Encode but specifying the base URL (which is used by TestUrlNamer
  // but is unused by UrlNamer so for it results in exactly the same as Encode).
  GoogleString EncodeWithBase(const StringPiece& base,
                              const StringPiece& path,
                              const StringPiece& filter_id,
                              const StringPiece& hash,
                              const StringPiece& name,
                              const StringPiece& ext) {
    return EncodeWithBase(base, path, filter_id, hash, MultiUrl(name), ext);
  }
  GoogleString EncodeWithBase(const StringPiece& base,
                              const StringPiece& path,
                              const StringPiece& filter_id,
                              const StringPiece& hash,
                              const StringVector& name_vector,
                              const StringPiece& ext);

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

  TestRewriteDriverFactory* factory() { return factory_.get(); }
  TestRewriteDriverFactory* other_factory() { return other_factory_.get(); }

  void UseMd5Hasher() {
    resource_manager_->set_hasher(&md5_hasher_);
    other_resource_manager_->set_hasher(&md5_hasher_);
  }


  void SetDefaultLongCacheHeaders(const ContentType* content_type,
                                  ResponseHeaders* header) {
    resource_manager_->SetDefaultLongCacheHeaders(content_type, header);
  }

  void SetFetchResponse(const StringPiece& url,
                        const ResponseHeaders& response_header,
                        const StringPiece& response_body) {
    mock_url_fetcher()->SetResponse(url, response_header, response_body);
  }

  void AddToResponse(const StringPiece& url,
                     const StringPiece& name,
                     const StringPiece& value) {
    mock_url_fetcher()->AddToResponse(url, name, value);
  }

  void SetFetchResponse404(const StringPiece& url);

  void SetFetchFailOnUnexpected(bool fail) {
    mock_url_fetcher()->set_fail_on_unexpected(fail);
  }
  void FetcherUpdateDateHeaders() {
    mock_url_fetcher()->set_timer(mock_timer());
    mock_url_fetcher()->set_update_date_headers(true);
  }
  void ClearFetcherResponses() { mock_url_fetcher()->Clear(); }

  virtual void ClearStats();

  void EncodeFilename(const StringPiece& url, GoogleString* filename) {
    factory_->filename_encoder()->Encode(factory_->filename_prefix(), url,
                                         filename);
  }

  MockUrlFetcher* mock_url_fetcher() {
    return &mock_url_fetcher_;
  }
  Hasher* hasher() { return resource_manager_->hasher(); }
  LRUCache* lru_cache() { return factory_->lru_cache(); }
  Statistics* statistics() { return factory_->statistics(); }
  MemFileSystem* file_system() { return factory_->mem_file_system(); }
  HTTPCache* http_cache() { return factory_->http_cache(); }
  MockMessageHandler* message_handler() {
    return factory_->mock_message_handler();
  }

  // TODO(jmarantz): These abstractions are not satisfactory long-term
  // where we want to have driver-lifetime in tests be reflective of
  // how servers work.  But for now we use these accessors.
  //
  // Note that the *rewrite_driver() methods are not valid during
  // construction, so any test classes that need to use them must
  // do so from SetUp() methods.
  RewriteDriver* rewrite_driver() { return rewrite_driver_; }
  RewriteDriver* other_rewrite_driver() { return other_rewrite_driver_; }

  // The scheduler used by rewrite_driver
  MockScheduler* mock_scheduler() { return factory_->mock_scheduler(); }

  int64 start_time_ms() const { return factory_->kStartTimeMs; }

  bool ReadFile(const char* filename, GoogleString* contents) {
    return file_system()->ReadFile(filename, contents, message_handler());
  }
  bool WriteFile(const char* filename, const StringPiece& contents) {
    return file_system()->WriteFile(filename, contents, message_handler());
  }

  ResourceManager* resource_manager() { return resource_manager_; }
  ResourceManager* other_resource_manager() { return other_resource_manager_; }
  CountingUrlAsyncFetcher* counting_url_async_fetcher() {
    return factory_->counting_url_async_fetcher();
  }

  void SetMockHashValue(const GoogleString& value) {
    factory_->mock_hasher()->set_hash_value(value);
  }

  void SetCacheDelayUs(int64 delay_us);

  // Creates a RewriteDriver using the passed-in options, object, but
  // does *not* finalize the driver.  This gives individual _test.cc
  // files the chance to add filters to the options prior to calling
  // driver->AddFilters().
  RewriteDriver* MakeDriver(ResourceManager* resource_manager,
                            RewriteOptions* options);

  // Converts a potentially relative URL off kTestDomain to absolute if needed.
  GoogleString AbsolutifyUrl(const StringPiece& in);

  // Tests that non-caching-related response-header attributes are propagated
  // to output resources.
  //
  // 'name' is the name of the resource.
  void TestRetainExtraHeaders(const StringPiece& name,
                              const StringPiece& filter_id,
                              const StringPiece& ext);

  // Find the segment-encoder for the filter found via 'id'.  Some
  // test filters are not registered with RewriteDriver so for those
  // we use the default encoder.
  const UrlSegmentEncoder* FindEncoder(const StringPiece& id) const;

  // Switch url namers as specified.
  void SetUseTestUrlNamer(bool use_test_url_namer);

  // Helper function which instantiates an encoder, collects the
  // required arguments and calls the virtual Encode().
  GoogleString EncodeCssName(const StringPiece& name,
                             bool supports_webp,
                             bool can_inline);

 protected:
  void Init();

  // Calls callbacks on given wait fetcher, making sure to properly synchronize
  // with async rewrite flows given driver.
  void CallFetcherCallbacksForDriver(WaitUrlAsyncFetcher* fetcher,
                                     RewriteDriver* driver);

  // The mock fetcher & stats are global across all Factories used in the tests.
  MockUrlFetcher mock_url_fetcher_;
  SimpleStats statistics_;

  // We have two independent RewriteDrivers representing two completely
  // separate servers for the same domain (say behind a load-balancer).
  //
  // Server A runs rewrite_driver_ and will be used to rewrite pages and
  // serves the rewritten resources.
  scoped_ptr<TestRewriteDriverFactory> factory_;
  scoped_ptr<TestRewriteDriverFactory> other_factory_;
  ResourceManager* resource_manager_;
  RewriteDriver* rewrite_driver_;
  ResourceManager* other_resource_manager_;
  RewriteDriver* other_rewrite_driver_;
  bool use_managed_rewrite_drivers_;

  MD5Hasher md5_hasher_;

  RewriteOptions* options_;  // owned by rewrite_driver_.
  RewriteOptions* other_options_;  // owned by other_rewrite_driver_.
  UrlSegmentEncoder default_encoder_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
