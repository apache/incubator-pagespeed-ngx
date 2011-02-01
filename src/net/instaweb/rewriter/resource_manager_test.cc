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

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/resource_manager_testing_peer.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"

namespace {

const char kResourceUrl[] = "http://example.com/image.png";
const char kResourceUrlBase[] = "http://example.com";
const char kResourceUrlPath[] = "/image.png";

const char kFilterKey[] = "X-ModPagespeed-FilterData";
const char kFilterVal[] = "X-ModPagespeed-FilterVal";

}  // namespace

namespace net_instaweb {

class VerifyContentsCallback : public Resource::AsyncCallback {
 public:
  explicit VerifyContentsCallback(const std::string& contents) :
      contents_(contents), called_(false) {
  }
  virtual void Done(bool success, Resource* resource) {
    EXPECT_EQ(0, contents_.compare(resource->contents().as_string()));
    called_ = true;
  }
  void AssertCalled() {
    EXPECT_TRUE(called_);
  }
  std::string contents_;
  bool called_;
};

class ResourceManagerTest : public ResourceManagerTestBase {
 protected:
  ResourceManagerTest() { }

  // Calls FetchOutputResource with different values of writer and
  // response_headers, to test all branches.  Expects the fetch to succeed all
  // times, and finally returns the contents.
  std::string FetchOutputResource(OutputResource* resource) {
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, NULL, NULL, &message_handler_, ResourceManager::kMayBlock));
    ResponseHeaders empty;
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, NULL, &empty, &message_handler_, ResourceManager::kMayBlock));
    std::string contents;
    StringWriter writer(&contents);
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, &writer, resource->metadata(),
        &message_handler_, ResourceManager::kMayBlock));
    return contents;
  }

  // Asserts that the given url starts with an appropriate prefix;
  // then cuts off that prefix.
  virtual void RemoveUrlPrefix(std::string* url) {
    EXPECT_EQ(0, url->compare(0, url_prefix_.length(), url_prefix_));
    url->erase(0, url_prefix_.length());
  }


  // Tests for the lifecycle and various flows of a named output resource.
  void TestNamed() {
    const char* filter_prefix = "fp";
    const char* name = "name";
    const char* contents = "contents";
    // origin_expire_time_ms should be considerably longer than the various
    // timeouts for resource locking, since we hit those timeouts in various
    // places.
    const int64 origin_expire_time_ms = 100000;
    const ContentType* content_type = &kContentTypeText;
    scoped_ptr<OutputResource> nor(
        resource_manager_->CreateOutputResourceWithPath(
            url_prefix_, filter_prefix, name, content_type,
            &options_, &message_handler_));
    ASSERT_TRUE(nor.get() != NULL);
    // Check name_key against url_prefix/fp.name
    std::string name_key = nor->name_key();
    RemoveUrlPrefix(&name_key);
    EXPECT_EQ(nor->full_name().EncodeIdName(), name_key);
    // Make sure the resource hasn't already been created (and lock it for
    // creation).
    EXPECT_FALSE(resource_manager_->FetchOutputResource(
        nor.get(), NULL, NULL, &message_handler_,
        ResourceManager::kNeverBlock));
    EXPECT_FALSE(nor->IsWritten());
    {
      // Now show that another attempt to create the resource will fail.
      // Here we attempt to create without the hash.
      scoped_ptr<OutputResource> nor1(
          resource_manager_->CreateOutputResourceWithPath(
              url_prefix_, filter_prefix, name, content_type,
              &options_, &message_handler_));
      ASSERT_TRUE(nor1.get() != NULL);
      // We'll succeed in fetching (meaning don't create the resource), but the
      // resource won't be written.
      EXPECT_TRUE(resource_manager_->FetchOutputResource(
          nor1.get(), NULL, NULL, &message_handler_,
          ResourceManager::kNeverBlock));
      EXPECT_FALSE(nor1->IsWritten());
    }
    {
      // Here we attempt to create the object with the hash and fail.
      ResourceNamer namer;
      namer.CopyFrom(nor->full_name());
      namer.set_hash("0");
      namer.set_ext("txt");
      std::string name = StrCat(url_prefix_, namer.Encode());
      scoped_ptr<OutputResource> nor1(
          resource_manager_->CreateOutputResourceForFetch(name));
      ASSERT_TRUE(nor1.get() != NULL);
      // Again we'll succeed in fetching (meaning don't create), but the
      // resource won't be written.  Note that we do a non-blocking fetch here.
      // An actual resource fetch does a blocking fetch that would end by
      // stealing the creation lock; we don't want to steal the lock here.
      EXPECT_TRUE(resource_manager_->FetchOutputResource(
          nor1.get(), NULL, NULL, &message_handler_,
          ResourceManager::kNeverBlock));
      EXPECT_FALSE(nor1->IsWritten());
    }
    // Write some data
    EXPECT_FALSE(ResourceManagerTestingPeer::HasHash(nor.get()));
    EXPECT_FALSE(ResourceManagerTestingPeer::Generated(nor.get()));
    EXPECT_TRUE(resource_manager_->Write(HttpStatus::kOK, contents, nor.get(),
                                         origin_expire_time_ms,
                                         &message_handler_));
    EXPECT_TRUE(nor->IsWritten());
    // Check that hash_ext() is correct.
    ResourceNamer full_name;
    EXPECT_TRUE(full_name.DecodeHashExt(nor->hash_ext()));
    EXPECT_EQ("0", full_name.hash());
    EXPECT_EQ("txt", full_name.ext());
    // Retrieve the same NOR from the cache.
    scoped_ptr<OutputResource> nor2(
        resource_manager_->CreateOutputResourceWithPath(
            url_prefix_, filter_prefix, name, &kContentTypeText,
            &options_, &message_handler_));
    ASSERT_TRUE(nor2.get() != NULL);
    EXPECT_TRUE(ResourceManagerTestingPeer::HasHash(nor2.get()));
    EXPECT_FALSE(ResourceManagerTestingPeer::Generated(nor2.get()));
    EXPECT_FALSE(nor2->IsWritten());

    // Fetch its contents and make sure they match
    EXPECT_EQ(contents, FetchOutputResource(nor2.get()));

    // Try asynchronously too
    VerifyContentsCallback callback(contents);
    resource_manager_->ReadAsync(nor2.get(), &callback, &message_handler_);
    callback.AssertCalled();

    // Grab the URL for later
    EXPECT_TRUE(nor2->HasValidUrl());
    std::string url = nor2->url();
    EXPECT_LT(0, url.length());

    // Now expire it from the HTTP cache.  Since we don't know its hash, we
    // cannot fetch it (even though the contents are still in the filesystem).
    mock_timer()->advance_ms(2 * origin_expire_time_ms);
    {
      scoped_ptr<OutputResource> nor3(
          resource_manager_->CreateOutputResourceWithPath(
              url_prefix_, filter_prefix, name, &kContentTypeText,
              &options_, &message_handler_));
      EXPECT_FALSE(resource_manager_->FetchOutputResource(
          nor3.get(), NULL, NULL, &message_handler_,
          ResourceManager::kNeverBlock));
      // Now nor3 has locked the resource for creation.
      // We must destruct nor3 in order to unlock it again, since we
      // have no intention of creating it.
    }

    RemoveUrlPrefix(&url);
    ASSERT_TRUE(full_name.Decode(url));
    EXPECT_EQ(content_type, full_name.ContentTypeFromExt());
    EXPECT_EQ(filter_prefix, full_name.id());
    EXPECT_EQ(name, full_name.name());

    // But with the URL (which contains the hash), we can retrieve it
    // from the http_cache.
    // first cut off the "http://mysite{,.0,.1}/" from the front.
    scoped_ptr<OutputResource> nor4(
        resource_manager_->CreateOutputResourceForFetch(nor->url()));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchOutputResource(nor4.get()));

    // If it's evicted from the http_cache, we can also retrieve it from the
    // filesystem.
    lru_cache_->Clear();
    nor4.reset(resource_manager_->CreateOutputResourceForFetch(nor->url()));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchOutputResource(nor4.get()));
    // This also works asynchronously.
    lru_cache_->Clear();
    VerifyContentsCallback callback2(contents);
    resource_manager_->ReadAsync(nor4.get(), &callback2, &message_handler_);
    callback2.AssertCalled();
  }

  bool ResourceIsCached() {
    scoped_ptr<Resource> resource(
        resource_manager_->CreateInputResource(
            GURL(kResourceUrlBase), kResourceUrlPath,
            &options_, &message_handler_));
    return resource_manager_->ReadIfCached(resource.get(), &message_handler_);
  }

  // Makes an output resource corresponding to given input resource of
  // given content type
  OutputResource* CreateTestOutputResource(Resource* input_resource,
                                           const ContentType* content_type) {
    return resource_manager_->CreateOutputResourceFromResource(
        "tf", content_type, resource_manager_->url_escaper(),
        input_resource, rewrite_driver_.options(), &message_handler_);
  }

  void VerifyCustomMetadata(OutputResource* output) {
    std::string val;
    EXPECT_TRUE(output->cached_result()->Remembered(kFilterKey, &val));
    EXPECT_EQ(std::string(kFilterVal), val);
    EXPECT_FALSE(output->cached_result()->Remembered("nosuchkey", &val));
  }

  void StoreCustomMetadata(OutputResource* output) {
    OutputResource::CachedResult* cached =
        output->EnsureCachedResultCreated();
    ASSERT_TRUE(cached != NULL);
    EXPECT_EQ(cached, output->cached_result());
    cached->SetRemembered(kFilterKey, kFilterVal);
  }

  // Expiration times are not entirely precise as some cache headers
  // have a 1 second resolution, so this permits such a difference.
  void VerifyWithinSecond(int64 time_a_ms, int64 time_b_ms) {
    int64 diff = time_a_ms - time_b_ms;
    if (diff < 0) {
      diff = -diff;
    }
    EXPECT_LE(diff, Timer::kSecondMs);
  }

  // Test to make sure we associate a CachedResult properly when doing
  // operations on output resources. This is parametrized on storing
  // custom metadata or not for better coverage (as the path with it on
  // creates a CachedResult outside ResourceManager)
  void TestCachedResult(bool test_meta_data) {
    // Note: we do not fetch the input here, just use it to name the output.
    scoped_ptr<Resource> input(
        resource_manager_->CreateInputResource(
            GURL(kResourceUrlBase), kResourceUrlPath, rewrite_driver_.options(),
            &message_handler_));
    ASSERT_TRUE(input.get() != NULL);

    scoped_ptr<OutputResource> output(
        CreateTestOutputResource(input.get(), &kContentTypePng));

    ASSERT_TRUE(output.get() != NULL);
    EXPECT_EQ(NULL, output->cached_result());
    EXPECT_TRUE(output->optimizable());
    EXPECT_FALSE(output->HasValidUrl());

    const int kTtlMs = 100000;
    mock_timer()->set_time_us(0);

    if (test_meta_data) {
      StoreCustomMetadata(output.get());
    }

    resource_manager_->Write(HttpStatus::kOK, "PNGnotreally",
                             output.get(), kTtlMs, &message_handler_);
    std::string producedUrl = output->url();

    // Make sure the cached_result object is in OK state after write.
    ASSERT_TRUE(output->cached_result() != NULL);
    EXPECT_EQ(producedUrl, output->cached_result()->url());
    VerifyWithinSecond(kTtlMs,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_TRUE(output->cached_result()->optimizable());
    EXPECT_TRUE(output->optimizable());
    if (test_meta_data) {
      VerifyCustomMetadata(output.get());
    }

    // Transfer ownership of it here and delete it --- should not blow up.
    delete output->ReleaseCachedResult();
    EXPECT_EQ(NULL, output->cached_result());

    // Now create the output resource again. We should recover the info,
    // including everything in cached_result and the URL and content-type
    // for the resource (notice this is passing NULL for content-type)
    output.reset(CreateTestOutputResource(input.get(), NULL));
    ASSERT_TRUE(output.get() != NULL);
    ASSERT_TRUE(output->cached_result() != NULL);

    ASSERT_TRUE(output->HasValidUrl());
    EXPECT_EQ(producedUrl, output->url());
    EXPECT_EQ(producedUrl, output->cached_result()->url());
    VerifyWithinSecond(kTtlMs,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_TRUE(output->cached_result()->optimizable());
    EXPECT_TRUE(output->optimizable());
    EXPECT_EQ(&kContentTypePng, output->type());
    if (test_meta_data) {
      VerifyCustomMetadata(output.get());
    }

    // Fast-forward the time, to make sure the entry expires.
    mock_timer()->advance_ms(kTtlMs + 1);
    output.reset(CreateTestOutputResource(input.get(), &kContentTypePng));
    EXPECT_FALSE(output->HasValidUrl());
    EXPECT_TRUE(output->optimizable());  // can't guarantee it's unoptimizable

    // Note: this is temporary. Eventually we want to keep CachedResults past
    // expiration and have explicit expiration bit on them.
    EXPECT_EQ(NULL, output->cached_result());

    // Write that it's unoptimizable this time.
    if (test_meta_data) {
      StoreCustomMetadata(output.get());
    }

    int next_expire = mock_timer()->NowMs() + kTtlMs;
    resource_manager_->WriteUnoptimizable(output.get(), next_expire,
                                          &message_handler_);

    ASSERT_TRUE(output->cached_result() != NULL);
    EXPECT_FALSE(output->HasValidUrl());
    VerifyWithinSecond(next_expire,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_FALSE(output->cached_result()->optimizable());
    EXPECT_FALSE(output->optimizable());
    if (test_meta_data) {
      VerifyCustomMetadata(output.get());
    }

    // Make a new resource, test for cached data getting fetched
    output.reset(CreateTestOutputResource(input.get(), NULL));
    ASSERT_TRUE(output.get() != NULL);
    ASSERT_TRUE(output->cached_result() != NULL);
    EXPECT_FALSE(output->HasValidUrl());
    VerifyWithinSecond(next_expire,
                       output->cached_result()->origin_expiration_time_ms());
    EXPECT_FALSE(output->cached_result()->optimizable());
    EXPECT_FALSE(output->optimizable());
    if (test_meta_data) {
      VerifyCustomMetadata(output.get());
    }

    // Now test expiration
    mock_timer()->advance_ms(kTtlMs);
    output.reset(CreateTestOutputResource(input.get(), &kContentTypePng));
    EXPECT_FALSE(output->HasValidUrl());
    EXPECT_TRUE(output->optimizable());
    EXPECT_EQ(NULL, output->cached_result());
  }
};

TEST_F(ResourceManagerTest, TestNamed) {
  TestNamed();
}

TEST_F(ResourceManagerTest, TestOutputInputUrl) {
  std::string url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResourceFromOutputResource(
          resource_manager_->url_escaper(),
          output_resource.get(),
          &options_,
          &message_handler_));
  EXPECT_EQ("http://example.com/dir/123/orig", input_resource->url());
}

TEST_F(ResourceManagerTest, TestOutputInputUrlEvil) {
  std::string escaped_abs;
  resource_manager_->url_escaper()->EncodeToUrlSegment("http://www.evil.com",
                                                       &escaped_abs);
  std::string url = Encode("http://example.com/dir/123/",
                            "jm", "0", escaped_abs, "js");
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResourceFromOutputResource(
          resource_manager_->url_escaper(),
          output_resource.get(),
          &options_,
          &message_handler_));
  EXPECT_EQ(NULL, input_resource.get());
}

TEST_F(ResourceManagerTest, TestOutputInputUrlBusy) {
  EXPECT_TRUE(options_.domain_lawyer()->AddOriginDomainMapping(
      "www.busy.com", "example.com", &message_handler_));

  std::string escaped_abs;
  resource_manager_->url_escaper()->EncodeToUrlSegment("http://www.busy.com",
                                                       &escaped_abs);
  std::string url = Encode("http://example.com/dir/123/",
                            "jm", "0", escaped_abs, "js");
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceForFetch(url));
  ASSERT_TRUE(output_resource.get());
  scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResourceFromOutputResource(
          resource_manager_->url_escaper(),
          output_resource.get(),
          &options_,
          &message_handler_));
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

  scoped_ptr<Resource> input(resource_manager_->CreateInputResource(
      GoogleUrl::Create(StrCat(kTestDomain, "index.html")),
      "style.css", &options_, &message_handler_));
  ASSERT_TRUE(input.get() != NULL);
  EXPECT_EQ(StrCat(kTestDomain, "style.css"), input->url());

  // The absolute input URL is in test.com, but we will only be
  // able to serve it from localhost, per the origin mapping above.
  static const char kStyleContent[] = "style content";
  const int kOriginTtlSec = 300;
  InitResponseHeaders("http://localhost/style.css", kContentTypeCss,
                      kStyleContent, kOriginTtlSec);
  EXPECT_TRUE(resource_manager_->ReadIfCached(input.get(), &message_handler_));

  // When we rewrite the resource as an ouptut, it will show up in the
  // CDN per the rewrite mapping.
  scoped_ptr<OutputResource> output(
      resource_manager_->CreateOutputResourceFromResource(
          RewriteDriver::kCacheExtenderId, input->type(),
          resource_manager_->url_escaper(), input.get(),
          &options_, &message_handler_));
  ASSERT_TRUE(output.get() != NULL);

  // We need to 'Write' an output resource before we can determine its
  // URL.
  resource_manager_->Write(HttpStatus::kOK, StringPiece(kStyleContent),
                           output.get(), kOriginTtlSec * Timer::kSecondMs,
                           &message_handler_);
  EXPECT_EQ(std::string("http://cdn.com/style.css.pagespeed.ce.0.css"),
            output->url());
}

// CreateOutputResourceForFetch should drop query
TEST_F(ResourceManagerTest, TestOutputResourceFetchQuery) {
  std::string url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceForFetch(StrCat(url, "?query")));
  ASSERT_TRUE(output_resource.get() != NULL);
  EXPECT_EQ(url, output_resource->url());
}

// Input resources and corresponding output resources should keep queries
TEST_F(ResourceManagerTest, TestInputResourceQuery) {
  const char kUrl[] = "test?param";
  scoped_ptr<Resource> resource(
      resource_manager_->CreateInputResource(
            GURL(kResourceUrlBase), kUrl,
            &options_, &message_handler_));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(StrCat(std::string(kResourceUrlBase), "/", kUrl), resource->url());
  scoped_ptr<OutputResource> output(
    resource_manager_->CreateOutputResourceFromResource(
      "sf", &kContentTypeCss, resource_manager_->url_escaper(), resource.get(),
      &options_, &message_handler_));
  ASSERT_TRUE(output.get() != NULL);

  std::string included_name;
  EXPECT_TRUE(
      resource_manager_->url_escaper()->DecodeFromUrlSegment(output->name(),
                                                             &included_name));
  EXPECT_EQ(std::string(kUrl), included_name);
}

TEST_F(ResourceManagerTest, TestRemember404) {
  // Make sure our resources remember that a page 404'd
  ResponseHeaders not_found;
  resource_manager_->SetDefaultHeaders(&kContentTypeHtml, &not_found);
  not_found.SetStatusAndReason(HttpStatus::kNotFound);
  mock_url_fetcher_.SetResponse("http://example.com/404", not_found, "");

  GURL base = GoogleUrl::Create(StringPiece("http://example.com/"));
  scoped_ptr<Resource> resource(
      resource_manager_->CreateInputResourceAndReadIfCached(
          base, "404", &options_, &message_handler_));
  EXPECT_EQ(NULL, resource.get());

  HTTPValue valueOut;
  ResponseHeaders headersOut;
  EXPECT_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch,
            http_cache_.Find("http://example.com/404", &valueOut, &headersOut,
                             &message_handler_));
}

TEST_F(ResourceManagerTest, TestNonCacheable) {
  const std::string kContents = "ok";

  // Make sure that when we get non-cacheable resources
  // we mark the fetch as failed in the cache.
  ResponseHeaders no_cache;
  resource_manager_->SetDefaultHeaders(&kContentTypeHtml, &no_cache);
  no_cache.RemoveAll(HttpAttributes::kCacheControl);
  no_cache.Add(HttpAttributes::kCacheControl, "no-cache");
  no_cache.ComputeCaching();
  mock_url_fetcher_.SetResponse("http://example.com/", no_cache, kContents);

  GURL base = GoogleUrl::Create(StringPiece("http://example.com"));
  scoped_ptr<Resource> resource(
      resource_manager_->CreateInputResource(base, "/", &options_,
                                             &message_handler_));
  ASSERT_TRUE(resource.get() != NULL);

  VerifyContentsCallback callback(kContents);
  resource_manager_->ReadAsync(resource.get(), &callback, &message_handler_);
  callback.AssertCalled();

  HTTPValue valueOut;
  ResponseHeaders headersOut;
  EXPECT_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch,
            http_cache_.Find("http://example.com/", &valueOut, &headersOut,
                             &message_handler_));
}

TEST_F(ResourceManagerTest, TestCachedResults) {
  TestCachedResult(false);
}

TEST_F(ResourceManagerTest, TestCachedResultsMetaData) {
  TestCachedResult(true);
}

class ResourceFreshenTest : public ResourceManagerTest {
 protected:
  static const char kContents[];

  virtual void SetUp() {
    ResourceManagerTest::SetUp();
    HTTPCache::Initialize(&stats_);
    http_cache_.SetStatistics(&stats_);
    expirations_ = stats_.GetVariable(HTTPCache::kCacheExpirations);
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

  SimpleStats stats_;
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
  const std::string kContents = "ok";
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
  const std::string kContents = "ok";

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
  std::string url = Encode("http://example.com/dir/123/",
                            "jm", "0", "orig", "js");
  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceWithPath(
          "http://example.com/dir/",
          "jm",
          "orig.js",
          &kContentTypeJavascript,
          &options_,
          &message_handler_));
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
