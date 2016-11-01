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

// Base class for tests which want a ServerContext.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_TEST_BASE_H_

#include <utility>
#include <vector>

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
// We need to include rewrite_driver.h due to covariant return of html_parse()
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_hasher.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
// We need to include mock_timer.h to allow upcast to Timer*.
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/util/url_segment_encoder.h"
#include "pagespeed/opt/logging/request_timing_info.h"



namespace net_instaweb {

class AbstractLogRecord;
class CountingUrlAsyncFetcher;
class DelayCache;
class LRUCache;
class MockLogRecord;
class MockScheduler;
class ProcessContext;
class RewriteFilter;
class WaitUrlAsyncFetcher;

class RewriteOptionsTestBase : public HtmlParseTestBaseNoAlloc {
 protected:
  RewriteOptionsTestBase() {
    RewriteOptions::Initialize();
  }
  ~RewriteOptionsTestBase() {
    RewriteOptions::Terminate();
  }
};

class RewriteTestBase : public RewriteOptionsTestBase {
 public:
  static const char kTestData[];    // Testdata directory.

  // Beaconing key values used when downstream caching is enabled.
  static const char kConfiguredBeaconingKey[];
  static const char kWrongBeaconingKey[];

  // Specifies which server should be "active" in that rewrites and fetches
  // will use it. The data members affected are those returned by:
  // - factory() / other_factory()
  // - server_context() / other_server_context()
  // - rewrite_driver() / other_rewrite_driver()
  // - options() / other_options()
  enum ActiveServerFlag {
    kPrimary,    // Use the normal data members.
    kSecondary,  // Use all the other_ data members.
  };

  RewriteTestBase();

  // Specifies alternate factories to be initialized on construction.
  // By default, TestRewriteDriverFactory is used, but you can employ
  // your own subclass of TestRewriteDriverFactory using this
  // constructor.  If you do, you probably also want to override
  // MakeTestFactory.
  explicit RewriteTestBase(std::pair<TestRewriteDriverFactory*,
                                     TestRewriteDriverFactory*> factories);
  virtual ~RewriteTestBase();

  virtual void SetUp();
  virtual void TearDown();

  // In this set of tests, we will provide explicit body tags, so
  // the test harness should not add them in for our convenience.
  // It can go ahead and add the <html> and </html>, however.
  virtual bool AddBody() const { return false; }

  // Makes a TestRewriteDriverFactory.  This can be overridden in
  // subclasses if you need a factory with special properties.
  //
  // TODO(jmarantz): This is currently only used in
  // ServeResourceFromNewContext, but should be used for factory_ and
  // other_factory_.  This would requuire a refactor, because those
  // are created at construction; too early for subclass overrides to
  // take effect.  To deal with that, an alternate constructor is
  // provided above so that the proper sort of factories can be passed in.
  virtual TestRewriteDriverFactory* MakeTestFactory();

  // Adds kRecompressJpeg, kRecompressPng, kRecompressWebp, kConvertPngToJpeg,
  // kConvertJpegToWebp and kConvertGifToPng.
  void AddRecompressImageFilters();

  // Add a single rewrite filter to rewrite_driver_.
  void AddFilter(RewriteOptions::Filter filter);

  // Add a single rewrite filter to other_rewrite_driver_.
  void AddOtherFilter(RewriteOptions::Filter filter);

  // Add a custom rewrite filter (one without a corresponding option)
  // to rewrite_driver and enable it.
  void AddRewriteFilter(RewriteFilter* filter);

  // Adds a custom rewriter filter but does not register it for HTML
  // rewriting, only for fetches.
  void AddFetchOnlyRewriteFilter(RewriteFilter* filter);

  // Add a custom rewrite filter (one without a corresponding option)
  // to other_rewrite_driver and enable it.
  void AddOtherRewriteFilter(RewriteFilter* filter);

  // Sets the active context URL for purposes of XS checks of fetches
  // on the main rewrite driver.
  void SetBaseUrlForFetch(const StringPiece& url);

  // Populates request-headers based on the current user-agent and
  // the attributes added via AddRequestAttribute and installs them
  // into rewrite_driver_.
  void SetDriverRequestHeaders();

  // Enable downstream caching feature and set up the downstream cache
  // rebeaconing key.
  void SetDownstreamCacheDirectives(
    StringPiece downstream_cache_purge_method,
    StringPiece downstream_cache_purge_location_prefix,
    StringPiece rebeaconing_key);

  // Set ShouldBeacon request header to the specified value.
  void SetShouldBeaconHeader(StringPiece rebeaconing_key);

  ResourcePtr CreateResource(const StringPiece& base, const StringPiece& url);

  // Returns the main factory Timer*, which can be used for calling NowUs and
  // NowMs.  To set the time, use (Advance|Set)Time(Ms|Us), which wake up any
  // scheduler alarms.  See also AdjustTimeUsWithoutWakingAlarms which should
  // be used with extreme care.
  Timer* timer() { return factory()->mock_timer(); }

  // Append default headers to the given string.
  void AppendDefaultHeaders(const ContentType& content_type,
                            GoogleString* text);

  // Like above, but also include a Link: <..>; rel="canonical" header.
  void AppendDefaultHeadersWithCanonical(const ContentType& content_type,
                                         StringPiece canonical_url,
                                         GoogleString* text);

  void ServeResourceFromManyContexts(const GoogleString& resource_url,
                                     const StringPiece& expected_content);

  void ServeResourceFromManyContextsWithUA(
      const GoogleString& resource_url,
      const StringPiece& expected_content,
      const StringPiece& user_agent);

  // Test that a resource can be served from an new server that has not already
  // constructed it.
  void ServeResourceFromNewContext(
      const GoogleString& resource_url,
      const StringPiece& expected_content);

  // This definition is required by HtmlParseTestBase which defines this as
  // pure abstract, so that the test subclass can define how it instantiates
  // HtmlParse.
  virtual RewriteDriver* html_parse() { return rewrite_driver_; }

  // Set default headers for a resource with content_type and Cache ttl_sec.
  void DefaultResponseHeaders(const ContentType& content_type, int64 ttl_sec,
                              ResponseHeaders* response_headers);

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool FetchResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     GoogleString* content);
  bool FetchResource(const StringPiece& path, const StringPiece& filter_id,
                     const StringPiece& name, const StringPiece& ext,
                     GoogleString* content, ResponseHeaders* response);

  bool FetchResourceUrl(const StringPiece& url, GoogleString* content,
                        ResponseHeaders* response);
  bool FetchResourceUrl(const StringPiece& url,
                        RequestHeaders* request_headers,
                        GoogleString* content,
                        ResponseHeaders* response_headers);
  bool FetchResourceUrl(const StringPiece& url, GoogleString* content);

  // Just check if we can fetch a resource successfully, ignore response.
  bool TryFetchResource(const StringPiece& url);

  // Use managed rewrite drivers for the test so that we see the same behavior
  // in tests that we see in real servers. By default, tests use unmanaged
  // drivers so that _test.cc files can add options after the driver was created
  // and before the filters are added.  Note that this will only clean them up
  // via shutdown codepath if you don't actually use them, unless an explicit
  // Cleanup() call is made.
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
    bool DecomposeCombinedUrl(StringPiece base_url, GoogleString* base,
                              StringVector* segments, MessageHandler* handler);

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

  // Encode image with width and height. Use -1 for either width or height to
  // omit it from the encoding.
  GoogleString EncodeImage(int width, int height,
                           StringPiece filename, StringPiece hash,
                           StringPiece rewritten_ext);

  // Takes an already-encoded URL and adds options to to it.
  GoogleString AddOptionsToEncodedUrl(const StringPiece& url,
                                      const StringPiece& options);

  // If append_new_suffix is true, appends new_suffix to old_url.
  // If append_new_suffix is false, replaces old_suffix at the end of old_url
  // with new_suffix.
  // Either way, precondition: old_url ends with old_suffix
  static GoogleString ChangeSuffix(
      StringPiece old_url, bool append_new_suffix,
      StringPiece old_suffix, StringPiece new_suffix);

  // Overrides the async fetcher on the primary context to be a
  // wait fetcher which permits delaying callback invocation.
  // CallFetcherCallbacks can then be called to let the fetches complete
  // and call the callbacks.
  void SetupWaitFetcher();
  void CallFetcherCallbacks();
  void OtherCallFetcherCallbacks();
  RewriteOptions* options() const { return options_; }
  RewriteOptions* other_options() const { return other_options_; }

  // Set the RewriteOptions to be returned by the RewriteOptionsManager.
  void SetRewriteOptions(RewriteOptions* opts);

  // Authorizes a domain to options()->domain_lawyer(), recomputing
  // the options signature if necessary.
  bool AddDomain(StringPiece domain);

  // Adds an origin domain mapping to options()->domain_lawyer(), recomputing
  // the options signature if necessary.
  bool AddOriginDomainMapping(StringPiece to_domain, StringPiece from_domain);

  // Adds a rewrite domain mapping to options()->domain_lawyer(), recomputing
  // the options signature if necessary.
  bool AddRewriteDomainMapping(StringPiece to_domain, StringPiece from_domain);

  // Adds a shard to options()->domain_lawyer(), recomputing the options
  // signature if necessary.
  bool AddShard(StringPiece domain, StringPiece shards);

  // Helper method to test all manner of resource serving from a filter.
  void TestServeFiles(const ContentType* content_type,
                      const StringPiece& filter_id,
                      const StringPiece& rewritten_ext,
                      const StringPiece& orig_name,
                      const StringPiece& orig_content,
                      const StringPiece& rewritten_name,
                      const StringPiece& rewritten_content);

  // Check that when we have a cache miss for a pagespeed resource we set
  // headers to reduce the chance of it being interpreted as html.
  void ValidateFallbackHeaderSanitization(StringPiece filter_id);

  TestRewriteDriverFactory* factory() { return factory_.get(); }
  TestRewriteDriverFactory* other_factory() { return other_factory_.get(); }

  void UseMd5Hasher() {
    server_context_->set_hasher(&md5_hasher_);
    server_context_->http_cache()->set_hasher(&md5_hasher_);
    other_server_context_->set_hasher(&md5_hasher_);
    other_server_context_->http_cache()->set_hasher(&md5_hasher_);
  }


  void SetDefaultLongCacheHeaders(const ContentType* content_type,
                                  ResponseHeaders* header) {
    server_context_->SetDefaultLongCacheHeaders(
        content_type, StringPiece(), StringPiece(), header);
  }

  void SetFetchResponse(const StringPiece& url,
                        const ResponseHeaders& response_header,
                        const StringPiece& response_body) {
    mock_url_fetcher()->SetResponse(url, response_header, response_body);
  }

  // Add content to mock fetcher (with default headers).
  void SetResponseWithDefaultHeaders(const StringPiece& relative_url,
                                     const ContentType& content_type,
                                     const StringPiece& content,
                                     int64 ttl_sec);

  // Load a test file (from testdata/) into 'contents', returning false on
  // failure.
  bool LoadFile(const StringPiece& filename, GoogleString* contents);

  // Add the contents of a file to mock fetcher (with default headers).
  void AddFileToMockFetcher(const StringPiece& url,
                            const StringPiece& filename,
                            const ContentType& content_type, int64 ttl_sec);

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
    mock_url_fetcher()->set_timer(timer());
    mock_url_fetcher()->set_update_date_headers(true);
  }
  void ClearFetcherResponses() { mock_url_fetcher()->Clear(); }

  virtual void ClearStats();

  // Calls Clear() on the rewrite driver and does any other necessary
  // clean-up so the driver is okay for a test to reuse.
  //
  // Removes pending request-header attributes added via AddRequestAttribute.
  void ClearRewriteDriver();

  MockUrlFetcher* mock_url_fetcher() {
    return &mock_url_fetcher_;
  }
  Hasher* hasher() { return server_context_->hasher(); }
  DelayCache* delay_cache() { return factory_->delay_cache(); }
  LRUCache* lru_cache() { return factory_->lru_cache(); }
  Statistics* statistics() { return factory_->statistics(); }
  MemFileSystem* file_system() { return factory_->mem_file_system(); }
  HTTPCache* http_cache() { return server_context_->http_cache(); }
  PropertyCache* page_property_cache() {
    return server_context_->page_property_cache();
  }
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

  ServerContext* server_context() { return server_context_; }
  ServerContext* other_server_context() { return other_server_context_; }
  CountingUrlAsyncFetcher* counting_url_async_fetcher() {
    return factory_->counting_url_async_fetcher();
  }
  void SetMockHashValue(const GoogleString& value) {
    factory_->mock_hasher()->set_hash_value(value);
  }

  void SetCacheDelayUs(int64 delay_us);

  void SetupWriter() override;

  // Creates a RewriteDriver using the passed-in options, object, but
  // does *not* finalize the driver.  This gives individual _test.cc
  // files the chance to add filters to the options prior to calling
  // driver->AddFilters().
  RewriteDriver* MakeDriver(ServerContext* server_context,
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

  // Helper function for legacy tests that used this now-extinct interface.
  // In general we don't support this flow in production but we rely on it
  // in tests for obliquely covering some cases relating to resource pathnames.
  bool ReadIfCached(const ResourcePtr& resource);

  // Variation on ReadIfCached that is used when we expect the resource
  // not to be in present in cache, but instead we are looking to
  // initiate the resource-rewrite process so that a subsequent call
  // to ReadIfCached succeeds.
  void InitiateResourceRead(const ResourcePtr& resource);

  // While our production cache model is non-blocking, we use an in-memory LRU
  // for tests that calls its callback directly from Get.  Thus we can make
  // a convenient blocking cache wrapper to make it easier to write tests.
  HTTPCache::FindResult HttpBlockingFind(
      const GoogleString& key, HTTPCache* http_cache, HTTPValue* value_out,
      ResponseHeaders* headers);

  // The same as the above function, but doesn't need an HTTPValue or
  // ResponseHeaders.
  HTTPCache::FindResult HttpBlockingFindStatus(
      const GoogleString& key, HTTPCache* http_cache);

  // Same as above, but with options (for invalidation checks)
  HTTPCache::FindResult HttpBlockingFindWithOptions(
      const RewriteOptions* options,
      const GoogleString& key, HTTPCache* http_cache, HTTPValue* value_out,
      ResponseHeaders* headers);

  // Sets the response-headers Content-Type to "application/xhtml+xml".
  void SetXhtmlMimetype() { SetMimetype("application/xhtml+xml"); }

  // Sets the response-headers Content-Type to "text/html".
  void SetHtmlMimetype() { SetMimetype("text/html"); }

  // Sets the response-headers Content-Type as specified.
  void SetMimetype(const StringPiece& mimetype);

  // Verifies that the specified URL can be fetched from HTTP cache, and that
  // its cache TTL and contents are as specified.
  void CheckFetchFromHttpCache(
      StringPiece url,
      StringPiece expected_contents,
      int64 expected_expiration_ms);

  // Setup statistics for the given cohort and add it to the give PropertyCache.
  const PropertyCache::Cohort*  SetupCohort(
      PropertyCache* cache, const GoogleString& cohort) {
    return factory()->SetupCohort(cache, cohort);
  }

  // Configure the other_server_context_ to use the same LRU cache as the
  // primary server context.
  void SetupSharedCache();

  // Returns a new mock property page for the page property cache.
  MockPropertyPage* NewMockPage(const StringPiece& url,
                                const StringPiece& options_signature_hash,
                                UserAgentMatcher::DeviceType device_type) {
    return new MockPropertyPage(
        server_context_->thread_system(),
        server_context_->page_property_cache(),
        url,
        options_signature_hash,
        UserAgentMatcher::DeviceTypeSuffix(device_type));
  }

  MockPropertyPage* NewMockPage(const StringPiece& url) {
    return NewMockPage(url, "hash", UserAgentMatcher::kDesktop);
  }

  // Sets MockLogRecord in the driver's request_context.
  void SetMockLogRecord();

  // Returns the MockLogRecord in the driver.
  MockLogRecord* mock_log_record();

  // Helper methods to return js/html snippets related to lazyload images.
  GoogleString GetLazyloadScriptHtml();
  GoogleString GetLazyloadPostscriptHtml();

  // Sets the server-scoped invalidation timestamp.  Time is advanced by
  // 1 second both before and after invalidation.  E.g. if the current time
  // is 100000 milliseconds at the time this is called, the invalidation
  // timestamp will be at 101000 milliseconds, and time will be rolled
  // forward to 102000 on exit from this function.
  void SetCacheInvalidationTimestamp();

  // Sets the invalidation timestamp for a URL pattern.  Time is advanced by
  // in the same manner as for SetCacheInvalidationTimestamp above.
  void SetCacheInvalidationTimestampForUrl(
      StringPiece url, bool ignores_metadata_and_pcache);

  // Changes the way cache-purges are implemented for non-wildcards to
  // avoid flushing the entire metadata cache and instead match each
  // metadata Input against the invalidation-set.
  void EnableCachePurge();

  // Enables the debug flag, which is often done on a test-by-test basis.
  void EnableDebug();

  // Enable debugging and set expected_debug_message used by DebugMessage.
  // Occurrences of %url% in the message will be replaced by the argument
  // to DebugMessage.
  void DebugWithMessage(StringPiece expected_debug_message) {
    EnableDebug();

    expected_debug_message.CopyToString(&debug_message_);
  }

  // Return the debug message if it was set by DebugWithMessage, empty string
  // otherwise.  Inserts url for %url% if needed, attempting to resolve it
  // against kTestDomain first, and using url exactly as passed if resolving it
  // doesn't return a valid url.
  GoogleString DebugMessage(StringPiece url);

  // Returns a process context needed for any tests to instantiate factories
  // explicitly.
  static const ProcessContext& process_context();

  // Turns off gzip capability in the cache.  Note that requests will still be
  // formulated with Accept-Encoding:gzip.
  void DisableGzip();

  // Determines whether a response was originally gzipped.
  bool WasGzipped(const ResponseHeaders& response_headers);

 protected:
  // Common values for HttpBlockingFind* result.
  const HTTPCache::FindResult kFoundResult;
  const HTTPCache::FindResult kNotFoundResult;

  void Init();

  // Override this if the test fixture needs to use a different RequestContext
  // subclass.
  virtual RequestContextPtr CreateRequestContext();

  // Calls callbacks on given wait fetcher, making sure to properly synchronize
  // with async rewrite flows given driver.
  void CallFetcherCallbacksForDriver(WaitUrlAsyncFetcher* fetcher,
                                     RewriteDriver* driver);

  // Populate the given headers based on the content type and original
  // content length information.
  void PopulateDefaultHeaders(const ContentType& content_type,
                              int64 original_content_length,
                              ResponseHeaders* headers);

  // Set the "active" server to that specified; the active server is used for
  // rewriting and serving pages.
  void SetActiveServer(ActiveServerFlag server_to_use);

  // Advances time forward using the mock scheduler.  Note that time is not
  // advanced directly in the mock_timer; the scheduler must be used.
  void AdvanceTimeUs(int64 delay_ms);
  void AdvanceTimeMs(int64 delay_ms) { AdvanceTimeUs(delay_ms * Timer::kMsUs); }
  void SetTimeUs(int64 time_us);
  void SetTimeMs(int64 time_ms) { SetTimeUs(time_ms * Timer::kMsUs); }

  // Adjusts time ignoring any scheduler callbacks.  Use with caution.
  void AdjustTimeUsWithoutWakingAlarms(int64 time_us);

  // Accessor for TimingInfo.
  const RequestTimingInfo& timing_info();
  RequestTimingInfo* mutable_timing_info();

  // Returns the current request context.  The default implementation takes
  // the request context from rewrite_driver().  ProxyInterfaceTestBase
  // overrides.
  //
  // This method check-fails if the current request-context is null.
  virtual RequestContextPtr request_context();

  // Convenience method to pull the logging info proto out of the current
  // request context's log record. The request context owns the log record, and
  // if the log record has a non-NULL mutex, it will need to be locked
  // for this call.
  LoggingInfo* logging_info();

  // Convenience method to extract read-only metadata_cache_info.
  const MetadataCacheInfo& metadata_cache_info() {
    return logging_info()->metadata_cache_info();
  }

  // Convenience method for retrieving the computed applied rewriters string
  // from the current request context's log record. Thread-safe.
  GoogleString AppliedRewriterStringFromLog();

  // Convenience method for verifying that the rewriter info entries have
  // expected values.
  void VerifyRewriterInfoEntry(AbstractLogRecord* log_record,
                               const GoogleString& id,
                               int url_index,
                               int rewriter_info_index,
                               int rewriter_info_size,
                               int url_list_size,
                               const GoogleString& url);

  // Sets current_user_agent_
  void SetCurrentUserAgent(const StringPiece& user_agent) {
    current_user_agent_ = user_agent;
  }

  // Sets up user-agent and request-header to allow webp processing.
  void SetupForWebp() {
    SetCurrentUserAgent("webp");
    AddRequestAttribute(HttpAttributes::kAccept, "image/webp");
  }

  void SetupForWebpLossless() {
    SetCurrentUserAgent("webp-la");
    AddRequestAttribute(HttpAttributes::kAccept, "image/webp");
  }

  void SetupForWebpAnimated() {
    SetCurrentUserAgent("webp-animated");
    AddRequestAttribute(HttpAttributes::kAccept, "image/webp");
  }

  // Adds an attribute to be populated later into a RequestHeaders* object,
  // along with the user-agent.  Note that these attributes stay in the
  // test-class until ClearRewriteDriver is called.
  void AddRequestAttribute(StringPiece name, StringPiece value);

  // Populates a RequestHeaders* object with al
  void PopulateRequestHeaders(RequestHeaders* request_headers);

  // Override HtmlParseTestBaseNoAlloc::ParseUrl to populate the
  // request-headers into rewrite_driver_ before running filters.
  virtual void ParseUrl(StringPiece url, StringPiece html_input);

  GoogleString ExpectedNonce();

  // When reaching into a cache that backs an HTTP cache you need a cache key
  // that includes the fragment.
  GoogleString HttpCacheKey(StringPiece url) {
    return http_cache()->CompositeKey(url, rewrite_driver_->CacheFragment());
  }

  // Returns the value of a TimedVariable, specified by name.
  int TimedValue(StringPiece name);

  // The mock fetchers & stats are global across all Factories used in the
  // tests.
  MockUrlFetcher mock_url_fetcher_;
  scoped_ptr<Statistics> statistics_;

  // We have two independent RewriteDrivers representing two completely
  // separate servers for the same domain (say behind a load-balancer).
  //
  // Server A runs rewrite_driver_ and will be used to rewrite pages and
  // serves the rewritten resources.
  scoped_ptr<TestRewriteDriverFactory> factory_;
  scoped_ptr<TestRewriteDriverFactory> other_factory_;
  ServerContext* server_context_;
  RewriteDriver* rewrite_driver_;
  ServerContext* other_server_context_;
  RewriteDriver* other_rewrite_driver_;
  scoped_ptr<HtmlWriterFilter> other_html_writer_filter_;
  ActiveServerFlag active_server_;
  bool use_managed_rewrite_drivers_;
  StringPiece current_user_agent_;
  StringVector request_attribute_names_;
  StringVector request_attribute_values_;

  MD5Hasher md5_hasher_;

  RewriteOptions* options_;  // owned by rewrite_driver_.
  RewriteOptions* other_options_;  // owned by other_rewrite_driver_.
  UrlSegmentEncoder default_encoder_;
  ResponseHeaders response_headers_;
  const GoogleString kEtag0;  // Etag with a 0 hash.
  uint64 expected_nonce_;

  GoogleString debug_message_;  // Message used by DebugMessage

 private:
  void ValidateFallbackHeaderSanitizationHelper(
      StringPiece filter_id, StringPiece origin_content_type, bool expect_load);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_TEST_BASE_H_
