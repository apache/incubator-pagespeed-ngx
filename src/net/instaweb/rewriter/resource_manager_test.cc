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

// Unit-test the resource manager

#include "net/instaweb/rewriter/public/resource_manager.h"

#include <cmath>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/resource_manager_testing_peer.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"

namespace {

const char kResourceUrl[] = "http://example.com/image.png";
const char kResourceUrlBase[] = "http://example.com";
const char kResourceUrlPath[] = "/image.png";

}  // namespace

namespace net_instaweb {

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
  virtual void Done(bool success) {
    EXPECT_EQ(0, contents_.compare(resource()->contents().as_string()));
    called_ = true;
  }
  void AssertCalled() {
    EXPECT_TRUE(called_);
  }
  GoogleString contents_;
  bool called_;
};

class ResourceManagerTest : public ResourceManagerTestBase {
 protected:
  ResourceManagerTest() { }

  // Fetches data (which is expected to exist) for given resource,
  // but making sure to go through the path that checks for its
  // non-existence and potentially doing locking, too.
  GoogleString FetchExtantOutputResource(OutputResource* resource) {
    GoogleString contents;
    StringWriter writer(&contents);
    EXPECT_TRUE(rewrite_driver_.FetchExtantOutputResourceOrLock(
        resource, &writer, resource->metadata()));
    return contents;
  }

  // Returns whether there was an existing copy of data for the resource.
  // If not, makes sure the resource is wrapped.
  bool TryFetchExtantOutputResourceOrLock(OutputResource* resource) {
    NullWriter writer;
    return rewrite_driver_.FetchExtantOutputResourceOrLock(
        resource, &writer, resource->metadata());
  }

  // Asserts that the given url starts with an appropriate prefix;
  // then cuts off that prefix.
  virtual void RemoveUrlPrefix(GoogleString* url) {
    EXPECT_EQ(0, url->compare(0, url_prefix_.length(), url_prefix_));
    url->erase(0, url_prefix_.length());
  }

  OutputResourcePtr CreateOutputResourceForFetch(const StringPiece& url) {
    RewriteFilter* dummy;
    rewrite_driver_.SetBaseUrlForFetch(url);
    return rewrite_driver_.DecodeOutputResource(url, &dummy);
  }

  ResourcePtr CreateInputResourceAndReadIfCached(const StringPiece& url) {
    rewrite_driver_.SetBaseUrlForFetch(url);
    GoogleUrl resource_url(url);
    ResourcePtr resource(rewrite_driver_.CreateInputResource(resource_url));
    if ((resource.get() != NULL) &&
        (!resource->IsCacheable() ||
         !rewrite_driver_.ReadIfCached(resource))) {
      resource.clear();
    }
    return resource;
  }

  // Tests for the lifecycle and various flows of a named output resource.
  void TestNamed() {
    const char* filter_prefix = RewriteDriver::kCssFilterId;
    const char* name = "name";
    const char* contents = "contents";
    // origin_expire_time_ms should be considerably longer than the various
    // timeouts for resource locking, since we hit those timeouts in various
    // places.
    const int64 origin_expire_time_ms = 100000;
    const ContentType* content_type = &kContentTypeText;
    OutputResourcePtr nor(
        rewrite_driver_.CreateOutputResourceWithPath(
            url_prefix_, filter_prefix, name, content_type,
            OutputResource::kRewrittenResource));
    ASSERT_TRUE(nor.get() != NULL);
    // Check name_key against url_prefix/fp.name
    GoogleString name_key = nor->name_key();
    RemoveUrlPrefix(&name_key);
    EXPECT_EQ(nor->full_name().EncodeIdName(), name_key);
    // Make sure the resource hasn't already been created (and lock it for
    // creation). We do need to give it a hash for fetching to do anything.
    ResourceManagerTestingPeer::SetHash(nor.get(), "42");
    EXPECT_FALSE(TryFetchExtantOutputResourceOrLock(nor.get()));
    EXPECT_FALSE(nor->IsWritten());

    {
      // Check that a non-blocking attempt to lock another resource
      // with the same name returns quickly. We don't need a hash in this
      // case since we're just trying to create the resource, not fetch it.
      OutputResourcePtr nor1(
          rewrite_driver_.CreateOutputResourceWithPath(
              url_prefix_, filter_prefix, name, content_type,
              OutputResource::kRewrittenResource));
      ASSERT_TRUE(nor1.get() != NULL);
      EXPECT_FALSE(nor1->LockForCreation(resource_manager_,
                                         ResourceManager::kNeverBlock));
      EXPECT_FALSE(nor1->IsWritten());
    }

    {
      // Here we attempt to create the object with the hash and fetch it.
      // The fetch fails (but returns after stealing the lock, however).
      ResourceNamer namer;
      namer.CopyFrom(nor->full_name());
      namer.set_hash("0");
      namer.set_ext("txt");
      GoogleString name = StrCat(url_prefix_, namer.Encode());
      OutputResourcePtr nor1(CreateOutputResourceForFetch(name));
      ASSERT_TRUE(nor1.get() != NULL);

      // non-blocking
      EXPECT_FALSE(nor1->LockForCreation(resource_manager_,
                                         ResourceManager::kNeverBlock));
      // blocking but stealing
      EXPECT_FALSE(TryFetchExtantOutputResourceOrLock(nor1.get()));
    }

    // Write some data
    EXPECT_TRUE(ResourceManagerTestingPeer::HasHash(nor.get()));
    EXPECT_EQ(OutputResource::kRewrittenResource, nor->kind());
    EXPECT_TRUE(resource_manager_->Write(HttpStatus::kOK, contents, nor.get(),
                                         origin_expire_time_ms,
                                         &message_handler_));
    EXPECT_TRUE(nor->IsWritten());
    // Check that hash and ext are correct.
    EXPECT_EQ("0", nor->hash());
    EXPECT_EQ("txt", nor->extension());

    // Retrieve the same NOR from the cache.
    OutputResourcePtr nor2(rewrite_driver_.CreateOutputResourceWithPath(
        url_prefix_, filter_prefix, name, &kContentTypeText,
        OutputResource::kRewrittenResource));
    ASSERT_TRUE(nor2.get() != NULL);
    EXPECT_TRUE(ResourceManagerTestingPeer::HasHash(nor2.get()));
    EXPECT_EQ(OutputResource::kRewrittenResource, nor2->kind());
    EXPECT_FALSE(nor2->IsWritten());

    // Fetch its contents and make sure they match
    EXPECT_EQ(contents, FetchExtantOutputResource(nor2.get()));

    // Try asynchronously too
    VerifyContentsCallback callback(nor2, contents);
    rewrite_driver_.ReadAsync(&callback, &message_handler_);
    callback.AssertCalled();

    // Grab the URL and make sure we correctly decode its components
    GoogleString url = nor2->url();
    EXPECT_LT(0, url.length());
    RemoveUrlPrefix(&url);
    ResourceNamer full_name;
    ASSERT_TRUE(full_name.Decode(url));
    EXPECT_EQ(content_type, full_name.ContentTypeFromExt());
    EXPECT_EQ(filter_prefix, full_name.id());
    EXPECT_EQ(name, full_name.name());

    // Now expire it from the HTTP cache.  Since we don't know its hash, we
    // cannot fetch it (even though the contents are still in the filesystem).
    mock_timer()->advance_ms(2 * origin_expire_time_ms);

    // But with the URL (which contains the hash), we can retrieve it
    // from the http_cache.
    OutputResourcePtr nor4(CreateOutputResourceForFetch(nor->url()));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchExtantOutputResource(nor4.get()));

    // If it's evicted from the http_cache, we can also retrieve it from the
    // filesystem.
    lru_cache_->Clear();
    nor4.reset(CreateOutputResourceForFetch(nor->url()));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchExtantOutputResource(nor4.get()));
    // This also works asynchronously.
    lru_cache_->Clear();
    VerifyContentsCallback callback2(nor4, contents);
    rewrite_driver_.ReadAsync(&callback2, &message_handler_);
    callback2.AssertCalled();
  }

  bool ResourceIsCached() {
    ResourcePtr resource(CreateResource(kResourceUrlBase, kResourceUrlPath));
    bool ok = rewrite_driver_.ReadIfCached(resource);
    // Should not damage resources when freshening
    EXPECT_FALSE(ok && !resource->loaded());
    return ok;
  }

  // Makes an output resource corresponding to given input resource of
  // given content type
  OutputResourcePtr CreateTestOutputResource(Resource* input_resource,
                                             const ContentType* content_type) {
    rewrite_driver_.SetBaseUrlForFetch(input_resource->url());
    return rewrite_driver_.CreateOutputResourceFromResource(
        "tf", content_type, rewrite_driver_.default_encoder(), NULL,
        input_resource, OutputResource::kRewrittenResource);
  }

  void VerifyCustomMetadata(OutputResource* output) {
    EXPECT_TRUE(output->cached_result()->has_image_inlined_uri());
    EXPECT_EQ(kResourceUrl, output->cached_result()->image_inlined_uri());
  }

  void StoreCustomMetadata(OutputResource* output) {
    CachedResult* cached = output->EnsureCachedResultCreated();
    ASSERT_TRUE(cached != NULL);
    EXPECT_EQ(cached, output->cached_result());
    cached->set_image_inlined_uri(kResourceUrl);
  }

  // Expiration times are not entirely precise as some cache headers
  // have a 1 second resolution, so this permits such a difference.
  void VerifyWithinSecond(int64 time_a_ms, int64 time_b_ms) {
    // Note: need to pass in 1 * since otherwise we get a link failure
    // due to conversion of compile-time constant to const reference
    EXPECT_GE(1 * Timer::kSecondMs, std::abs(time_a_ms - time_b_ms));
  }

  void VerifyValidCachedResult(const char* subtest_name, bool test_meta_data,
                               OutputResource* output, const GoogleString& url,
                               int64 expire_ms) {
    LOG(INFO) << "Subtest:" << subtest_name;
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(output->cached_result() != NULL);

    EXPECT_EQ(url, output->url());
    EXPECT_EQ(url, output->cached_result()->url());
    VerifyWithinSecond(expire_ms,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_TRUE(output->cached_result()->optimizable());
    if (test_meta_data) {
      VerifyCustomMetadata(output);
    }
  }

  void VerifyUnoptimizableCachedResult(
      const char* subtest_name, bool test_meta_data, OutputResource* output,
      int64 expire_ms) {
    LOG(INFO) << "Subtest:" << subtest_name;
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(output->cached_result() != NULL);
    VerifyWithinSecond(expire_ms,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_FALSE(output->cached_result()->optimizable());
    if (test_meta_data) {
      VerifyCustomMetadata(output);
    }
  }

  // Test to make sure we associate a CachedResult properly when doing
  // operations on output resources. This is parametrized on storing
  // custom metadata or not for better coverage (as the path with it on
  // creates a CachedResult outside ResourceManager)
  void TestCachedResult(bool test_meta_data, bool auto_expire) {
    // Note: we do not fetch the input here, just use it to name the output.
    rewrite_driver_.SetBaseUrlForFetch(kResourceUrlBase);
    GoogleUrl base_url(kResourceUrlBase);
    GoogleUrl path_url(base_url, kResourceUrlPath);
    ResourcePtr input(rewrite_driver_.CreateInputResource(path_url));
    ASSERT_TRUE(input.get() != NULL);

    OutputResourcePtr output(
        CreateTestOutputResource(input.get(), &kContentTypePng));

    ASSERT_TRUE(output.get() != NULL);
    EXPECT_EQ(NULL, output->cached_result());

    const int kTtlMs = 100000;
    mock_timer()->set_time_us(0);

    output->EnsureCachedResultCreated()->set_auto_expire(auto_expire);
    if (test_meta_data) {
      StoreCustomMetadata(output.get());
    }

    resource_manager_->Write(HttpStatus::kOK, "PNGnotreally",
                             output.get(), kTtlMs, &message_handler_);
    GoogleString producedUrl = output->url();

    // Make sure the cached_result object is in OK state after write.
    VerifyValidCachedResult("initial", test_meta_data, output.get(),
                            producedUrl, kTtlMs);

    // Transfer ownership of it here and delete it --- should not blow up.
    delete output->ReleaseCachedResult();
    EXPECT_EQ(NULL, output->cached_result());

    // Now create the output resource again. We should recover the info,
    // including everything in cached_result and the URL and content-type
    // for the resource (notice this is passing NULL for content-type)
    output.reset(CreateTestOutputResource(input.get(), NULL));
    VerifyValidCachedResult("initial cached", test_meta_data, output.get(),
                            producedUrl, kTtlMs);

    // Fast-forward the time, to make sure the entry's TTL passes.
    mock_timer()->advance_ms(kTtlMs + 1);
    output.reset(CreateTestOutputResource(input.get(), &kContentTypePng));

    if (auto_expire) {
      EXPECT_EQ(NULL, output->cached_result());
    } else {
      VerifyValidCachedResult("non-autoexpire still cached", test_meta_data,
                              output.get(), producedUrl, kTtlMs);
    }

    // Write that it's unoptimizable this time.
    output->EnsureCachedResultCreated()->set_auto_expire(auto_expire);

    if (test_meta_data) {
      StoreCustomMetadata(output.get());
    }

    int next_expire = mock_timer()->NowMs() + kTtlMs;
    resource_manager_->WriteUnoptimizable(output.get(), next_expire,
                                          &message_handler_);
    VerifyUnoptimizableCachedResult(
        "initial unopt", test_meta_data, output.get(), next_expire);

    // Make a new resource, test for cached data getting fetched
    output.reset(CreateTestOutputResource(input.get(), NULL));
    VerifyUnoptimizableCachedResult(
        "unopt cached", test_meta_data, output.get(), next_expire);

    // Now test expiration
    mock_timer()->advance_ms(kTtlMs);
    output.reset(CreateTestOutputResource(input.get(), &kContentTypePng));
    if (auto_expire) {
      EXPECT_EQ(NULL, output->cached_result());
    } else {
      VerifyUnoptimizableCachedResult("non-autoexpire unopt cached",
                                      test_meta_data, output.get(),
                                      next_expire);
    }
  }
};

TEST_F(ResourceManagerTest, TestNamed) {
  TestNamed();
}

TEST_F(ResourceManagerTest, TestOutputInputUrl) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            RewriteDriver::kJavascriptMinId, "0", "orig", "js");
  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  RewriteFilter* filter = rewrite_driver_.FindFilter(
      RewriteDriver::kJavascriptMinId);
  ASSERT_TRUE(filter != NULL);
  ResourcePtr input_resource(
      filter->CreateInputResourceFromOutputResource(output_resource.get()));
  EXPECT_EQ("http://example.com/dir/123/orig", input_resource->url());
}

TEST_F(ResourceManagerTest, TestOutputInputUrlEvil) {
  GoogleString escaped_abs;
  UrlEscaper::EncodeToUrlSegment("http://www.evil.com", &escaped_abs);
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", escaped_abs, "js");
  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  RewriteFilter* filter = rewrite_driver_.FindFilter(
      RewriteDriver::kJavascriptMinId);
  ASSERT_TRUE(filter != NULL);
  ResourcePtr input_resource(
      filter->CreateInputResourceFromOutputResource(output_resource.get()));
  EXPECT_EQ(NULL, input_resource.get());
}

TEST_F(ResourceManagerTest, TestOutputInputUrlBusy) {
  EXPECT_TRUE(options_.domain_lawyer()->AddOriginDomainMapping(
      "www.busy.com", "example.com", &message_handler_));

  GoogleString escaped_abs;
  UrlEscaper::EncodeToUrlSegment("http://www.busy.com", &escaped_abs);
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", escaped_abs, "js");
  OutputResourcePtr output_resource(CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  RewriteFilter* filter = rewrite_driver_.FindFilter(
      RewriteDriver::kJavascriptMinId);
  ASSERT_TRUE(filter != NULL);
  ResourcePtr input_resource(
      filter->CreateInputResourceFromOutputResource(output_resource.get()));
  EXPECT_EQ(NULL, input_resource.get());
  if (input_resource.get() != NULL) {
    LOG(ERROR) << input_resource->url();
  }
}

// Check that we can origin-map a domain referenced from an HTML file
// to 'localhost', but rewrite-map it to 'cdn.com'.  This was not working
// earlier because ResourceManager::CreateInputResource was mapping to the
// rewrite domain, preventing us from finding the origin-mapping when
// fetching the URL.
TEST_F(ResourceManagerTest, TestMapRewriteAndOrigin) {
  ASSERT_TRUE(options_.domain_lawyer()->AddOriginDomainMapping(
      "localhost", kTestDomain, &message_handler_));
  EXPECT_TRUE(options_.domain_lawyer()->AddRewriteDomainMapping(
      "cdn.com", kTestDomain, &message_handler_));

  ResourcePtr input(CreateResource(StrCat(kTestDomain, "index.html"),
                                   "style.css"));
  ASSERT_TRUE(input.get() != NULL);
  EXPECT_EQ(StrCat(kTestDomain, "style.css"), input->url());

  // The absolute input URL is in test.com, but we will only be
  // able to serve it from localhost, per the origin mapping above.
  static const char kStyleContent[] = "style content";
  const int kOriginTtlSec = 300;
  InitResponseHeaders("http://localhost/style.css", kContentTypeCss,
                      kStyleContent, kOriginTtlSec);
  EXPECT_TRUE(rewrite_driver_.ReadIfCached(input));

  // When we rewrite the resource as an ouptut, it will show up in the
  // CDN per the rewrite mapping.
  OutputResourcePtr output(
      rewrite_driver_.CreateOutputResourceFromResource(
          RewriteDriver::kCacheExtenderId, input->type(),
          rewrite_driver_.default_encoder(), NULL,
          input.get(), OutputResource::kRewrittenResource));
  ASSERT_TRUE(output.get() != NULL);

  // We need to 'Write' an output resource before we can determine its
  // URL.
  resource_manager_->Write(HttpStatus::kOK, StringPiece(kStyleContent),
                           output.get(), kOriginTtlSec * Timer::kSecondMs,
                           &message_handler_);
  EXPECT_EQ(GoogleString("http://cdn.com/style.css.pagespeed.ce.0.css"),
            output->url());
}

// DecodeOutputResource should drop query
TEST_F(ResourceManagerTest, TestOutputResourceFetchQuery) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  RewriteFilter* dummy;
  OutputResourcePtr output_resource(
      rewrite_driver_.DecodeOutputResource(StrCat(url, "?query"), &dummy));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(url, output_resource->url());
}

// Input resources and corresponding output resources should keep queries
TEST_F(ResourceManagerTest, TestInputResourceQuery) {
  const char kUrl[] = "test?param";
  ResourcePtr resource(CreateResource(kResourceUrlBase, kUrl));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(StrCat(GoogleString(kResourceUrlBase), "/", kUrl), resource->url());
  OutputResourcePtr output(
    rewrite_driver_.CreateOutputResourceFromResource(
      "sf", &kContentTypeCss,
      rewrite_driver_.default_encoder(), NULL,
      resource.get(), OutputResource::kRewrittenResource));
  ASSERT_TRUE(output.get() != NULL);

  GoogleString included_name;
  EXPECT_TRUE(UrlEscaper::DecodeFromUrlSegment(output->name(), &included_name));
  EXPECT_EQ(GoogleString(kUrl), included_name);
}

TEST_F(ResourceManagerTest, TestRemember404) {
  // Make sure our resources remember that a page 404'd
  ResponseHeaders not_found;
  resource_manager_->SetDefaultHeaders(&kContentTypeHtml, &not_found);
  not_found.SetStatusAndReason(HttpStatus::kNotFound);
  mock_url_fetcher_.SetResponse("http://example.com/404", not_found, "");

  ResourcePtr resource(
      CreateInputResourceAndReadIfCached("http://example.com/404"));
  EXPECT_EQ(NULL, resource.get());

  HTTPValue valueOut;
  ResponseHeaders headersOut;
  EXPECT_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch,
            http_cache_.Find("http://example.com/404", &valueOut, &headersOut,
                             &message_handler_));
}

TEST_F(ResourceManagerTest, TestNonCacheable) {
  const GoogleString kContents = "ok";

  // Make sure that when we get non-cacheable resources
  // we mark the fetch as failed in the cache.
  ResponseHeaders no_cache;
  resource_manager_->SetDefaultHeaders(&kContentTypeHtml, &no_cache);
  no_cache.Replace(HttpAttributes::kCacheControl, "no-cache");
  no_cache.ComputeCaching();
  mock_url_fetcher_.SetResponse("http://example.com/", no_cache, kContents);

  ResourcePtr resource(CreateResource("http://example.com/", "/"));
  ASSERT_TRUE(resource.get() != NULL);

  VerifyContentsCallback callback(resource, kContents);
  rewrite_driver_.ReadAsync(&callback, &message_handler_);
  callback.AssertCalled();

  HTTPValue valueOut;
  ResponseHeaders headersOut;
  EXPECT_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch,
            http_cache_.Find("http://example.com/", &valueOut, &headersOut,
                             &message_handler_));
}

TEST_F(ResourceManagerTest, TestOutlined) {
  const int kLongExpireMs = 50000;

  // Outliner resources should not produce extra cache traffic
  // due to rname/ entries we can't use anyway.
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver_.CreateOutputResourceWithPath(
          url_prefix_, CssOutlineFilter::kFilterId, "_", &kContentTypeCss,
          OutputResource::kOutlinedResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  resource_manager_->Write(HttpStatus::kOK, "", output_resource.get(),
                           kLongExpireMs, &message_handler_);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(1, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  // Now try fetching again. It should not get a cached_result either.
  output_resource.reset(
      rewrite_driver_.CreateOutputResourceWithPath(
          url_prefix_, CssOutlineFilter::kFilterId, "_", &kContentTypeCss,
          OutputResource::kOutlinedResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(1, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
}

TEST_F(ResourceManagerTest, TestOnTheFly) {
  // Test to make sure that an on-fly insert does not insert the data,
  // just the rname/
  const int kLongExpireMs = 50000;

  // For derived resources we can and should use the rewrite
  // summary/metadata cache
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver_.CreateOutputResourceWithPath(
            url_prefix_, RewriteDriver::kCssFilterId, "_", &kContentTypeCss,
            OutputResource::kOnTheFlyResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses())
      << "should have a single miss trying to get a CachedResult";
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  resource_manager_->Write(HttpStatus::kOK, "", output_resource.get(),
                           kLongExpireMs, &message_handler_);
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
  EXPECT_EQ(1, lru_cache_->num_inserts())
      << "should insert a CachedResult (but not data)";
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  // Now try fetching again. Should hit in cache for rname.
  output_resource.reset(rewrite_driver_.CreateOutputResourceWithPath(
      url_prefix_, RewriteDriver::kCssFilterId, "_", &kContentTypeCss,
      OutputResource::kOnTheFlyResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
  EXPECT_EQ(1, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
}

TEST_F(ResourceManagerTest, TestNotGenerated) {
  const int kLongExpireMs = 50000;

  // For derived resources we can and should use the rewrite
  // summary/metadata cache
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
  OutputResourcePtr output_resource(
      rewrite_driver_.CreateOutputResourceWithPath(
            url_prefix_, RewriteDriver::kCssFilterId, "_", &kContentTypeCss,
            OutputResource::kRewrittenResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(NULL, output_resource->cached_result());
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses()) << "miss trying to get a CachedResult";
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  resource_manager_->Write(HttpStatus::kOK, "", output_resource.get(),
                           kLongExpireMs, &message_handler_);
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts()) << "insert CachedResult and output";
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());

  // Now try fetching again. Should hit in cache
  output_resource.reset(rewrite_driver_.CreateOutputResourceWithPath(
      url_prefix_, RewriteDriver::kCssFilterId, "_", &kContentTypeCss,
      OutputResource::kRewrittenResource));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_TRUE(output_resource->cached_result() != NULL);
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
}

TEST_F(ResourceManagerTest, TestCachedResults) {
  TestCachedResult(false, true);
}

TEST_F(ResourceManagerTest, TestCachedResultsMetaData) {
  TestCachedResult(true, true);
}

TEST_F(ResourceManagerTest, TestCachedResultsNoAutoExpire) {
  TestCachedResult(false, false);
}

TEST_F(ResourceManagerTest, TestCachedResultsMetaDataNoAutoExpire) {
  TestCachedResult(true, false);
}

class ResourceFreshenTest : public ResourceManagerTest {
 protected:
  static const char kContents[];

  virtual void SetUp() {
    ResourceManagerTest::SetUp();
    HTTPCache::Initialize(statistics_);
    expirations_ = statistics_->GetVariable(HTTPCache::kCacheExpirations);
    CHECK(expirations_ != NULL);
    resource_manager_->SetDefaultHeaders(&kContentTypePng, &response_headers_);
    response_headers_.SetStatusAndReason(HttpStatus::kOK);
    response_headers_.RemoveAll(HttpAttributes::kCacheControl);
    response_headers_.RemoveAll(HttpAttributes::kExpires);
  }

  // Moves the mock-timer forward by the specified number of seconds.
  // Updates kResourceUrl's headers as ssen by the mock fetcher, to
  // match the new mock timestamp.
  void AdvanceTimeAndUpdateOriginHeaders(int delta_sec) {
    mock_timer()->advance_ms(delta_sec * Timer::kSecondMs);
    response_headers_.SetDate(mock_timer()->NowMs());
    response_headers_.ComputeCaching();
    mock_url_fetcher_.SetResponse(kResourceUrl, response_headers_, "");
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
  WaitUrlAsyncFetcher simulate_async(&mock_url_fetcher_);
  rewrite_driver_.set_async_fetcher(&simulate_async);
  resource_manager_->set_url_async_fetcher(&simulate_async);

  // Make sure we don't try to insert non-cacheable resources
  // into the cache wastefully, but still fetch them well.
  int max_age_sec = ResponseHeaders::kImplicitCacheTtlMs / Timer::kSecondMs;
  response_headers_.Add(HttpAttributes::kCacheControl,
                       StringPrintf("max-age=%d", max_age_sec));
  AdvanceTimeAndUpdateOriginHeaders(0);

  // The test here is not that the ReadIfCached will succeed, because
  // it's a fake url fetcher.
  EXPECT_FALSE(ResourceIsCached());
  simulate_async.CallCallbacks();
  EXPECT_TRUE(ResourceIsCached());

  // Now let the time expire with no intervening fetches to freshen the cache.
  // This is because we do not proactively initiate refreshes for all resources;
  // only the ones that are actually asked for on a regular basis.  So a
  // completely inactive site will not see its resources freshened.
  AdvanceTimeAndUpdateOriginHeaders(max_age_sec + 1);
  expirations_->Clear();
  EXPECT_FALSE(ResourceIsCached());
  EXPECT_EQ(1, expirations_->Get());
  expirations_->Clear();
  simulate_async.CallCallbacks();
  EXPECT_TRUE(ResourceIsCached());

  // But if we have just a little bit of traffic then when we get a request
  // for a soon-to-expire resource it will auto-freshen.
  AdvanceTimeAndUpdateOriginHeaders(1 + (max_age_sec * 4) / 5);
  EXPECT_TRUE(ResourceIsCached());
  simulate_async.CallCallbacks();  // freshens cache.
  AdvanceTimeAndUpdateOriginHeaders(max_age_sec / 5);
  EXPECT_TRUE(ResourceIsCached());  // Yay, no cache misses after 301 seconds
  EXPECT_EQ(0, expirations_->Get());
}

// Tests that freshining will not be performed when we have caching
// forced.  Nothing will ever be evicted due to time, so there is no
// need to freshen.
TEST_F(ResourceFreshenTest, NoFreshenOfForcedCachedResources) {
  const GoogleString kContents = "ok";
  http_cache_.set_force_caching(true);

  CountingUrlAsyncFetcher counter(&mock_url_async_fetcher_);
  rewrite_driver_.set_async_fetcher(&counter);
  resource_manager_->set_url_async_fetcher(&counter);

  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0");
  AdvanceTimeAndUpdateOriginHeaders(0);

  // We should get just 1 fetch.  If we were aggressively freshening
  // we would get 2.
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counter.fetch_count());

  // There should be no extra fetches required because our cache is
  // still active.  We shouldn't have needed an extra fetch to freshen,
  // either, because the cache expiration time is irrelevant -- we are
  // forcing caching so we consider the resource to always be fresh.
  // So even after an hour we should have no expirations.
  AdvanceTimeAndUpdateOriginHeaders(3600);  // 1 hour
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counter.fetch_count());

  // Nothing expires with force-caching on.
  EXPECT_EQ(0, expirations_->Get());
}

// Tests that freshining will not occur for short-lived resources,
// which could impact the performance of the server.
TEST_F(ResourceFreshenTest, NoFreshenOfShortLivedResources) {
  const GoogleString kContents = "ok";

  CountingUrlAsyncFetcher counter(&mock_url_async_fetcher_);
  rewrite_driver_.set_async_fetcher(&counter);
  resource_manager_->set_url_async_fetcher(&counter);

  int max_age_sec = ResponseHeaders::kImplicitCacheTtlMs / Timer::kSecondMs - 1;
  response_headers_.Add(HttpAttributes::kCacheControl,
                       StringPrintf("max-age=%d", max_age_sec));
  AdvanceTimeAndUpdateOriginHeaders(0);

  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counter.fetch_count());

  // There should be no extra fetches required because our cache is
  // still active.  We shouldn't have needed an extra fetch to freshen,
  // either.
  AdvanceTimeAndUpdateOriginHeaders(max_age_sec - 1);
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(1, counter.fetch_count());
  EXPECT_EQ(0, expirations_->Get());

  // Now let the resource expire.  We'll need another fetch since we did not
  // freshen.
  AdvanceTimeAndUpdateOriginHeaders(2);
  EXPECT_TRUE(ResourceIsCached());
  EXPECT_EQ(2, counter.fetch_count());
  EXPECT_EQ(1, expirations_->Get());
}

class ResourceManagerShardedTest : public ResourceManagerTest {
 protected:
  virtual void SetUp() {
    ResourceManagerTest::SetUp();
    EXPECT_TRUE(options_.domain_lawyer()->AddShard(
        "example.com", "shard0.com,shard1.com", &message_handler_));
  }
};

TEST_F(ResourceManagerShardedTest, TestNamed) {
  GoogleString url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  OutputResourcePtr output_resource(
      rewrite_driver_.CreateOutputResourceWithPath(
          "http://example.com/dir/",
          "jm",
          "orig.js",
          &kContentTypeJavascript,
          OutputResource::kRewrittenResource));
  ASSERT_TRUE(output_resource.get());
  ASSERT_TRUE(resource_manager_->Write(HttpStatus::kOK, "alert('hello');",
                                       output_resource.get(), 0,
                                       &message_handler_));

  // This always gets mapped to shard0 because we are using the mock
  // hasher for the content hash.  Note that the sharding sensitivity
  // to the hash value is tested in DomainLawyerTest.Shard, and will
  // also be covered in a system test.
  EXPECT_EQ("http://shard0.com/dir/orig.js.pagespeed.jm.0.js",
            output_resource->url());
}

}  // namespace net_instaweb
