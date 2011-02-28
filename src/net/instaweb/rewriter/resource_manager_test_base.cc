// Copyright 2010 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/simple_stats.h"

namespace net_instaweb {

const char ResourceManagerTestBase::kTestData[] =
    "/net/instaweb/rewriter/testdata/";

void ResourceManagerTestBase::AddRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_.RegisterRewriteFilter(filter);
  rewrite_driver_.EnableRewriteFilter(filter->id().c_str());
}

void ResourceManagerTestBase::AddOtherRewriteFilter(RewriteFilter* filter) {
  other_rewrite_driver_.RegisterRewriteFilter(filter);
  other_rewrite_driver_.EnableRewriteFilter(filter->id().c_str());
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

}  // namespace net_instaweb
