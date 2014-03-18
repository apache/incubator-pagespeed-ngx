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

// Unit-test the server context

#include "net/instaweb/rewriter/public/server_context.h"

#include <cstddef>                     // for size_t

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace {

const char kResourceUrl[] = "http://example.com/image.png";
const char kResourceUrlBase[] = "http://example.com";
const char kResourceUrlPath[] = "/image.png";
const char kOptionsHash[] = "1234";

const char kUrlPrefix[] = "http://www.example.com/";
const size_t kUrlPrefixLength = STATIC_STRLEN(kUrlPrefix);

}  // namespace

namespace net_instaweb {

class HtmlElement;

class VerifyContentsCallback : public Resource::AsyncCallback {
 public:
  VerifyContentsCallback(const ResourcePtr& resource,
                         const GoogleString& contents) :
      Resource::AsyncCallback(resource),
      contents_(contents),
      called_(false) {
  }
  VerifyContentsCallback(const OutputResourcePtr& resource,
                         const GoogleString& contents) :
      Resource::AsyncCallback(ResourcePtr(resource)),
      contents_(contents),
      called_(false) {
  }
  virtual void Done(bool lock_failure, bool resource_ok) {
    EXPECT_FALSE(lock_failure);
    EXPECT_EQ(0, contents_.compare(resource()->contents().as_string()));
    called_ = true;
  }
  void AssertCalled() {
    EXPECT_TRUE(called_);
  }
  GoogleString contents_;
  bool called_;
};

class ServerContextTest : public RewriteTestBase {
 protected:
  ServerContextTest() { }

  // Fetches data (which is expected to exist) for given resource,
  // but making sure to go through the path that checks for its
  // non-existence and potentially doing locking, too.
  // Note: resource must have hash set.
  bool FetchExtantOutputResourceHelper(const OutputResourcePtr& resource,
                                       StringAsyncFetch* async_fetch) {
    async_fetch->set_response_headers(resource->response_headers());
    RewriteFilter* null_filter = NULL;  // We want to test the cache only.
    EXPECT_TRUE(rewrite_driver()->FetchOutputResource(resource, null_filter,
                                                      async_fetch));
    rewrite_driver()->WaitForCompletion();
    EXPECT_TRUE(async_fetch->done());
    return async_fetch->success();
  }

  // Helper for testing of FetchOutputResource. Assumes that output_resource
  // is to be handled by the filter with 2-letter code filter_id, and
  // verifies result to match expect_success and expect_content.
  void TestFetchOutputResource(const OutputResourcePtr& output_resource,
                               const char* filter_id,
                               bool expect_success,
                               StringPiece expect_content) {
    ASSERT_TRUE(output_resource.get());
    RewriteFilter* filter = rewrite_driver()->FindFilter(filter_id);
    ASSERT_TRUE(filter != NULL);
    StringAsyncFetch fetch_result(CreateRequestContext());
    EXPECT_TRUE(rewrite_driver()->FetchOutputResource(
        output_resource, filter, &fetch_result));
    rewrite_driver()->WaitForCompletion();
    EXPECT_TRUE(fetch_result.done());
    EXPECT_EQ(expect_success, fetch_result.success());
    EXPECT_EQ(expect_content, fetch_result.buffer());
  }

  GoogleString GetOutputResource(const OutputResourcePtr& resource) {
    StringAsyncFetch fetch(RequestContext::NewTestRequestContext(
        server_context()->thread_system()));
    EXPECT_TRUE(FetchExtantOutputResourceHelper(resource, &fetch));
    return fetch.buffer();
  }

  // Returns whether there was an existing copy of data for the resource.
  // If not, makes sure the resource is wrapped.
  bool TryFetchExtantOutputResource(const OutputResourcePtr& resource) {
    StringAsyncFetch dummy_fetch(CreateRequestContext());
    return FetchExtantOutputResourceHelper(resource, &dummy_fetch);
  }

  // Asserts that the given url starts with an appropriate prefix;
  // then cuts off that prefix.
  virtual void RemoveUrlPrefix(const GoogleString& prefix, GoogleString* url) {
    EXPECT_TRUE(StringPiece(*url).starts_with(prefix));
    url->erase(0, prefix.length());
  }

  OutputResourcePtr CreateOutputResourceForFetch(const StringPiece& url) {
    RewriteFilter* dummy;
    rewrite_driver()->SetBaseUrlForFetch(url);
    GoogleUrl gurl(url);
    return rewrite_driver()->DecodeOutputResource(gurl, &dummy);
  }

  ResourcePtr CreateInputResourceAndReadIfCached(const StringPiece& url) {
    rewrite_driver()->SetBaseUrlForFetch(url);
    GoogleUrl resource_url(url);
    ResourcePtr resource(rewrite_driver()->CreateInputResource(resource_url));
    if ((resource.get() != NULL) && !ReadIfCached(resource)) {
      resource.clear();
    }
    return resource;
  }

  // Tests for the lifecycle and various flows of a named output resource.
  void TestNamed() {
    const char* filter_prefix = RewriteOptions::kCssFilterId;
    const char* name = "I.name";  // valid name for CSS filter.
    const char* contents = "contents";
    OutputResourcePtr output(
        rewrite_driver()->CreateOutputResourceWithPath(
            kUrlPrefix, filter_prefix, name, kRewrittenResource));
    ASSERT_TRUE(output.get() != NULL);
    // Check name_key against url_prefix/fp.name
    GoogleString name_key = output->name_key();
    RemoveUrlPrefix(kUrlPrefix, &name_key);
    EXPECT_EQ(output->full_name().EncodeIdName(), name_key);
    // Make sure the resource hasn't already been created. We do need to give it
    // a hash for fetching to do anything.
    output->SetHash("42");
    EXPECT_FALSE(TryFetchExtantOutputResource(output));
    EXPECT_FALSE(output->IsWritten());

    {
      // Check that a non-blocking attempt to create another resource
      // with the same name returns quickly. We don't need a hash in this
      // case since we're just trying to create the resource, not fetch it.
      OutputResourcePtr output1(
          rewrite_driver()->CreateOutputResourceWithPath(
              kUrlPrefix, filter_prefix, name, kRewrittenResource));
      ASSERT_TRUE(output1.get() != NULL);
      EXPECT_FALSE(output1->IsWritten());
    }

    {
      // Here we attempt to create the object with the hash and fetch it.
      // The fetch fails as there is no active filter to resolve it.
      ResourceNamer namer;
      namer.CopyFrom(output->full_name());
      namer.set_hash("0");
      namer.set_ext("txt");
      GoogleString name = StrCat(kUrlPrefix, namer.Encode());
      OutputResourcePtr output1(CreateOutputResourceForFetch(name));
      ASSERT_TRUE(output1.get() != NULL);

      // blocking but stealing
      EXPECT_FALSE(TryFetchExtantOutputResource(output1));
    }

    // Write some data
    ASSERT_TRUE(output->has_hash());
    EXPECT_EQ(kRewrittenResource, output->kind());
    EXPECT_TRUE(rewrite_driver()->Write(
        ResourceVector(), contents, &kContentTypeText, "utf-8", output.get()));
    EXPECT_TRUE(output->IsWritten());
    // Check that hash and ext are correct.
    EXPECT_EQ("0", output->hash());
    EXPECT_EQ("txt", output->extension());
    EXPECT_STREQ("utf-8", output->charset());

    // With the URL (which contains the hash), we can retrieve it
    // from the http_cache.
    OutputResourcePtr output4(CreateOutputResourceForFetch(output->url()));
    EXPECT_EQ(output->url(), output4->url());
    EXPECT_EQ(contents, GetOutputResource(output4));
  }

  bool ResourceIsCached() {
    ResourcePtr resource(CreateResource(kResourceUrlBase, kResourceUrlPath));
    return ReadIfCached(resource);
  }

  void StartRead() {
    ResourcePtr resource(CreateResource(kResourceUrlBase, kResourceUrlPath));
    InitiateResourceRead(resource);
  }

  GoogleString MakeEvilUrl(const StringPiece& host, const StringPiece& name) {
    GoogleString escaped_abs;
    UrlEscaper::EncodeToUrlSegment(name, &escaped_abs);
    // Do not use Encode, which will make the URL non-evil.
    // TODO(matterbury):  Rewrite this for a non-standard UrlNamer?
    return StrCat("http://", host, "/dir/123/", escaped_abs,
                  ".pagespeed.jm.0.js");
  }

  // Accessor for ServerContext field; also cleans up
  // deferred_release_rewrite_drivers_.
  void EnableRewriteDriverCleanupMode(bool s) {
    server_context()->trying_to_cleanup_rewrite_drivers_ = s;
    server_context()->deferred_release_rewrite_drivers_.clear();
  }

  // Creates a response with given ttl and extra cache control under given URL.
  void SetCustomCachingResponse(const StringPiece& url,
                                int ttl_ms,
                                const StringPiece& extra_cache_control) {
    ResponseHeaders response_headers;
    DefaultResponseHeaders(kContentTypeCss, ttl_ms, &response_headers);
    response_headers.SetDateAndCaching(
        http_cache()->timer()->NowMs(), ttl_ms * Timer::kSecondMs,
        extra_cache_control);
    response_headers.ComputeCaching();
    SetFetchResponse(AbsolutifyUrl(url), response_headers, "payload");
  }

  // Creates a resource with given ttl and extra cache control under given URL.
  ResourcePtr CreateCustomCachingResource(
      const StringPiece& url, int ttl_ms,
      const StringPiece& extra_cache_control) {
    SetCustomCachingResponse(url, ttl_ms, extra_cache_control);
    GoogleUrl gurl(AbsolutifyUrl(url));
    rewrite_driver()->SetBaseUrlForFetch(kTestDomain);
    ResourcePtr resource(rewrite_driver()->CreateInputResource(gurl));
    VerifyContentsCallback callback(resource, "payload");
    resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                        rewrite_driver()->request_context(),
                        &callback);
    callback.AssertCalled();
    return resource;
  }

  void RefererTest(const RequestHeaders* headers,
                   bool is_background_fetch) {
    GoogleString url = "test.jpg";
    rewrite_driver()->SetBaseUrlForFetch(kTestDomain);
    SetCustomCachingResponse(url, 100, "");
    GoogleUrl gurl(AbsolutifyUrl(url));
    ResourcePtr resource(rewrite_driver()->CreateInputResource(gurl));
    if (!is_background_fetch) {
      rewrite_driver()->SetRequestHeaders(*headers);
    }
    resource->set_is_background_fetch(is_background_fetch);
    VerifyContentsCallback callback(resource, "payload");
    resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                        rewrite_driver()->request_context(),
                        &callback);
    callback.AssertCalled();
  }

  void DefaultHeaders(ResponseHeaders* headers) {
    SetDefaultLongCacheHeaders(&kContentTypeCss, headers);
  }

  const RewriteDriver* decoding_driver() {
    return server_context()->decoding_driver_;
  }

  RewriteOptions* GetCustomOptions(const StringPiece& url,
                                   RequestHeaders* request_headers,
                                   RewriteOptions* domain_options) {
    // The default url_namer does not yield any name-derived options, and we
    // have not specified any URL params or request-headers, so there will be
    // no custom options, and no errors.
    GoogleUrl gurl(url);
    RewriteOptions* copy_options = domain_options != NULL ?
        domain_options->Clone() : NULL;
    RewriteQuery rewrite_query;
    bool success = server_context()->GetQueryOptions(&gurl,
                                                     request_headers, NULL,
                                                     &rewrite_query);
    EXPECT_TRUE(success);
    RewriteOptions* options =
        server_context()->GetCustomOptions(request_headers, copy_options,
                                           rewrite_query.ReleaseOptions());
    return options;
  }

  void CheckExtendCache(RewriteOptions* options, bool x) {
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheCss));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheImages));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheScripts));
  }
};

TEST_F(ServerContextTest, CustomOptionsWithNoUrlNamerOptions) {
  // The default url_namer does not yield any name-derived options, and we
  // have not specified any URL params or request-headers, so there will be
  // no custom options, and no errors.
  RequestHeaders request_headers;
  scoped_ptr<RewriteOptions> options(
      GetCustomOptions("http://example.com/", &request_headers, NULL));
  ASSERT_TRUE(options.get() == NULL);

  // Now put a query-param in, just turning on PageSpeed.  The core filters
  // should be enabled.
  options.reset(GetCustomOptions(
      "http://example.com/?PageSpeed=on",
      &request_headers, NULL));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDeferJavascript));

  // Now explicitly enable a filter, which should disable others.
  options.reset(GetCustomOptions(
      "http://example.com/?PageSpeedFilters=extend_cache",
      &request_headers, NULL));
  ASSERT_TRUE(options.get() != NULL);
  CheckExtendCache(options.get(), true);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDeferJavascript));

  // Now put a request-header in, turning off pagespeed.  request-headers get
  // priority over query-params.
  request_headers.Add("PageSpeed", "off");
  options.reset(GetCustomOptions(
      "http://example.com/?PageSpeed=on",
      &request_headers, NULL));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_FALSE(options->enabled());

  // Now explicitly enable a bogus filter, which should will cause the
  // options to be uncomputable.
  GoogleUrl gurl("http://example.com/?PageSpeedFilters=bogus_filter");
  RewriteQuery rewrite_query;
  EXPECT_FALSE(server_context()->GetQueryOptions(
      &gurl, &request_headers, NULL, &rewrite_query));

  // The default url_namer does not yield any name-derived options, and we
  // have not specified any URL params or request-headers, and kXRequestedWith
  // header is set with bogus value, so there will be no custom options, and no
  // errors.
  request_headers.Add(
      HttpAttributes::kXRequestedWith, "bogus");
  options.reset(
      GetCustomOptions("http://example.com/", &request_headers, NULL));
  ASSERT_TRUE(options.get() == NULL);

  // The default url_namer does not yield any name-derived options, and we
  // have not specified any URL params or request-headers, but kXRequestedWith
  // header is set to 'XmlHttpRequest', so there will be custom options with
  // all js inserting filters disabled.
  request_headers.RemoveAll(HttpAttributes::kXRequestedWith);
  request_headers.Add(
      HttpAttributes::kXRequestedWith, HttpAttributes::kXmlHttpRequest);
  options.reset(
      GetCustomOptions("http://example.com/", &request_headers, NULL));
  // Disable DelayImages for XmlHttpRequests.
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDelayImages));
  // As kDelayImages filter is present in the disabled list, so it will not get
  // enabled even if it is enabled via EnableFilter().
  options->EnableFilter(RewriteOptions::kDelayImages);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDelayImages));

  options->EnableFilter(RewriteOptions::kCachePartialHtml);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCachePartialHtml));
  options->EnableFilter(RewriteOptions::kDeferIframe);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDeferIframe));
  options->EnableFilter(RewriteOptions::kDeferJavascript);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDeferJavascript));
  options->EnableFilter(RewriteOptions::kFlushSubresources);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kFlushSubresources));
  options->EnableFilter(RewriteOptions::kLazyloadImages);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kLazyloadImages));
  options->EnableFilter(RewriteOptions::kLocalStorageCache);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kLocalStorageCache));
  options->EnableFilter(RewriteOptions::kSplitHtml);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kSplitHtml));
  options->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kPrioritizeCriticalCss));
}

TEST_F(ServerContextTest, CustomOptionsWithUrlNamerOptions) {
  // Inject a url-namer that will establish a domain configuration.
  RewriteOptions namer_options(factory()->thread_system());
  namer_options.EnableFilter(RewriteOptions::kCombineJavascript);
  namer_options.EnableFilter(RewriteOptions::kDelayImages);

  RequestHeaders request_headers;
  scoped_ptr<RewriteOptions> options(
      GetCustomOptions("http://example.com/", &request_headers,
                       &namer_options));
  // Even with no query-params or request-headers, we get the custom
  // options as domain options provided as argument.
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kDelayImages));

  // Now combine with query params, which turns core-filters on.
  options.reset(GetCustomOptions(
      "http://example.com/?PageSpeed=on",
      &request_headers, &namer_options));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Explicitly enable a filter in query-params, which will turn off
  // the core filters that have not been explicitly enabled.  Note
  // that explicit filter-setting in query-params overrides completely
  // the options provided as a parameter.
  options.reset(GetCustomOptions(
      "http://example.com/?PageSpeedFilters=combine_css",
      &request_headers, &namer_options));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), false);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Now explicitly enable a bogus filter, which should will cause the
  // options to be uncomputable.
  GoogleUrl gurl("http://example.com/?PageSpeedFilters=bogus_filter");
  RewriteQuery rewrite_query;
  EXPECT_FALSE(server_context()->GetQueryOptions(
      &gurl, &request_headers, NULL, &rewrite_query));

  request_headers.Add(
      HttpAttributes::kXRequestedWith, "bogus");
  options.reset(
      GetCustomOptions("http://example.com/", &request_headers,
                       &namer_options));
  // Don't disable DelayImages for Non-XmlHttpRequests.
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kDelayImages));

  request_headers.RemoveAll(HttpAttributes::kXRequestedWith);
  request_headers.Add(
      HttpAttributes::kXRequestedWith, HttpAttributes::kXmlHttpRequest);
  options.reset(
      GetCustomOptions("http://example.com/", &request_headers,
                       &namer_options));
  // Disable DelayImages for XmlHttpRequests.
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options.get(), false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kDelayImages));
}

TEST_F(ServerContextTest, TestNamed) {
  TestNamed();
}

TEST_F(ServerContextTest, TestOutputInputUrl) {
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  rewrite_driver()->AddFilters();

  GoogleString url = Encode("http://example.com/dir/123/",
                            RewriteOptions::kJavascriptMinId,
                            "0", "orig", "js");
  SetResponseWithDefaultHeaders(
      "http://example.com/dir/123/orig", kContentTypeJavascript,
      "foo() /*comment */;", 100);

  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  TestFetchOutputResource(output_resource, RewriteOptions::kJavascriptMinId,
                          true, "foo();");
}

TEST_F(ServerContextTest, TestOutputInputUrlEvil) {
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  rewrite_driver()->AddFilters();

  GoogleString url = MakeEvilUrl("example.com", "http://www.evil.com");
  SetResponseWithDefaultHeaders(
      "http://www.evil.com/", kContentTypeJavascript,
      "foo() /*comment */;", 100);

  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  TestFetchOutputResource(output_resource, RewriteOptions::kJavascriptMinId,
                          false, "");
}

TEST_F(ServerContextTest, TestOutputInputUrlBusy) {
  EXPECT_TRUE(options()->WriteableDomainLawyer()->AddOriginDomainMapping(
      "www.busy.com", "example.com", "", message_handler()));
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  rewrite_driver()->AddFilters();

  GoogleString url = MakeEvilUrl("example.com", "http://www.busy.com");
  SetResponseWithDefaultHeaders(
      "http://www.busy.com/", kContentTypeJavascript,
      "foo() /*comment */;", 100);

  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  TestFetchOutputResource(output_resource, RewriteOptions::kJavascriptMinId,
                          false, "");
}

// Check that we can origin-map a domain referenced from an HTML file
// to 'localhost', but rewrite-map it to 'cdn.com'.  This was not working
// earlier because RewriteDriver::CreateInputResource was mapping to the
// rewrite domain, preventing us from finding the origin-mapping when
// fetching the URL.
TEST_F(ServerContextTest, TestMapRewriteAndOrigin) {
  ASSERT_TRUE(options()->WriteableDomainLawyer()->AddOriginDomainMapping(
      "localhost", kTestDomain, "", message_handler()));
  EXPECT_TRUE(options()->WriteableDomainLawyer()->AddRewriteDomainMapping(
      "cdn.com", kTestDomain, message_handler()));

  ResourcePtr input(CreateResource(StrCat(kTestDomain, "index.html"),
                                   "style.css"));
  ASSERT_TRUE(input.get() != NULL);
  EXPECT_EQ(StrCat(kTestDomain, "style.css"), input->url());

  // The absolute input URL is in test.com, but we will only be
  // able to serve it from localhost, per the origin mapping above.
  static const char kStyleContent[] = "style content";
  const int kOriginTtlSec = 300;
  SetResponseWithDefaultHeaders("http://localhost/style.css", kContentTypeCss,
                                kStyleContent, kOriginTtlSec);
  EXPECT_TRUE(ReadIfCached(input));

  // When we rewrite the resource as an ouptut, it will show up in the
  // CDN per the rewrite mapping.
  OutputResourcePtr output(
      rewrite_driver()->CreateOutputResourceFromResource(
          RewriteOptions::kCacheExtenderId, rewrite_driver()->default_encoder(),
          NULL, input, kRewrittenResource));
  ASSERT_TRUE(output.get() != NULL);

  // We need to 'Write' an output resource before we can determine its
  // URL.
  rewrite_driver()->Write(
      ResourceVector(), StringPiece(kStyleContent), &kContentTypeCss,
      StringPiece(), output.get());
  EXPECT_EQ(Encode("http://cdn.com/", "ce", "0", "style.css", "css"),
            output->url());
}

TEST_F(ServerContextTest, ScanSplitHtmlRequestSplitEnabled) {
  options()->EnableFilter(RewriteOptions::kSplitHtml);
  RequestContextPtr ctx(CreateRequestContext());
  GoogleString url("http://test.com/?x_split=btf");
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_TRUE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_BELOW_THE_FOLD, ctx->split_request_type());
  EXPECT_EQ("http://test.com/", url);

  url = "http://test.com/?a=b&x_split=btf";
  ctx.reset(CreateRequestContext());
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_TRUE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_BELOW_THE_FOLD, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?a=b", url);

  url = "http://test.com/?a=b&x_split=atf";
  ctx.reset(CreateRequestContext());
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_TRUE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_ABOVE_THE_FOLD, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?a=b", url);

  url = "http://test.com/?a=b&x_split=junk";
  ctx.reset(CreateRequestContext());
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_TRUE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?a=b", url);

  ctx.reset(CreateRequestContext());
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_FALSE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?a=b", url);
}

TEST_F(ServerContextTest, ScanSplitHtmlRequestOptionsNull) {
  RequestContextPtr ctx(CreateRequestContext());
  GoogleString url("http://test.com/?x_split=btf");
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_FALSE(server_context()->ScanSplitHtmlRequest(ctx, NULL, &url));
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?x_split=btf", url);
}

TEST_F(ServerContextTest, ScanSplitHtmlRequestSplitDisabled) {
  options()->DisableFilter(RewriteOptions::kSplitHtml);
  RequestContextPtr ctx(CreateRequestContext());
  GoogleString url("http://test.com/?x_split=btf");
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_FALSE(server_context()->ScanSplitHtmlRequest(ctx, options(), &url));
  EXPECT_EQ(RequestContext::SPLIT_FULL, ctx->split_request_type());
  EXPECT_EQ("http://test.com/?x_split=btf", url);
}

class MockRewriteFilter : public RewriteFilter {
 public:
  explicit MockRewriteFilter(RewriteDriver* driver)
      : RewriteFilter(driver) {}
  virtual ~MockRewriteFilter() {}
  virtual const char* id() const { return "mk"; }
  virtual const char* Name() const { return "mock_filter"; }
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRewriteFilter);
};

class CreateMockRewriterCallback
    : public TestRewriteDriverFactory::CreateRewriterCallback {
 public:
  CreateMockRewriterCallback() {}
  virtual ~CreateMockRewriterCallback() {}
  virtual RewriteFilter* Done(RewriteDriver* driver) {
    return new MockRewriteFilter(driver);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CreateMockRewriterCallback);
};

class MockPlatformConfigCallback
    : public TestRewriteDriverFactory::PlatformSpecificConfigurationCallback {
 public:
  explicit MockPlatformConfigCallback(RewriteDriver** result_ptr)
      : result_ptr_(result_ptr) {
  }

  virtual void Done(RewriteDriver* driver) {
    *result_ptr_ = driver;
  }

 private:
  RewriteDriver** result_ptr_;
  DISALLOW_COPY_AND_ASSIGN(MockPlatformConfigCallback);
};

// Tests that platform-specific configuration hook runs for various
// factory methods.
TEST_F(ServerContextTest, TestPlatformSpecificConfiguration) {
  RewriteDriver* rec_normal_driver = NULL;
  RewriteDriver* rec_custom_driver = NULL;

  MockPlatformConfigCallback normal_callback(&rec_normal_driver);
  MockPlatformConfigCallback custom_callback(&rec_custom_driver);

  factory()->AddPlatformSpecificConfigurationCallback(&normal_callback);
  RewriteDriver* normal_driver = server_context()->NewRewriteDriver(
      RequestContext::NewTestRequestContext(server_context()->thread_system()));
  EXPECT_EQ(normal_driver, rec_normal_driver);
  factory()->ClearPlatformSpecificConfigurationCallback();
  normal_driver->Cleanup();

  factory()->AddPlatformSpecificConfigurationCallback(&custom_callback);
  RewriteDriver* custom_driver =
      server_context()->NewCustomRewriteDriver(
          new RewriteOptions(factory()->thread_system()),
          RequestContext::NewTestRequestContext(
              server_context()->thread_system()));
  EXPECT_EQ(custom_driver, rec_custom_driver);
  custom_driver->Cleanup();
}

// Tests that platform-specific rewriters are used for decoding fetches.
TEST_F(ServerContextTest, TestPlatformSpecificRewritersDecoding) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "mk", "0", "orig", "js");
  GoogleUrl gurl(url);
  RewriteFilter* dummy;

  // Without the mock rewriter enabled, this URL should not be decoded.
  OutputResourcePtr bad_output(
      decoding_driver()->DecodeOutputResource(gurl, &dummy));
  ASSERT_TRUE(bad_output.get() == NULL);

  // With the mock rewriter enabled, this URL should be decoded.
  CreateMockRewriterCallback callback;
  factory()->AddCreateRewriterCallback(&callback);
  factory()->set_add_platform_specific_decoding_passes(true);
  factory()->RebuildDecodingDriverForTests(server_context());
  // TODO(sligocki): Do we still want to expose decoding_driver() for
  // platform-specific rewriters? Or should we just use IsPagespeedResource()
  // in these tests?
  OutputResourcePtr good_output(
      decoding_driver()->DecodeOutputResource(gurl, &dummy));
  ASSERT_TRUE(good_output.get() != NULL);
  EXPECT_EQ(url, good_output->url());
}

// Tests that platform-specific rewriters are used for decoding fetches even
// if they are only added in AddPlatformSpecificRewritePasses, not
// AddPlatformSpecificDecodingPasses.  Required for backwards compatibility.
TEST_F(ServerContextTest, TestPlatformSpecificRewritersImplicitDecoding) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "mk", "0", "orig", "js");
  GoogleUrl gurl(url);
  RewriteFilter* dummy;

  // The URL should be decoded even if AddPlatformSpecificDecodingPasses is
  // suppressed.
  CreateMockRewriterCallback callback;
  factory()->AddCreateRewriterCallback(&callback);
  factory()->set_add_platform_specific_decoding_passes(false);
  factory()->RebuildDecodingDriverForTests(server_context());
  OutputResourcePtr good_output(
      decoding_driver()->DecodeOutputResource(gurl, &dummy));
  ASSERT_TRUE(good_output.get() != NULL);
  EXPECT_EQ(url, good_output->url());
}

// DecodeOutputResource should drop query
TEST_F(ServerContextTest, TestOutputResourceFetchQuery) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  RewriteFilter* dummy;
  GoogleUrl gurl(StrCat(url, "?query"));
  OutputResourcePtr output_resource(
      rewrite_driver()->DecodeOutputResource(gurl, &dummy));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(url, output_resource->url());
}

// Input resources and corresponding output resources should keep queries
TEST_F(ServerContextTest, TestInputResourceQuery) {
  const char kUrl[] = "test?param";
  ResourcePtr resource(CreateResource(kResourceUrlBase, kUrl));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(StrCat(GoogleString(kResourceUrlBase), "/", kUrl), resource->url());
  OutputResourcePtr output(rewrite_driver()->CreateOutputResourceFromResource(
      "sf", rewrite_driver()->default_encoder(), NULL, resource,
      kRewrittenResource));
  ASSERT_TRUE(output.get() != NULL);

  GoogleString included_name;
  EXPECT_TRUE(UrlEscaper::DecodeFromUrlSegment(output->name(), &included_name));
  EXPECT_EQ(GoogleString(kUrl), included_name);
}

TEST_F(ServerContextTest, TestRemember404) {
  // Make sure our resources remember that a page 404'd, but not too long.
  http_cache()->set_remember_not_cacheable_ttl_seconds(10000);
  http_cache()->set_remember_fetch_failed_ttl_seconds(100);

  ResponseHeaders not_found;
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &not_found);
  not_found.SetStatusAndReason(HttpStatus::kNotFound);
  SetFetchResponse("http://example.com/404", not_found, "");

  ResourcePtr resource(
      CreateInputResourceAndReadIfCached("http://example.com/404"));
  EXPECT_EQ(NULL, resource.get());

  HTTPValue value_out;
  ResponseHeaders headers_out;
  EXPECT_EQ(HTTPCache::kRecentFetchFailed, HttpBlockingFind(
      "http://example.com/404", http_cache(), &value_out, &headers_out));
  AdvanceTimeMs(150 * Timer::kSecondMs);

  EXPECT_EQ(HTTPCache::kNotFound, HttpBlockingFind(
      "http://example.com/404", http_cache(), &value_out, &headers_out));
}

TEST_F(ServerContextTest, TestRememberDropped) {
  // Fake resource being dropped by adding the appropriate header to the
  // resource proper.
  ResponseHeaders not_found;
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &not_found);
  not_found.SetStatusAndReason(HttpStatus::kNotFound);
  not_found.Add(HttpAttributes::kXPsaLoadShed, "1");
  SetFetchResponse("http://example.com/404", not_found, "");

  ResourcePtr resource(
      CreateInputResourceAndReadIfCached("http://example.com/404"));
  EXPECT_EQ(NULL, resource.get());

  HTTPValue value_out;
  ResponseHeaders headers_out;
  EXPECT_EQ(HTTPCache::kRecentFetchFailed, HttpBlockingFind(
      "http://example.com/404", http_cache(), &value_out, &headers_out));

  AdvanceTimeMs(11 * Timer::kSecondMs);
  EXPECT_EQ(HTTPCache::kNotFound, HttpBlockingFind(
      "http://example.com/404", http_cache(), &value_out, &headers_out));
}

TEST_F(ServerContextTest, TestNonCacheable) {
  const GoogleString kContents = "ok";

  // Make sure that when we get non-cacheable resources
  // we mark the fetch as not cacheable in the cache.
  ResponseHeaders no_cache;
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &no_cache);
  no_cache.Replace(HttpAttributes::kCacheControl, "no-cache");
  no_cache.ComputeCaching();
  SetFetchResponse("http://example.com/", no_cache, kContents);

  ResourcePtr resource(CreateResource("http://example.com/", "/"));
  ASSERT_TRUE(resource.get() != NULL);

  VerifyContentsCallback callback(resource, kContents);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  callback.AssertCalled();

  HTTPValue value_out;
  ResponseHeaders headers_out;
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable, HttpBlockingFind(
      "http://example.com/", http_cache(), &value_out, &headers_out));
}

TEST_F(ServerContextTest, TestNonCacheableReadResultPolicy) {
  // Make sure we report the success/failure for non-cacheable resources
  // depending on the policy. (TestNonCacheable also covers the value).

  ResponseHeaders no_cache;
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &no_cache);
  no_cache.Replace(HttpAttributes::kCacheControl, "no-cache");
  no_cache.ComputeCaching();
  SetFetchResponse("http://example.com/", no_cache, "stuff");

  ResourcePtr resource1(CreateResource("http://example.com/", "/"));
  ASSERT_TRUE(resource1.get() != NULL);
  MockResourceCallback callback1(resource1, factory()->thread_system());
  resource1->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback1);
  EXPECT_TRUE(callback1.done());
  EXPECT_FALSE(callback1.success());

  ResourcePtr resource2(CreateResource("http://example.com/", "/"));
  ASSERT_TRUE(resource2.get() != NULL);
  MockResourceCallback callback2(resource2, factory()->thread_system());
  resource2->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
}

TEST_F(ServerContextTest, TestVaryOption) {
  // Make sure that when we get non-cacheable resources
  // we mark the fetch as not-cacheable in the cache.
  options()->set_respect_vary(true);
  ResponseHeaders no_cache;
  const GoogleString kContents = "ok";
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &no_cache);
  no_cache.Add(HttpAttributes::kVary, HttpAttributes::kAcceptEncoding);
  no_cache.Add(HttpAttributes::kVary, HttpAttributes::kUserAgent);
  no_cache.ComputeCaching();
  SetFetchResponse("http://example.com/", no_cache, kContents);

  ResourcePtr resource(CreateResource("http://example.com/", "/"));
  ASSERT_TRUE(resource.get() != NULL);

  VerifyContentsCallback callback(resource, kContents);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  callback.AssertCalled();
  EXPECT_FALSE(resource->IsValidAndCacheable());

  HTTPValue valueOut;
  ResponseHeaders headersOut;
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable, HttpBlockingFind(
      "http://example.com/", http_cache(), &valueOut, &headersOut));
}

TEST_F(ServerContextTest, TestOutlined) {
  // Outliner resources should not produce extra cache traffic
  // due to rname/ entries we can't use anyway.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver()->CreateOutputResourceWithPath(
          kUrlPrefix, CssOutlineFilter::kFilterId, "_", kOutlinedResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  rewrite_driver()->Write(
      ResourceVector(), "", &kContentTypeCss, StringPiece(),
      output_resource.get());
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Now try fetching again. It should not get a cached_result either.
  output_resource.reset(
      rewrite_driver()->CreateOutputResourceWithPath(
          kUrlPrefix, CssOutlineFilter::kFilterId, "_", kOutlinedResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

TEST_F(ServerContextTest, TestOnTheFly) {
  // Test to make sure that an on-fly insert does not insert the data,
  // just the rname/

  // For derived resources we can and should use the rewrite
  // summary/metadata cache
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver()->CreateOutputResourceWithPath(
          kUrlPrefix, RewriteOptions::kCssFilterId, "_", kOnTheFlyResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  rewrite_driver()->Write(
      ResourceVector(), "", &kContentTypeCss, StringPiece(),
      output_resource.get());
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

TEST_F(ServerContextTest, TestNotGenerated) {
  // For derived resources we can and should use the rewrite
  // summary/metadata cache
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver()->CreateOutputResourceWithPath(
          kUrlPrefix, RewriteOptions::kCssFilterId, "_", kRewrittenResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  rewrite_driver()->Write(
      ResourceVector(), "", &kContentTypeCss, StringPiece(),
      output_resource.get());
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

TEST_F(ServerContextTest, TestHandleBeaconNoLoadParam) {
  EXPECT_FALSE(server_context()->HandleBeacon(
      "", UserAgentMatcherTestBase::kChromeUserAgent,
      CreateRequestContext()));
}

TEST_F(ServerContextTest, TestHandleBeaconInvalidLoadParam) {
  EXPECT_FALSE(server_context()->HandleBeacon(
      "ets=asd", UserAgentMatcherTestBase::kChromeUserAgent,
      CreateRequestContext()));
}

TEST_F(ServerContextTest, TestHandleBeaconNoUrl) {
  EXPECT_FALSE(server_context()->HandleBeacon(
      "ets=load:34", UserAgentMatcherTestBase::kChromeUserAgent,
      CreateRequestContext()));
}

TEST_F(ServerContextTest, TestHandleBeaconInvalidUrl) {
  EXPECT_FALSE(server_context()->HandleBeacon(
      "url=%2f%2finvalidurl&ets=load:34",
      UserAgentMatcherTestBase::kChromeUserAgent, CreateRequestContext()));
}

TEST_F(ServerContextTest, TestHandleBeaconMissingValue) {
  EXPECT_FALSE(server_context()->HandleBeacon(
      "url=http%3A%2F%2Flocalhost%3A8080%2Findex.html&ets=load:",
      UserAgentMatcherTestBase::kChromeUserAgent, CreateRequestContext()));
}

TEST_F(ServerContextTest, TestHandleBeacon) {
  EXPECT_TRUE(server_context()->HandleBeacon(
      "url=http%3A%2F%2Flocalhost%3A8080%2Findex.html&ets=load:34",
      UserAgentMatcherTestBase::kChromeUserAgent, CreateRequestContext()));
}

class BeaconTest : public ServerContextTest {
 protected:
  BeaconTest() : property_cache_(NULL) { }
  virtual ~BeaconTest() { }

  virtual void SetUp() {
    ServerContextTest::SetUp();

    property_cache_ = server_context()->page_property_cache();
    property_cache_->set_enabled(true);
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(property_cache_, RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    server_context()->set_critical_images_finder(
        new BeaconCriticalImagesFinder(
            beacon_cohort, factory()->nonce_generator(), statistics()));
    server_context()->set_critical_selector_finder(
        new BeaconCriticalSelectorFinder(beacon_cohort,
                                         factory()->nonce_generator(),
                                         statistics()));
    ResetDriver();
    candidates_.insert("#foo");
    candidates_.insert(".bar");
    candidates_.insert("img");
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    SetDummyRequestHeaders();
  }

  MockPropertyPage* MockPageForUA(StringPiece user_agent) {
    UserAgentMatcher::DeviceType device_type =
        server_context()->user_agent_matcher()->GetDeviceTypeForUA(
            user_agent);
    MockPropertyPage* page = NewMockPage(
        kUrlPrefix, kOptionsHash, device_type);
    property_cache_->Read(page);
    return page;
  }

  void InsertCssBeacon(StringPiece user_agent) {
    // Simulate effects on pcache of CSS beacon insertion.
    rewrite_driver()->set_property_page(MockPageForUA(user_agent));
    factory()->mock_timer()->AdvanceMs(
        options()->beacon_reinstrument_time_sec() * Timer::kSecondMs);
    last_beacon_metadata_ =
        server_context()->critical_selector_finder()->
            PrepareForBeaconInsertion(candidates_, rewrite_driver());
    ASSERT_EQ(kBeaconWithNonce, last_beacon_metadata_.status);
    ASSERT_FALSE(last_beacon_metadata_.nonce.empty());
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
  }

  void InsertImageBeacon(StringPiece user_agent) {
    // Simulate effects on pcache of image beacon insertion.
    rewrite_driver()->set_property_page(MockPageForUA(user_agent));
    // Some of the critical image tests send enough beacons with the same set of
    // images that we can go into low frequency beaconing mode, so advance time
    // by the low frequency rebeacon interval.
    factory()->mock_timer()->AdvanceMs(
        options()->beacon_reinstrument_time_sec() * Timer::kSecondMs *
        kLowFreqBeaconMult);
    last_beacon_metadata_ =
        server_context()->critical_images_finder()->
            PrepareForBeaconInsertion(rewrite_driver());
    ASSERT_EQ(kBeaconWithNonce, last_beacon_metadata_.status);
    ASSERT_FALSE(last_beacon_metadata_.nonce.empty());
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
  }

  // Send a beacon through ServerContext::HandleBeacon and verify that the
  // property cache entries for critical images, critical selectors and rendered
  // dimensions of images were updated correctly.
  void TestBeacon(const StringSet* critical_image_hashes,
                  const StringSet* critical_css_selectors,
                  const GoogleString* rendered_images_json_map,
                  StringPiece user_agent) {
    ASSERT_EQ(kBeaconWithNonce, last_beacon_metadata_.status)
        << "Remember to insert a beacon!";
    // Setup the beacon_url and pass to HandleBeacon.
    GoogleString beacon_url = StrCat(
        "url=http%3A%2F%2Fwww.example.com"
        "&oh=", kOptionsHash, "&n=", last_beacon_metadata_.nonce);
    if (critical_image_hashes != NULL) {
      StrAppend(&beacon_url, "&ci=");
      AppendJoinCollection(&beacon_url, *critical_image_hashes, ",");
    }
    if (critical_css_selectors != NULL) {
      StrAppend(&beacon_url, "&cs=");
      AppendJoinCollection(&beacon_url, *critical_css_selectors, ",");
    }
    if (rendered_images_json_map != NULL) {
      StrAppend(&beacon_url, "&rd=", *rendered_images_json_map);
    }
    EXPECT_TRUE(server_context()->HandleBeacon(
        beacon_url,
        user_agent,
        CreateRequestContext()));

    // Read the property cache value for critical images, and verify that it has
    // the expected value.
    ResetDriver();
    scoped_ptr<MockPropertyPage> page(MockPageForUA(user_agent));
    rewrite_driver()->set_property_page(page.release());
    if (critical_image_hashes != NULL) {
      critical_html_images_ = server_context()->critical_images_finder()->
          GetHtmlCriticalImages(rewrite_driver());
    }
    if (critical_css_selectors != NULL) {
      critical_css_selectors_ = server_context()->critical_selector_finder()->
          GetCriticalSelectors(rewrite_driver());
    }

    if (rendered_images_json_map != NULL) {
      rendered_images_.reset(server_context()->critical_images_finder()->
                             ExtractRenderedImageDimensionsFromCache(
                                 rewrite_driver()));
    }
  }

  PropertyCache* property_cache_;
  // These fields hold data deserialized from the pcache after TestBeacon.
  StringSet critical_html_images_;
  StringSet critical_css_selectors_;
  // This field holds the data deserialized from pcache after a BeaconTest call.
  scoped_ptr<RenderedImages> rendered_images_;
  // This field holds candidate critical css selectors.
  StringSet candidates_;
  BeaconMetadata last_beacon_metadata_;
};

TEST_F(BeaconTest, BasicPcacheSetup) {
  const PropertyCache::Cohort* cohort = property_cache_->GetCohort(
      RewriteDriver::kBeaconCohort);
  UserAgentMatcher::DeviceType device_type =
      server_context()->user_agent_matcher()->GetDeviceTypeForUA(
          UserAgentMatcherTestBase::kChromeUserAgent);
  scoped_ptr<MockPropertyPage> page(
      NewMockPage(kUrlPrefix, kOptionsHash, device_type));
  property_cache_->Read(page.get());
  PropertyValue* property = page->GetProperty(cohort, "critical_images");
  EXPECT_FALSE(property->has_value());
}

TEST_F(BeaconTest, HandleBeaconRenderedDimensionsofImages) {
  GoogleString img1 = "http://www.example.com/img1.png";
  GoogleString hash1 = IntegerToString(
      HashString<CasePreserve, int>(img1.c_str(), img1.size()));
  options()->EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  RenderedImages rendered_images;
  RenderedImages_Image* images = rendered_images.add_image();
  images->set_src(hash1);
  images->set_rendered_width(40);
  images->set_rendered_height(50);
  GoogleString json_map_rendered_dimensions = StrCat(
      "{\"", hash1, "\":{\"rw\":40,", "\"rh\":50,\"ow\":160,\"oh\":200}}");
  InsertImageBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  TestBeacon(NULL, NULL, &json_map_rendered_dimensions,
              UserAgentMatcherTestBase::kChromeUserAgent);
  ASSERT_FALSE(rendered_images_.get() == NULL);
  EXPECT_EQ(1, rendered_images_->image_size());
  EXPECT_STREQ(hash1, rendered_images_->image(0).src());
  EXPECT_EQ(40, rendered_images_->image(0).rendered_width());
  EXPECT_EQ(50, rendered_images_->image(0).rendered_height());
}

TEST_F(BeaconTest, HandleBeaconCritImages) {
  GoogleString img1 = "http://www.example.com/img1.png";
  GoogleString img2 = "http://www.example.com/img2.png";
  GoogleString hash1 = IntegerToString(
      HashString<CasePreserve, int>(img1.c_str(), img1.size()));
  GoogleString hash2 = IntegerToString(
      HashString<CasePreserve, int>(img2.c_str(), img2.size()));

  StringSet critical_image_hashes;
  critical_image_hashes.insert(hash1);
  InsertImageBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  TestBeacon(&critical_image_hashes, NULL, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_STREQ(hash1, JoinCollection(critical_html_images_, ","));

  // Beacon both images as critical.  Since we require 80% support, img2 won't
  // show as critical until we've beaconed four times.  It doesn't require five
  // beacon results because we weight recent beacon values more heavily and
  // beacon support decays over time.
  critical_image_hashes.insert(hash2);
  for (int i = 0; i < 3; ++i) {
    InsertImageBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
    TestBeacon(&critical_image_hashes, NULL, NULL,
               UserAgentMatcherTestBase::kChromeUserAgent);
    EXPECT_STREQ(hash1, JoinCollection(critical_html_images_, ","));
  }
  GoogleString expected = StrCat(hash1, ",", hash2);
  InsertImageBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  TestBeacon(&critical_image_hashes, NULL, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_STREQ(expected, JoinCollection(critical_html_images_, ","));

  // Test with a different user agent, providing support only for img1.
  critical_image_hashes.clear();
  critical_image_hashes.insert(hash1);
  InsertImageBeacon(UserAgentMatcherTestBase::kIPhoneUserAgent);
  TestBeacon(&critical_image_hashes, NULL, NULL,
             UserAgentMatcherTestBase::kIPhoneUserAgent);
  EXPECT_STREQ(hash1, JoinCollection(critical_html_images_, ","));

  // Beacon once more with the original user agent and with only img1; img2
  // loses 80% support again.
  InsertImageBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  TestBeacon(&critical_image_hashes, NULL, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_STREQ(hash1, JoinCollection(critical_html_images_, ","));
}

TEST_F(BeaconTest, HandleBeaconCriticalCss) {
  InsertCssBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  StringSet critical_css_selector;
  critical_css_selector.insert("#foo");
  critical_css_selector.insert(".bar");
  critical_css_selector.insert("#noncandidate");
  TestBeacon(NULL, &critical_css_selector, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_STREQ("#foo,.bar",
               JoinCollection(critical_css_selectors_, ","));

  // Send another beacon response, and make sure we are storing a history of
  // responses.
  InsertCssBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  critical_css_selector.clear();
  critical_css_selector.insert(".bar");
  critical_css_selector.insert("img");
  critical_css_selector.insert("#noncandidate");
  TestBeacon(NULL, &critical_css_selector, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_STREQ("#foo,.bar,img",
               JoinCollection(critical_css_selectors_, ","));
}

TEST_F(BeaconTest, EmptyCriticalCss) {
  InsertCssBeacon(UserAgentMatcherTestBase::kChromeUserAgent);
  StringSet empty_critical_selectors;
  TestBeacon(NULL, &empty_critical_selectors, NULL,
             UserAgentMatcherTestBase::kChromeUserAgent);
  EXPECT_TRUE(critical_css_selectors_.empty());
}

class ResourceFreshenTest : public ServerContextTest {
 protected:
  static const char kContents[];

  virtual void SetUp() {
    ServerContextTest::SetUp();
    HTTPCache::InitStats(statistics());
    expirations_ = statistics()->GetVariable(HTTPCache::kCacheExpirations);
    CHECK(expirations_ != NULL);
    SetDefaultLongCacheHeaders(&kContentTypePng, &response_headers_);
    response_headers_.SetStatusAndReason(HttpStatus::kOK);
    response_headers_.RemoveAll(HttpAttributes::kCacheControl);
    response_headers_.RemoveAll(HttpAttributes::kExpires);
  }

  Variable* expirations_;
  ResponseHeaders response_headers_;
};

const char ResourceFreshenTest::kContents[] = "ok";

// Many resources expire in 5 minutes, because that is our default for
// when caching headers are not present.  This test ensures that iff
// we ask for the resource when there's just a minute left, we proactively
// fetch it rather than allowing it to expire.
TEST_F(ResourceFreshenTest, TestFreshenImminentlyExpiringResources) {
  SetupWaitFetcher();
  FetcherUpdateDateHeaders();

  // Make sure we don't try to insert non-cacheable resources
  // into the cache wastefully, but still fetch them well.
  int max_age_sec =
      ResponseHeaders::kDefaultImplicitCacheTtlMs / Timer::kSecondMs;
  response_headers_.Add(HttpAttributes::kCacheControl,
                       StringPrintf("max-age=%d", max_age_sec));
  SetFetchResponse(kResourceUrl, response_headers_, "");

  // The test here is not that the ReadIfCached will succeed, because
  // it's a fake url fetcher.
  StartRead();
  CallFetcherCallbacks();
  EXPECT_TRUE(ResourceIsCached());

  // Now let the time expire with no intervening fetches to freshen the cache.
  // This is because we do not proactively initiate refreshes for all resources;
  // only the ones that are actually asked for on a regular basis.  So a
  // completely inactive site will not see its resources freshened.
  AdvanceTimeMs((max_age_sec + 1) * Timer::kSecondMs);
  expirations_->Clear();
  StartRead();
  EXPECT_EQ(1, expirations_->Get());
  expirations_->Clear();
  CallFetcherCallbacks();
  EXPECT_TRUE(ResourceIsCached());

  // But if we have just a little bit of traffic then when we get a request
  // for a soon-to-expire resource it will auto-freshen.
  AdvanceTimeMs((1 + (max_age_sec * 4) / 5) * Timer::kSecondMs);
  EXPECT_TRUE(ResourceIsCached());
  CallFetcherCallbacks();  // freshens cache.
  AdvanceTimeMs((max_age_sec / 5) * Timer::kSecondMs);
  EXPECT_TRUE(ResourceIsCached());  // Yay, no cache misses after 301 seconds
  EXPECT_EQ(0, expirations_->Get());
}

// Tests that freshining will not be performed when we have caching
// forced.  Nothing will ever be evicted due to time, so there is no
// need to freshen.
TEST_F(ResourceFreshenTest, NoFreshenOfForcedCachedResources) {
  const GoogleString kContents = "ok";
  http_cache()->set_force_caching(true);
  FetcherUpdateDateHeaders();

  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0");
  SetFetchResponse(kResourceUrl, response_headers_, "");

  // We should get just 1 fetch.  If we were aggressively freshening
  // we would get 2.
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // There should be no extra fetches required because our cache is
  // still active.  We shouldn't have needed an extra fetch to freshen,
  // either, because the cache expiration time is irrelevant -- we are
  // forcing caching so we consider the resource to always be fresh.
  // So even after an hour we should have no expirations.
  AdvanceTimeMs(1 * Timer::kHourMs);
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Nothing expires with force-caching on.
  EXPECT_EQ(0, expirations_->Get());
}

// Tests that freshining will not occur for short-lived resources,
// which could impact the performance of the server.
TEST_F(ResourceFreshenTest, NoFreshenOfShortLivedResources) {
  const GoogleString kContents = "ok";
  FetcherUpdateDateHeaders();

  int max_age_sec =
      ResponseHeaders::kDefaultImplicitCacheTtlMs / Timer::kSecondMs - 1;
  response_headers_.Add(HttpAttributes::kCacheControl,
                       StringPrintf("max-age=%d", max_age_sec));
  SetFetchResponse(kResourceUrl, response_headers_, "");

  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // There should be no extra fetches required because our cache is
  // still active.  We shouldn't have needed an extra fetch to freshen,
  // either.
  AdvanceTimeMs((max_age_sec - 1) * Timer::kSecondMs);
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, expirations_->Get());

  // Now let the resource expire.  We'll need another fetch since we did not
  // freshen.
  AdvanceTimeMs(2 * Timer::kSecondMs);
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, expirations_->Get());
}

class ServerContextShardedTest : public ServerContextTest {
 protected:
  virtual void SetUp() {
    ServerContextTest::SetUp();
    EXPECT_TRUE(options()->WriteableDomainLawyer()->AddShard(
        "example.com", "shard0.com,shard1.com", message_handler()));
  }
};

TEST_F(ServerContextShardedTest, TestNamed) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  OutputResourcePtr output_resource(
      rewrite_driver()->CreateOutputResourceWithPath(
          "http://example.com/dir/",
          "jm",
          "orig.js",
          kRewrittenResource));
  ASSERT_TRUE(output_resource.get());
  ASSERT_TRUE(rewrite_driver()->Write(ResourceVector(),
                                      "alert('hello');",
                                      &kContentTypeJavascript,
                                      StringPiece(),
                                      output_resource.get()));

  // This always gets mapped to shard0 because we are using the mock
  // hasher for the content hash.  Note that the sharding sensitivity
  // to the hash value is tested in DomainLawyerTest.Shard, and will
  // also be covered in a system test.
  EXPECT_EQ(Encode("http://shard0.com/dir/", "jm", "0", "orig.js", "js"),
            output_resource->url());
}

TEST_F(ServerContextTest, TestMergeNonCachingResponseHeaders) {
  ResponseHeaders input, output;
  input.Add("X-Extra-Header", "Extra Value");  // should be copied to output
  input.Add(HttpAttributes::kCacheControl, "max-age=300");  // should not be
  server_context()->MergeNonCachingResponseHeaders(input, &output);
  ConstStringStarVector v;
  EXPECT_FALSE(output.Lookup(HttpAttributes::kCacheControl, &v));
  ASSERT_TRUE(output.Lookup("X-Extra-Header", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("Extra Value", *v[0]);
}

TEST_F(ServerContextTest, ApplyInputCacheControl) {
  ResourcePtr public_100(
      CreateCustomCachingResource("pub_100", 100, ""));
  ResourcePtr public_200(
      CreateCustomCachingResource("pub_200", 200, ""));
  ResourcePtr private_300(
      CreateCustomCachingResource("pri_300", 300, ",private"));
  ResourcePtr private_400(
      CreateCustomCachingResource("pri_400", 400, ",private"));
  ResourcePtr no_cache_150(
      CreateCustomCachingResource("noc_150", 400, ",no-cache"));
  ResourcePtr no_store_200(
      CreateCustomCachingResource("nos_200", 200, ",no-store"));

  {
    // If we feed in just public resources, we should get something with
    // ultra-long TTL, regardless of how soon they expire.
    ResponseHeaders out;
    DefaultHeaders(&out);
    ResourceVector two_public;
    two_public.push_back(public_100);
    two_public.push_back(public_200);
    server_context()->ApplyInputCacheControl(two_public, &out);

    GoogleString expect_ttl = StrCat(
        "max-age=",
        Integer64ToString(ServerContext::kGeneratedMaxAgeMs /
                            Timer::kSecondMs));
    EXPECT_STREQ(expect_ttl, out.Lookup1(HttpAttributes::kCacheControl));
  }

  {
    // If an input is private, however, we must mark output appropriately
    // and not cache-extend.
    ResponseHeaders out;
    DefaultHeaders(&out);
    ResourceVector some_private;
    some_private.push_back(public_100);
    some_private.push_back(private_300);
    some_private.push_back(private_400);
    server_context()->ApplyInputCacheControl(some_private, &out);
    EXPECT_FALSE(out.HasValue(HttpAttributes::kCacheControl, "public"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "private"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "max-age=100"));
  }

  {
    // Similarly no-cache should be incorporated --- but then we also need
    // to have 0 ttl.
    ResponseHeaders out;
    DefaultHeaders(&out);
    ResourceVector some_nocache;
    some_nocache.push_back(public_100);
    some_nocache.push_back(private_300);
    some_nocache.push_back(private_400);
    some_nocache.push_back(no_cache_150);
    server_context()->ApplyInputCacheControl(some_nocache, &out);
    EXPECT_FALSE(out.HasValue(HttpAttributes::kCacheControl, "public"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "no-cache"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
  }

  {
    // Make sure we save no-store as well.
    ResponseHeaders out;
    DefaultHeaders(&out);
    ResourceVector some_nostore;
    some_nostore.push_back(public_100);
    some_nostore.push_back(private_300);
    some_nostore.push_back(private_400);
    some_nostore.push_back(no_cache_150);
    some_nostore.push_back(no_store_200);
    server_context()->ApplyInputCacheControl(some_nostore, &out);
    EXPECT_FALSE(out.HasValue(HttpAttributes::kCacheControl, "public"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "no-cache"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "no-store"));
    EXPECT_TRUE(out.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
  }
}

TEST_F(ServerContextTest, WriteChecksInputVector) {
  // Make sure ->Write incorporates the cache control info from inputs,
  // and doesn't cache a private resource improperly. Also make sure
  // we get the charset right (including quoting).
  ResourcePtr private_400(
      CreateCustomCachingResource("pri_400", 400, ",private"));
  // Should have the 'it's not cacheable!' entry here; see also below.
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  OutputResourcePtr output_resource(
      rewrite_driver()->CreateOutputResourceFromResource(
          "cf", rewrite_driver()->default_encoder(), NULL /* no context*/,
          private_400, kRewrittenResource));


  rewrite_driver()->Write(ResourceVector(1, private_400),
                          "boo!",
                          &kContentTypeText,
                          "\"\\koi8-r\"",  // covers escaping behavior, too.
                          output_resource.get());
  ResponseHeaders* headers = output_resource->response_headers();
  EXPECT_FALSE(headers->HasValue(HttpAttributes::kCacheControl, "public"));
  EXPECT_TRUE(headers->HasValue(HttpAttributes::kCacheControl, "private"));
  EXPECT_TRUE(headers->HasValue(HttpAttributes::kCacheControl, "max-age=400"));
  EXPECT_STREQ("text/plain; charset=\"\\koi8-r\"",
               headers->Lookup1(HttpAttributes::kContentType));

  // Make sure nothing extra in the cache at this point.
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
}

TEST_F(ServerContextTest, IsPagespeedResource) {
  GoogleUrl rewritten(Encode("http://shard0.com/dir/", "jm", "0",
                             "orig.js", "js"));
  EXPECT_TRUE(server_context()->IsPagespeedResource(rewritten));

  GoogleUrl normal("http://jqueryui.com/jquery-1.6.2.js");
  EXPECT_FALSE(server_context()->IsPagespeedResource(normal));
}

TEST_F(ServerContextTest, PartlyFailedFetch) {
  // Regression test for invalid Resource state when the fetch physically
  // succeeds but does not get added to cache due to invalid cacheability.
  // In that case, we would end up with headers claiming successful fetch,
  // but an HTTPValue without headers set (which would also crash on
  // access if no data was emitted by fetcher via Write).
  static const char kCssName[] = "a.css";
  GoogleString abs_url = AbsolutifyUrl(kCssName);
  ResponseHeaders non_cacheable;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &non_cacheable);
  non_cacheable.SetDateAndCaching(start_time_ms() /* date */, 0 /* ttl */,
                                  "private, no-cache");
  non_cacheable.ComputeCaching();
  SetFetchResponse(abs_url, non_cacheable, "");

  // We tell the fetcher to quash the zero-bytes writes, as that behavior
  // (which Serf has) made the bug more severe, with not only
  // loaded() and HttpStatusOk() lying, but also contents() crashing.
  mock_url_fetcher()->set_omit_empty_writes(true);

  // We tell the fetcher to output the headers and then immediately fail.
  mock_url_fetcher()->set_fail_after_headers(true);

  GoogleUrl gurl(abs_url);
  SetBaseUrlForFetch(abs_url);
  ResourcePtr resource = rewrite_driver()->CreateInputResource(gurl);
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource, factory()->thread_system());
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_FALSE(resource->IsValidAndCacheable());
  EXPECT_FALSE(resource->loaded());
  EXPECT_FALSE(resource->HttpStatusOk())
    << " Unexpectedly got access to resource contents:" << resource->contents();
}

TEST_F(ServerContextTest, LoadFromFileReadAsync) {
  // This reads a resource twice, to make sure that there is no misbehavior
  // (read: check failures or crashes) when cache invalidation logic tries to
  // deal with FileInputResource.
  const char kContents[] = "lots of bits of data";
  options()->file_load_policy()->Associate("http://test.com/", "/test/");

  GoogleUrl test_url("http://test.com/a.css");

  // Init file resources.
  WriteFile("/test/a.css", kContents);

  SetBaseUrlForFetch("http://test.com");
  ResourcePtr resource(
      rewrite_driver()->CreateInputResource(test_url));
  VerifyContentsCallback callback(resource, kContents);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  callback.AssertCalled();

  resource = rewrite_driver()->CreateInputResource(test_url);
  VerifyContentsCallback callback2(resource, kContents);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback2);
  callback2.AssertCalled();
}

namespace {

void CheckMatchesHeaders(const ResponseHeaders& headers,
                         const InputInfo& input) {
  ASSERT_TRUE(input.has_type());
  EXPECT_EQ(InputInfo::CACHED, input.type());

  ASSERT_TRUE(input.has_last_modified_time_ms());
  EXPECT_EQ(headers.last_modified_time_ms(), input.last_modified_time_ms());

  ASSERT_TRUE(input.has_expiration_time_ms());
  EXPECT_EQ(headers.CacheExpirationTimeMs(), input.expiration_time_ms());

  ASSERT_TRUE(input.has_date_ms());
  EXPECT_EQ(headers.date_ms(), input.date_ms());
}

}  // namespace

TEST_F(ServerContextTest, FillInPartitionInputInfo) {
  // Test for Resource::FillInPartitionInputInfo.
  const char kUrl[] = "http://example.com/page.html";
  const char kContents[] = "bits";
  SetBaseUrlForFetch("http://example.com/");

  ResponseHeaders headers;
  SetDefaultLongCacheHeaders(&kContentTypeHtml, &headers);
  headers.ComputeCaching();
  SetFetchResponse(kUrl, headers, kContents);
  GoogleUrl gurl(kUrl);
  ResourcePtr resource(rewrite_driver()->CreateInputResource(gurl));
  VerifyContentsCallback callback(resource, kContents);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  callback.AssertCalled();

  InputInfo with_hash, without_hash;
  resource->FillInPartitionInputInfo(Resource::kIncludeInputHash, &with_hash);
  resource->FillInPartitionInputInfo(Resource::kOmitInputHash, &without_hash);

  CheckMatchesHeaders(headers, with_hash);
  CheckMatchesHeaders(headers, without_hash);
  ASSERT_TRUE(with_hash.has_input_content_hash());
  EXPECT_STREQ("zEEebBNnDlISRim4rIP30", with_hash.input_content_hash());
  EXPECT_FALSE(without_hash.has_input_content_hash());
}

// Test of referer for BackgroundFetch: When the resource fetching request
// header misses referer, we set the driver base url as its referer.
TEST_F(ServerContextTest, TestRefererBackgroundFetch) {
  RefererTest(NULL, true);
  EXPECT_EQ(rewrite_driver()->base_url().Spec(),
            mock_url_fetcher()->last_referer());
}

// Test of referer for NonBackgroundFetch: When the resource fetching request
// header misses referer and the original request referer header misses, no
// referer would be added.
TEST_F(ServerContextTest, TestRefererNonBackgroundFetch) {
  RequestHeaders headers;
  RefererTest(&headers, false);
  EXPECT_EQ("", mock_url_fetcher()->last_referer());
}

// Test of referer for NonBackgroundFetch: When the resource fetching request
// header misses referer but the original request header has referer set, we set
// this referer as the referer of resource fetching request.
TEST_F(ServerContextTest, TestRefererNonBackgroundFetchWithDriverRefer) {
  RequestHeaders headers;
  const char kReferer[] = "http://other.com/";
  headers.Add(HttpAttributes::kReferer, kReferer);
  RefererTest(&headers, false);
  EXPECT_EQ(kReferer, mock_url_fetcher()->last_referer());
}

// Regression test for RewriteTestBase::DefaultResponseHeaders, which is based
// on ServerContext methods. It used to not set 'Expires' correctly.
TEST_F(ServerContextTest, RewriteTestBaseDefaultResponseHeaders) {
  ResponseHeaders headers;
  DefaultResponseHeaders(kContentTypeCss, 100 /* ttl_sec */, &headers);
  int64 expire_time_ms = 0;
  ASSERT_TRUE(
      headers.ParseDateHeader(HttpAttributes::kExpires, &expire_time_ms));
  EXPECT_EQ(timer()->NowMs() + 100 * Timer::kSecondMs, expire_time_ms);
}

}  // namespace net_instaweb
