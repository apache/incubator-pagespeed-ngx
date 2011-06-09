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

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/mem_clean_up.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_thread_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/worker.h"

namespace net_instaweb {

struct ContentType;

const char ResourceManagerTestBase::kTestData[] =
    "/net/instaweb/rewriter/testdata/";
const char ResourceManagerTestBase::kXhtmlDtd[] =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
    "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";
SimpleStats* ResourceManagerTestBase::statistics_;

namespace {

class IdleCallback : public Worker::Closure {
 public:
  IdleCallback(RewriteDriver* driver) : rewrite_driver_(driver) {}
  virtual void Run() { rewrite_driver_->WakeupFromIdle(); }

 private:
  RewriteDriver* rewrite_driver_;
};

}  // namespace

ResourceManagerTestBase::ResourceManagerTestBase()
    : mock_url_async_fetcher_(&mock_url_fetcher_),
      counting_url_async_fetcher_(&mock_url_async_fetcher_),
      wait_for_fetches_(false),
      base_thread_system_(ThreadSystem::CreateThreadSystem()),
      thread_system_(new MockThreadSystem(base_thread_system_.get(),
                                          mock_timer())),
      file_prefix_(StrCat(GTestTempDir(), "/")),
      url_prefix_(URL_PREFIX),

      lru_cache_(new LRUCache(kCacheSize)),
      http_cache_(lru_cache_, file_system_.timer(), statistics_),
      // TODO(jmaessen): Pull timer out of file_system_ and make it
      // standalone.
      lock_manager_(&file_system_, file_prefix_, file_system_.timer(),
                    &message_handler_),
      factory_(NULL),  // Not using the Factory in tests for now.
      options_(new RewriteOptions),
      rewrite_driver_(&message_handler_, &file_system_,
                      &counting_url_async_fetcher_),

      other_lru_cache_(new LRUCache(kCacheSize)),
      other_http_cache_(other_lru_cache_, other_file_system_.timer(),
                        statistics_),
      other_lock_manager_(
          &other_file_system_, file_prefix_,
          other_file_system_.timer(), &message_handler_),

      other_resource_manager_(
          file_prefix_, &other_file_system_, &filename_encoder_,
          &counting_url_async_fetcher_, &mock_hasher_,
          &other_http_cache_, other_lru_cache_, &other_lock_manager_,
          &message_handler_, statistics_, thread_system_.get(), NULL),
      other_options_(new RewriteOptions),
      other_rewrite_driver_(&message_handler_, &other_file_system_,
                            &counting_url_async_fetcher_),
      wait_url_async_fetcher_(&mock_url_fetcher_, thread_system_->NewMutex()) {
  rewrite_driver_.set_custom_options(options_);
  other_rewrite_driver_.set_custom_options(other_options_);
  // rewrite_driver_.SetResourceManager(resource_manager_);
  other_rewrite_driver_.SetResourceManager(&other_resource_manager_);

  // TODO(jmarantz): Lots of tests send multiple HTML files through the
  // same RewriteDriver.  Once this is changed then we can allow the
  // RewriteDrivers to be self-managed.
  rewrite_driver_.set_externally_managed(true);
  other_rewrite_driver_.set_externally_managed(true);
}

ResourceManagerTestBase::~ResourceManagerTestBase() {
}

void ResourceManagerTestBase::SetUpTestCase() {
  statistics_ = new SimpleStats();
  ResourceManager::Initialize(statistics_);
}

void ResourceManagerTestBase::TearDownTestCase() {
  delete statistics_;
  statistics_ = NULL;
}

void ResourceManagerTestBase::SetUp() {
  statistics_->Clear();
  HtmlParseTestBaseNoAlloc::SetUp();
  // TODO(sligocki): Init this in constructor.
  resource_manager_ = new ResourceManager(
      file_prefix_, &file_system_, &filename_encoder_,
      &counting_url_async_fetcher_, &mock_hasher_,
      &http_cache_, lru_cache_, &lock_manager_,
      &message_handler_, statistics_, thread_system_.get(), factory_);
  rewrite_driver_.SetResourceManager(resource_manager_);
}

void ResourceManagerTestBase::TearDown() {
  delete resource_manager_;
  HtmlParseTestBaseNoAlloc::TearDown();
}

// Add a single rewrite filter to rewrite_driver_.
void ResourceManagerTestBase::AddFilter(RewriteOptions::Filter filter) {
  options_->EnableFilter(filter);
  rewrite_driver_.AddFilters();
}

// Add a single rewrite filter to other_rewrite_driver_.
void ResourceManagerTestBase::AddOtherFilter(RewriteOptions::Filter filter) {
  other_options_->EnableFilter(filter);
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

void ResourceManagerTestBase::SetBaseUrlForFetch(const StringPiece& url) {
  rewrite_driver_.SetBaseUrlForFetch(url);
}

void ResourceManagerTestBase::DeleteFileIfExists(const GoogleString& filename) {
  if (file_system_.Exists(filename.c_str(), &message_handler_).is_true()) {
    ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
  }
}

ResourcePtr ResourceManagerTestBase::CreateResource(const StringPiece& base,
                                                    const StringPiece& url) {
  rewrite_driver_.SetBaseUrlForFetch(base);
  GoogleUrl base_url(base);
  GoogleUrl resource_url(base_url, url);
  return rewrite_driver_.CreateInputResource(resource_url);
}

void ResourceManagerTestBase::AppendDefaultHeaders(
    const ContentType& content_type,
    GoogleString* text) {
  ResponseHeaders header;
  int64 time = mock_timer()->NowUs();
  // Reset mock timer so synthetic headers match original.
  mock_timer()->SetTimeUs(0);
  resource_manager_->SetDefaultLongCacheHeaders(&content_type, &header);
  // Then set it back.  Note that no alarms should fire at this point
  // because alarms work on absolute time.
  mock_timer()->SetTimeUs(time);
  StringWriter writer(text);
  header.WriteAsHttp(&writer, &message_handler_);
}

void ResourceManagerTestBase::ServeResourceFromManyContexts(
    const GoogleString& resource_url,
    const StringPiece& expected_content) {
  // TODO(sligocki): Serve the resource under several contexts. For example:
  //   1) With output-resource cached,
  //   2) With output-resource not cached, but in a file,
  //   3) With output-resource unavailable, but input-resource cached,
  //   4) With output-resource unavailable and input-resource not cached,
  //      but still fetchable,
  ServeResourceFromNewContext(resource_url, expected_content);
  //   5) With nothing available (failure).
}

// Test that a resource can be served from a new server that has not yet
// been constructed.
void ResourceManagerTestBase::ServeResourceFromNewContext(
    const GoogleString& resource_url,
    const StringPiece& expected_content) {

  // New objects for the new server.
  SimpleStats stats;
  ResourceManager::Initialize(&stats);
  MemFileSystem other_file_system;
  // other_lru_cache is owned by other_http_cache_.
  LRUCache* other_lru_cache(new LRUCache(kCacheSize));
  MockTimer* other_mock_timer = other_file_system.timer();
  HTTPCache other_http_cache(other_lru_cache, other_mock_timer, &stats);
  DomainLawyer other_domain_lawyer;
  FileSystemLockManager other_lock_manager(
      &other_file_system, file_prefix_, other_mock_timer, &message_handler_);
  WaitUrlAsyncFetcher wait_url_async_fetcher(&mock_url_fetcher_,
                                             thread_system_->NewMutex());
  ResourceManager new_resource_manager(
      file_prefix_, &other_file_system, &filename_encoder_,
      &wait_url_async_fetcher, hasher(),
      &other_http_cache, other_lru_cache, &other_lock_manager,
      &message_handler_, &stats, thread_system_.get(), factory_);

  RewriteDriver new_rewrite_driver(&message_handler_, &other_file_system,
                                   &wait_url_async_fetcher);
  RewriteOptions* options = new RewriteOptions;
  options->CopyFrom(*options_);
  new_rewrite_driver.set_custom_options(options);
  new_rewrite_driver.SetResourceManager(&new_resource_manager);
  new_rewrite_driver.SetAsynchronousRewrites(
      rewrite_driver_.asynchronous_rewrites());
  new_rewrite_driver.AddFilters();

  new_resource_manager.SetIdleCallback(new IdleCallback(&new_rewrite_driver));

  RequestHeaders request_headers;
  // TODO(sligocki): We should set default request headers.
  ResponseHeaders response_headers;
  GoogleString response_contents;
  StringWriter response_writer(&response_contents);
  ExpectCallback callback(true);

  // Check that we don't already have it in cache.
  EXPECT_EQ(CacheInterface::kNotFound, other_http_cache.Query(resource_url));

  // Initiate fetch.
  EXPECT_EQ(true, new_rewrite_driver.FetchResource(
      resource_url, request_headers, &response_headers, &response_writer,
      &callback));

  // Content should not be set until we call the callback.
  EXPECT_FALSE(callback.done());
  EXPECT_EQ("", response_contents);

  // After we call the callback, it should be correct.
  CallFetcherCallbacksForDriver(&wait_url_async_fetcher, &new_rewrite_driver);
  EXPECT_EQ(true, callback.done());
  EXPECT_EQ(expected_content, response_contents);

  // Check that stats say we took the construct resource path.
  EXPECT_EQ(0, new_resource_manager.cached_resource_fetches()->Get());
  EXPECT_EQ(1, new_resource_manager.succeeded_filter_resource_fetches()->Get());
  EXPECT_EQ(0, new_resource_manager.failed_filter_resource_fetches()->Get());
}

// Initializes a resource for mock fetching.
void ResourceManagerTestBase::InitResponseHeaders(
    const StringPiece& resource_name,
    const ContentType& content_type,
    const StringPiece& content,
    int64 ttl_sec) {
  GoogleString name;
  if (resource_name.starts_with("http://")) {
    resource_name.CopyToString(&name);
  } else {
    name = StrCat(kTestDomain, resource_name);
  }
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&content_type, &response_headers);
  response_headers.Replace(HttpAttributes::kCacheControl,
                           StrCat("public, max-age=",
                                  Integer64ToString(ttl_sec)));
  response_headers.ComputeCaching();
  SetFetchResponse(name, response_headers, content);
}

void ResourceManagerTestBase::AddFileToMockFetcher(
    const StringPiece& url,
    const StringPiece& filename,
    const ContentType& content_type,
    int64 ttl_sec) {
  // TODO(sligocki): There's probably a lot of wasteful copying here.

  // We need to load a file from the testdata directory. Don't use this
  // physical filesystem for anything else, use file_system_ which can be
  // abstracted as a MemFileSystem instead.
  GoogleString contents;
  StdioFileSystem stdio_file_system;
  GoogleString filename_str = StrCat(GTestSrcDir(), kTestData, filename);
  ASSERT_TRUE(stdio_file_system.ReadFile(
      filename_str.c_str(), &contents, &message_handler_));
  InitResponseHeaders(url, content_type, contents, ttl_sec);
}

// Helper function to test resource fetching, returning true if the fetch
// succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
// on the status and EXPECT_EQ on the content.
bool ResourceManagerTestBase::ServeResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    GoogleString* content) {
  GoogleString url = Encode(path, filter_id, "0", name, ext);
  return ServeResourceUrl(url, content);
}

bool ResourceManagerTestBase::ServeResourceUrl(
    const StringPiece& url, GoogleString* content) {
  content->clear();
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  StringWriter writer(content);
  MockCallback callback;
  bool fetched = rewrite_driver_.FetchResource(
      url, request_headers, &response_headers, &writer, &callback);
  rewrite_driver_.WaitForCompletion();
  rewrite_driver_.Clear();

  // The callback should be called if and only if FetchResource returns true.
  EXPECT_EQ(fetched, callback.done());
  return fetched && callback.success();
}

void ResourceManagerTestBase::TestServeFiles(
    const ContentType* content_type,
    const StringPiece& filter_id,
    const StringPiece& rewritten_ext,
    const StringPiece& orig_name,
    const StringPiece& orig_content,
    const StringPiece& rewritten_name,
    const StringPiece& rewritten_content) {
  ResourceNamer namer;
  namer.set_id(filter_id);
  namer.set_name(rewritten_name);
  namer.set_ext(rewritten_ext);
  namer.set_hash("0");
  GoogleString expected_rewritten_path = StrCat(kTestDomain, namer.Encode());
  GoogleString content;

  // When we start, there are no mock fetchers, so we'll need to get it
  // from the cache or the disk.  Start with the cache.
  file_system_.Disable();
  ResponseHeaders headers;
  resource_manager_->SetDefaultLongCacheHeaders(content_type, &headers);
  http_cache_.Put(expected_rewritten_path, &headers, rewritten_content,
                  &message_handler_);
  EXPECT_EQ(0U, lru_cache_->num_hits());
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(1U, lru_cache_->num_hits());
  EXPECT_EQ(rewritten_content, content);

  // Now remove it from the cache, but put it in the file system.  Make sure
  // that works.  Still there is no mock fetcher.
  file_system_.Enable();
  lru_cache_->Clear();

  WriteOutputResourceFile(expected_rewritten_path, content_type,
                          rewritten_content);
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // After serving from the disk, we should have seeded our cache.  Check it.
  RewriteFilter* filter = rewrite_driver_.FindFilter(filter_id);
  if (!filter->ComputeOnTheFly()) {
    EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(
        expected_rewritten_path));
  }

  // Finally, nuke the file, nuke the cache, get it via a fetch.
  file_system_.Disable();
  RemoveOutputResourceFile(expected_rewritten_path);
  lru_cache_->Clear();
  InitResponseHeaders(orig_name, *content_type, orig_content,
                      100 /* ttl in seconds */);
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // Now we expect both the file and the cache entry to be there.
  if (!filter->ComputeOnTheFly()) {
    EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(
        expected_rewritten_path));
  }
  file_system_.Enable();
  EXPECT_TRUE(file_system_.Exists(OutputResourceFilename(
    expected_rewritten_path).c_str(), &message_handler_).is_true());
}

GoogleString ResourceManagerTestBase::OutputResourceFilename(const StringPiece& url) {
  GoogleString filename;
  FilenameEncoder* encoder = resource_manager_->filename_encoder();
  encoder->Encode(resource_manager_->filename_prefix(), url, &filename);
  return filename;
}

void ResourceManagerTestBase::WriteOutputResourceFile(
    const StringPiece& url, const ContentType* content_type,
    const StringPiece& rewritten_content) {
  ResponseHeaders headers;
  resource_manager_->SetDefaultLongCacheHeaders(content_type, &headers);
  GoogleString data = StrCat(headers.ToString(), rewritten_content);
  EXPECT_TRUE(file_system_.WriteFile(OutputResourceFilename(url).c_str(), data,
                                     &message_handler_));
}

void ResourceManagerTestBase::RemoveOutputResourceFile(const StringPiece& url) {
  EXPECT_TRUE(file_system_.RemoveFile(
      OutputResourceFilename(url).c_str(), &message_handler_));
}

// Just check if we can fetch a resource successfully, ignore response.
bool ResourceManagerTestBase::TryFetchResource(const StringPiece& url) {
  GoogleString contents;
  return ServeResourceUrl(url, &contents);
}

GoogleString ResourceManagerTestBase::Encode(
    const StringPiece& path, const StringPiece& id, const StringPiece& hash,
    const StringPiece& name, const StringPiece& ext) {
  ResourceNamer namer;
  namer.set_id(id);
  namer.set_hash(hash);

  // We only want to encode the last path-segment of 'name'.
  // Note that this block of code could be avoided if all call-sites
  // put subdirectory info in the 'path' argument, but it turns out
  // to be a lot more convenient for tests if we allow relative paths
  // in the 'name' argument for this method, so the one-time effort of
  // teasing out the leaf and encoding that saves a whole lot of clutter
  // in, at least, CacheExtenderTest.
  std::vector<StringPiece> path_vector;
  SplitStringPieceToVector(name, "/", &path_vector, false);
  UrlSegmentEncoder encoder;
  GoogleString encoded_name;
  StringVector v;
  CHECK_LT(0U, path_vector.size());
  v.push_back(path_vector[path_vector.size() - 1].as_string());
  encoder.Encode(v, NULL, &encoded_name);

  // Now reconstruct the path.
  GoogleString pathname;
  for (int i = 0, n = path_vector.size() - 1; i < n; ++i) {
    path_vector[i].AppendToString(&pathname);
    pathname += "/";
  }
  pathname += encoded_name;

  namer.set_name(pathname);
  namer.set_ext(ext);
  return StrCat(path, namer.Encode());
}

void ResourceManagerTestBase::SetupWaitFetcher() {
  resource_manager_->SetIdleCallback(new IdleCallback(rewrite_driver()));
  other_resource_manager_.SetIdleCallback(new IdleCallback(
      other_rewrite_driver()));
  counting_url_async_fetcher_.set_fetcher(&wait_url_async_fetcher_);
  wait_for_fetches_ = true;
}

void ResourceManagerTestBase::ParseUrl(const StringPiece& url,
                                       const GoogleString& html_input) {
  HtmlParseTestBaseNoAlloc::ParseUrl(url, html_input);
  if (!wait_for_fetches_) {
    rewrite_driver_.WaitForCompletion();
    rewrite_driver_.Clear();
  }
}

void ResourceManagerTestBase::CallFetcherCallbacksForDriver(
      WaitUrlAsyncFetcher* fetcher,
      RewriteDriver* driver) {
  bool pass_through_mode = fetcher->SetPassThroughMode(true);
  driver->WaitForCompletion();
  fetcher->SetPassThroughMode(pass_through_mode);
  driver->Clear();
}

void ResourceManagerTestBase::CallFetcherCallbacks() {
  CallFetcherCallbacksForDriver(&wait_url_async_fetcher_, &rewrite_driver_);
}

// Logging at the INFO level slows down tests, adds to the noise, and
// adds considerably to the speed variability.
class ResourceManagerProcessContext {
 public:
  ResourceManagerProcessContext() {
    logging::SetMinLogLevel(logging::LOG_WARNING);
  }

 private:
  MemCleanUp mem_clean_up_;
};
ResourceManagerProcessContext resource_manager_process_context;

}  // namespace net_instaweb
