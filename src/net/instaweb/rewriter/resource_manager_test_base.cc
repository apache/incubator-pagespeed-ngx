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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/simple_stats.h"

namespace net_instaweb {

const char ResourceManagerTestBase::kTestData[] =
    "/net/instaweb/rewriter/testdata/";

ResourceManagerTestBase::ResourceManagerTestBase()
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

void ResourceManagerTestBase::SetUp() {
  HtmlParseTestBaseNoAlloc::SetUp();
  // TODO(sligocki): Init this in constructor.
  resource_manager_ = new ResourceManager(
      file_prefix_, &file_system_,
      &filename_encoder_, &mock_url_async_fetcher_, &mock_hasher_,
      &http_cache_, &lock_manager_);

  resource_manager_->set_statistics(&statistics_);
  RewriteDriver::Initialize(&statistics_);
  rewrite_driver_.SetResourceManager(resource_manager_);
}

void ResourceManagerTestBase::TearDown() {
  delete resource_manager_;
  HtmlParseTestBaseNoAlloc::TearDown();
}

// Add a single rewrite filter to rewrite_driver_.
void ResourceManagerTestBase::AddFilter(RewriteOptions::Filter filter) {
  options_.EnableFilter(filter);
  rewrite_driver_.AddFilters();
}

// Add a single rewrite filter to other_rewrite_driver_.
void ResourceManagerTestBase::AddOtherFilter(RewriteOptions::Filter filter) {
  other_options_.EnableFilter(filter);
  other_rewrite_driver_.AddFilters();
}

void ResourceManagerTestBase::AddRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_.RegisterRewriteFilter(filter);
  rewrite_driver_.EnableRewriteFilter(filter->id().c_str());
}

void ResourceManagerTestBase::AddOtherRewriteFilter(RewriteFilter* filter) {
  other_rewrite_driver_.RegisterRewriteFilter(filter);
  other_rewrite_driver_.EnableRewriteFilter(filter->id().c_str());
}

void ResourceManagerTestBase::DeleteFileIfExists(const std::string& filename) {
  if (file_system_.Exists(filename.c_str(), &message_handler_).is_true()) {
    ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
  }
}

void ResourceManagerTestBase::AppendDefaultHeaders(
    const ContentType& content_type,
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

void ResourceManagerTestBase::ServeResourceFromManyContexts(
    const std::string& resource_url,
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
void ResourceManagerTestBase::ServeResourceFromNewContext(
    const std::string& resource_url,
    RewriteOptions::Filter filter,
    Hasher* hasher,
    const StringPiece& expected_content) {

  // New objects for the new server.
  MemFileSystem other_file_system;
  // other_lru_cache is owned by other_http_cache_.
  LRUCache* other_lru_cache(new LRUCache(kCacheSize));
  MockTimer* other_mock_timer = other_file_system.timer();
  HTTPCache other_http_cache(other_lru_cache, other_mock_timer);
  DomainLawyer other_domain_lawyer;
  FileSystemLockManager other_lock_manager(
      &other_file_system, other_mock_timer, &message_handler_);
  WaitUrlAsyncFetcher wait_url_async_fetcher(&mock_url_fetcher_);
  ResourceManager other_resource_manager(
      file_prefix_, &other_file_system,
      &filename_encoder_, &wait_url_async_fetcher, hasher,
      &other_http_cache, &other_lock_manager);

  SimpleStats stats;
  RewriteDriver::Initialize(&stats);
  other_resource_manager.set_statistics(&stats);

  RewriteDriver other_rewrite_driver(&message_handler_, &other_file_system,
                                     &wait_url_async_fetcher, other_options_);
  other_rewrite_driver.SetResourceManager(&other_resource_manager);
  other_options_.EnableFilter(filter);
  other_rewrite_driver.AddFilters();

  Variable* cached_resource_fetches =
      stats.GetVariable(RewriteDriver::kResourceFetchesCached);
  Variable* succeeded_filter_resource_fetches =
      stats.GetVariable(RewriteDriver::kResourceFetchConstructSuccesses);
  Variable* failed_filter_resource_fetches =
      stats.GetVariable(RewriteDriver::kResourceFetchConstructFailures);

  RequestHeaders request_headers;
  // TODO(sligocki): We should set default request headers.
  ResponseHeaders response_headers;
  std::string response_contents;
  StringWriter response_writer(&response_contents);
  DummyCallback callback(true);

  // Check that we don't already have it in cache.
  EXPECT_EQ(CacheInterface::kNotFound, other_http_cache.Query(resource_url));

  // Initiate fetch.
  EXPECT_EQ(true, other_rewrite_driver.FetchResource(
      resource_url, request_headers, &response_headers, &response_writer,
      &callback));

  // Content should not be set until we call the callback.
  EXPECT_EQ(false, callback.done_);
  EXPECT_EQ("", response_contents);

  // After we call the callback, it should be correct.
  wait_url_async_fetcher.CallCallbacks();
  EXPECT_EQ(true, callback.done_);
  EXPECT_EQ(expected_content, response_contents);
  EXPECT_EQ(CacheInterface::kAvailable, other_http_cache.Query(resource_url));

  // Check that stats say we took the construct resource path.
  EXPECT_EQ(0, cached_resource_fetches->Get());
  EXPECT_EQ(1, succeeded_filter_resource_fetches->Get());
  EXPECT_EQ(0, failed_filter_resource_fetches->Get());
}

// Initializes a resource for mock fetching.
void ResourceManagerTestBase::InitResponseHeaders(
    const StringPiece& resource_name,
    const ContentType& content_type,
    const StringPiece& content,
    int64 ttl) {
  std::string name;
  if (resource_name.starts_with("http://")) {
    resource_name.CopyToString(&name);
  } else {
    name = StrCat(kTestDomain, resource_name);
  }
  ResponseHeaders response_headers;
  resource_manager_->SetDefaultHeaders(&content_type, &response_headers);
  response_headers.RemoveAll(HttpAttributes::kCacheControl);
  response_headers.Add(HttpAttributes::kCacheControl,
                       StrCat("public, max-age=", Integer64ToString(ttl)));
  response_headers.ComputeCaching();
  mock_url_fetcher_.SetResponse(name, response_headers, content);
}

// TODO(sligocki): Take a ttl and share code with InitResponseHeaders.
void ResourceManagerTestBase::AddFileToMockFetcher(
    const StringPiece& url,
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

// Helper function to test resource fetching, returning true if the fetch
// succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
// on the status and EXPECT_EQ on the content.
bool ResourceManagerTestBase::ServeResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    std::string* content) {
  std::string url = Encode(path, filter_id, "0", name, ext);
  return ServeResourceUrl(url, content);
}

bool ResourceManagerTestBase::ServeResourceUrl(
    const StringPiece& url, std::string* content) {
  content->clear();
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  StringWriter writer(content);
  FetchCallback callback;
  bool fetched = rewrite_driver_.FetchResource(
      url, request_headers, &response_headers, &writer, &callback);
  // The callback should be called if and only if FetchResource
  // returns true.
  EXPECT_EQ(fetched, callback.done());
  return fetched && callback.success();
}

// Just check if we can fetch a resource successfully, ignore response.
bool ResourceManagerTestBase::TryFetchResource(const StringPiece& url) {
  std::string contents;
  return ServeResourceUrl(url, &contents);
}

std::string ResourceManagerTestBase::Encode(
    const StringPiece& path, const StringPiece& id, const StringPiece& hash,
    const StringPiece& name, const StringPiece& ext) {
  ResourceNamer namer;
  namer.set_id(id);
  namer.set_hash(hash);
  namer.set_name(name);
  namer.set_ext(ext);
  return StrCat(path, namer.Encode());
}

// Overrides the async fetcher on the primary context to be a
// wait fetcher which permits delaying callback invocation, and returns a
// pointer to the new fetcher.
WaitUrlAsyncFetcher* ResourceManagerTestBase::SetupWaitFetcher() {
  WaitUrlAsyncFetcher* delayer =
      new WaitUrlAsyncFetcher(&mock_url_fetcher_);
  rewrite_driver_.set_async_fetcher(delayer);
  resource_manager_->set_url_async_fetcher(delayer);
  return delayer;
}

}  // namespace net_instaweb
