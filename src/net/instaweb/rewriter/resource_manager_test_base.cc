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

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/mem_clean_up.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_time_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class MessageHandler;

const char ResourceManagerTestBase::kTestData[] =
    "/net/instaweb/rewriter/testdata/";
const char ResourceManagerTestBase::kXhtmlDtd[] =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
    "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";

ResourceManagerTestBase::ResourceManagerTestBase()
    : factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                            &mock_url_fetcher_)),
      other_factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                                  &mock_url_fetcher_)),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()) {
  Init();
}

ResourceManagerTestBase::ResourceManagerTestBase(
    TestRewriteDriverFactory* factory,
    TestRewriteDriverFactory* other_factory)
    : factory_(factory),
      other_factory_(other_factory),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()) {
  Init();
}

void ResourceManagerTestBase::Init() {
  RewriteDriverFactory::Initialize(&statistics_);
  factory_->SetStatistics(&statistics_);
  other_factory_->SetStatistics(&statistics_);
  resource_manager_ = factory_->CreateResourceManager();
  other_resource_manager_ = other_factory_->CreateResourceManager();
  other_rewrite_driver_ = MakeDriver(other_resource_manager_, other_options_);
}

ResourceManagerTestBase::~ResourceManagerTestBase() {
}

// The Setup/Constructor split is designed so that test subclasses can
// add options prior to calling ResourceManagerTestBase::SetUp().
void ResourceManagerTestBase::SetUp() {
  HtmlParseTestBaseNoAlloc::SetUp();
  rewrite_driver_ = MakeDriver(resource_manager_, options_);
}

void ResourceManagerTestBase::TearDown() {
  rewrite_driver_->WaitForCompletion();
  factory_->ShutDown();
  rewrite_driver_->Clear();
  delete rewrite_driver_;
  other_rewrite_driver_->WaitForCompletion();
  other_factory_->ShutDown();
  other_rewrite_driver_->Clear();
  delete other_rewrite_driver_;
  HtmlParseTestBaseNoAlloc::TearDown();
}

// Add a single rewrite filter to rewrite_driver_.
void ResourceManagerTestBase::AddFilter(RewriteOptions::Filter filter) {
  options()->EnableFilter(filter);
  rewrite_driver_->AddFilters();
}

// Add a single rewrite filter to other_rewrite_driver_.
void ResourceManagerTestBase::AddOtherFilter(RewriteOptions::Filter filter) {
  other_options()->EnableFilter(filter);
  other_rewrite_driver_->AddFilters();
}

void ResourceManagerTestBase::AddRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_->RegisterRewriteFilter(filter);
  rewrite_driver_->EnableRewriteFilter(filter->id().c_str());
}

void ResourceManagerTestBase::AddOtherRewriteFilter(RewriteFilter* filter) {
  other_rewrite_driver_->RegisterRewriteFilter(filter);
  other_rewrite_driver_->EnableRewriteFilter(filter->id().c_str());
}

void ResourceManagerTestBase::SetBaseUrlForFetch(const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(url);
}

void ResourceManagerTestBase::SetAsynchronousRewrites(bool async) {
  rewrite_driver_->SetAsynchronousRewrites(async);
  other_rewrite_driver_->SetAsynchronousRewrites(async);
}

void ResourceManagerTestBase::DeleteFileIfExists(const GoogleString& filename) {
  if (file_system()->Exists(filename.c_str(), message_handler()).is_true()) {
    ASSERT_TRUE(file_system()->RemoveFile(filename.c_str(), message_handler()));
  }
}

ResourcePtr ResourceManagerTestBase::CreateResource(const StringPiece& base,
                                                    const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(base);
  GoogleUrl base_url(base);
  GoogleUrl resource_url(base_url, url);
  return rewrite_driver_->CreateInputResource(resource_url);
}

void ResourceManagerTestBase::AppendDefaultHeaders(
    const ContentType& content_type,
    GoogleString* text) {
  ResponseHeaders header;
  int64 time = mock_timer()->NowUs();
  // Reset mock timer so synthetic headers match original.
  mock_timer()->SetTimeUs(start_time_ms() * Timer::kMsUs);
  resource_manager_->SetDefaultLongCacheHeaders(&content_type, &header);
  // Then set it back.  Note that no alarms should fire at this point
  // because alarms work on absolute time.
  mock_timer()->SetTimeUs(time);
  StringWriter writer(text);
  header.WriteAsHttp(&writer, message_handler());
}

void ResourceManagerTestBase::ServeResourceFromManyContexts(
    const GoogleString& resource_url,
    const StringPiece& expected_content,
    UrlNamer* new_rms_url_namer) {
  // TODO(sligocki): Serve the resource under several contexts. For example:
  //   1) With output-resource cached,
  //   2) With output-resource not cached, but in a file,
  //   3) With output-resource unavailable, but input-resource cached,
  //   4) With output-resource unavailable and input-resource not cached,
  //      but still fetchable,
  ServeResourceFromNewContext(resource_url, expected_content,
                              new_rms_url_namer);
  //   5) With nothing available (failure).
}

// Test that a resource can be served from a new server that has not yet
// been constructed.
void ResourceManagerTestBase::ServeResourceFromNewContext(
    const GoogleString& resource_url,
    const StringPiece& expected_content,
    UrlNamer* new_rms_url_namer) {

  // New objects for the new server.
  SimpleStats stats;
  TestRewriteDriverFactory new_factory(GTestTempDir(), &mock_url_fetcher_);
  new_factory.SetStatistics(&statistics_);
  ResourceManager* new_resource_manager = new_factory.CreateResourceManager();
  if (new_rms_url_namer != NULL) {
    new_resource_manager->set_url_namer(new_rms_url_namer);
  }
  new_resource_manager->set_hasher(resource_manager_->hasher());
  RewriteOptions* new_options = options_->Clone();
  resource_manager_->ComputeSignature(new_options);
  RewriteDriver* new_rewrite_driver = MakeDriver(new_resource_manager,
                                                 new_options);
  new_factory.SetupWaitFetcher();
  new_rewrite_driver->SetAsynchronousRewrites(
      rewrite_driver_->asynchronous_rewrites());

  RequestHeaders request_headers;
  // TODO(sligocki): We should set default request headers.
  ResponseHeaders response_headers;
  GoogleString response_contents;
  StringWriter response_writer(&response_contents);
  ExpectCallback callback(true);

  // Check that we don't already have it in cache.
  EXPECT_EQ(CacheInterface::kNotFound,
            new_factory.http_cache()->Query(resource_url));

  // Initiate fetch.
  EXPECT_EQ(true, new_rewrite_driver->FetchResource(
      resource_url, request_headers, &response_headers, &response_writer,
      &callback));

  // Content should not be set until we call the callback.
  EXPECT_FALSE(callback.done());
  EXPECT_EQ("", response_contents);

  // After we call the callback, it should be correct.
  new_factory.CallFetcherCallbacksForDriver(new_rewrite_driver);
  EXPECT_EQ(true, callback.done());
  EXPECT_STREQ(expected_content, response_contents);

  // Check that stats say we took the construct resource path.
  RewriteStats* new_stats = new_factory.rewrite_stats();
  EXPECT_EQ(0, new_stats->cached_resource_fetches()->Get());
  // We should construct at least one resource, and maybe more if the
  // output resource was produced by multiple filters (e.g. JS minimize
  // then combine).
  EXPECT_LE(1, new_stats->succeeded_filter_resource_fetches()->Get());
  EXPECT_EQ(0, new_stats->failed_filter_resource_fetches()->Get());

  // Make sure to shut the new worker down before we hit ~RewriteDriver for
  // new_rewrite_driver.
  new_factory.ShutDown();
  delete new_rewrite_driver;
}

GoogleString ResourceManagerTestBase::AbsolutifyUrl(
    const StringPiece& resource_name) {
  GoogleString name;
  if (resource_name.starts_with("http://") ||
      resource_name.starts_with("https://")) {
    resource_name.CopyToString(&name);
  } else {
    name = StrCat(kTestDomain, resource_name);
  }
  return name;
}

void ResourceManagerTestBase::DefaultResponseHeaders(
    const ContentType& content_type, int64 ttl_sec,
    ResponseHeaders* response_headers) {
  SetDefaultLongCacheHeaders(&content_type, response_headers);
  response_headers->Replace(HttpAttributes::kCacheControl,
                           StrCat("public, max-age=",
                                  Integer64ToString(ttl_sec)));
  response_headers->ComputeCaching();
}

// Initializes a resource for mock fetching.
void ResourceManagerTestBase::InitResponseHeaders(
    const StringPiece& resource_name,
    const ContentType& content_type,
    const StringPiece& content,
    int64 ttl_sec) {
  GoogleString url = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  DefaultResponseHeaders(content_type, ttl_sec, &response_headers);
  SetFetchResponse(url, response_headers, content);
}

void ResourceManagerTestBase::SetFetchResponse404(
    const StringPiece& resource_name) {
  GoogleString name = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.SetStatusAndReason(HttpStatus::kNotFound);
  SetFetchResponse(name, response_headers, StringPiece());
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
      filename_str.c_str(), &contents, message_handler()));
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
  ResponseHeaders response;
  return ServeResourceUrl(url, content, &response);
}

bool ResourceManagerTestBase::ServeResourceUrl(
    const StringPiece& url, GoogleString* content) {
  ResponseHeaders response;
  return ServeResourceUrl(url, content, &response);
}

bool ResourceManagerTestBase::ServeResourceUrl(
    const StringPiece& url, GoogleString* content, ResponseHeaders* response) {
  content->clear();
  RequestHeaders request_headers;
  StringWriter writer(content);
  MockCallback callback;
  bool fetched = rewrite_driver_->FetchResource(
      url, request_headers, response, &writer, &callback);

  // We call WaitForCompletion when testing the serving of rewritten
  // resources, because that's how the server will work.  It will
  // complete the Rewrite independent of how long it takes.
  rewrite_driver_->WaitForCompletion();
  rewrite_driver_->Clear();

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
  file_system()->Disable();
  ResponseHeaders headers;
  resource_manager_->SetDefaultLongCacheHeaders(content_type, &headers);
  http_cache()->Put(expected_rewritten_path, &headers, rewritten_content,
                    message_handler());
  EXPECT_EQ(0U, lru_cache()->num_hits());
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(1U, lru_cache()->num_hits());
  EXPECT_EQ(rewritten_content, content);

  // Now remove it from the cache, but put it in the file system.  Make sure
  // that works.  Still there is no mock fetcher.
  file_system()->Enable();
  lru_cache()->Clear();

  WriteOutputResourceFile(expected_rewritten_path, content_type,
                          rewritten_content);
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // After serving from the disk, we should have seeded our cache.  Check it.
  RewriteFilter* filter = rewrite_driver_->FindFilter(filter_id);
  if (!filter->ComputeOnTheFly()) {
    EXPECT_EQ(CacheInterface::kAvailable, http_cache()->Query(
        expected_rewritten_path));
  }

  // Finally, nuke the file, nuke the cache, get it via a fetch.
  file_system()->Disable();
  RemoveOutputResourceFile(expected_rewritten_path);
  lru_cache()->Clear();
  InitResponseHeaders(orig_name, *content_type, orig_content,
                      100 /* ttl in seconds */);
  EXPECT_TRUE(ServeResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // Now we expect both the file and the cache entry to be there.
  if (!filter->ComputeOnTheFly()) {
    EXPECT_EQ(CacheInterface::kAvailable, http_cache()->Query(
        expected_rewritten_path));
  }
  file_system()->Enable();
  EXPECT_TRUE(file_system()->Exists(OutputResourceFilename(
    expected_rewritten_path).c_str(), message_handler()).is_true());
}

GoogleString ResourceManagerTestBase::OutputResourceFilename(
    const StringPiece& url) {
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
  EXPECT_TRUE(file_system()->WriteFile(OutputResourceFilename(url).c_str(),
                                       data, message_handler()));
}

void ResourceManagerTestBase::RemoveOutputResourceFile(const StringPiece& url) {
  EXPECT_TRUE(file_system()->RemoveFile(
      OutputResourceFilename(url).c_str(), message_handler()));
}

// Just check if we can fetch a resource successfully, ignore response.
bool ResourceManagerTestBase::TryFetchResource(const StringPiece& url) {
  GoogleString contents;
  ResponseHeaders response;
  return ServeResourceUrl(url, &contents, &response);
}


ResourceManagerTestBase::CssLink::CssLink(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock)
    : url_(url.data(), url.size()),
      content_(content.data(), content.size()),
      media_(media.data(), media.size()),
      supply_mock_(supply_mock) {
}

ResourceManagerTestBase::CssLink::Vector::~Vector() {
  STLDeleteElements(this);
}

void ResourceManagerTestBase::CssLink::Vector::Add(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock) {
  push_back(new CssLink(url, content, media, supply_mock));
}

bool ResourceManagerTestBase::CssLink::DecomposeCombinedUrl(
    GoogleString* base, StringVector* segments, MessageHandler* handler) {
  GoogleUrl gurl(url_);
  bool ret = false;
  if (gurl.is_valid()) {
    gurl.AllExceptLeaf().CopyToString(base);
    ResourceNamer namer;
    if (namer.Decode(gurl.LeafWithQuery()) &&
        (namer.id() == RewriteDriver::kCssCombinerId)) {
      UrlMultipartEncoder multipart_encoder;
      GoogleString segment;
      ret = multipart_encoder.Decode(namer.name(), segments, NULL, handler);
    }
  }
  return ret;
}

namespace {

// Helper class to collect CSS hrefs.
class CssCollector : public EmptyHtmlFilter {
 public:
  CssCollector(HtmlParse* html_parse,
               ResourceManagerTestBase::CssLink::Vector* css_links)
      : css_links_(css_links),
        css_tag_scanner_(html_parse) {
  }

  virtual void EndElement(HtmlElement* element) {
    HtmlElement::Attribute* href;
    const char* media;
    if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
      // TODO(jmarantz): collect content of the CSS files, before and
      // after combination, so we can diff.
      const char* content = "";
      css_links_->Add(href->value(), content, media, false);
    }
  }

  virtual const char* Name() const { return "CssCollector"; }

 private:
  ResourceManagerTestBase::CssLink::Vector* css_links_;
  CssTagScanner css_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(CssCollector);
};

}  // namespace

// Collects just the hrefs from CSS links into a string vector.
void ResourceManagerTestBase::CollectCssLinks(
    const StringPiece& id, const StringPiece& html, StringVector* css_links) {
  CssLink::Vector v;
  CollectCssLinks(id, html, &v);
  for (int i = 0, n = v.size(); i < n; ++i) {
    css_links->push_back(v[i]->url_);
  }
}

// Collects all information about CSS links into a CssLink::Vector.
void ResourceManagerTestBase::CollectCssLinks(
    const StringPiece& id, const StringPiece& html,
    CssLink::Vector* css_links) {
  HtmlParse html_parse(message_handler());
  CssCollector collector(&html_parse, css_links);
  html_parse.AddFilter(&collector);
  GoogleString dummy_url = StrCat("http://collect.css.links/", id, ".html");
  html_parse.StartParse(dummy_url);
  html_parse.ParseText(html.data(), html.size());
  html_parse.FinishParse();
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
  StringPieceVector path_vector;
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
  factory_->SetupWaitFetcher();
}

void ResourceManagerTestBase::CallFetcherCallbacks() {
  factory_->CallFetcherCallbacksForDriver(rewrite_driver_);
}

RewriteDriver* ResourceManagerTestBase::MakeDriver(
    ResourceManager* resource_manager, RewriteOptions* options) {
  // We use unmanaged drivers rather than NewCustomDriver here so
  // that _test.cc files can add options after the driver was created
  // and before the filters are added.
  //
  // TODO(jmarantz): change call-sites to make this use a more
  // standard flow.
  RewriteDriver* rd = resource_manager->NewUnmanagedRewriteDriver();
  rd->set_custom_options(options);
  // As we are using mock time, we need to set a consistent deadline here,
  // as otherwise when running under Valgrind some tests will finish
  // with different HTML headers than expected.
  rd->set_rewrite_deadline_ms(20);
  rd->set_externally_managed(true);
  return rd;
}

void ResourceManagerTestBase::TestRetainExtraHeaders(
    const StringPiece& name,
    const StringPiece& encoded_name,
    const StringPiece& filter_id,
    const StringPiece& ext) {
  GoogleString url = AbsolutifyUrl(name);

  // Add some extra headers.
  AddToResponse(url, HttpAttributes::kEtag, "Custom-Etag");
  AddToResponse(url, "extra", "attribute");
  AddToResponse(url, HttpAttributes::kSetCookie, "Custom-Cookie");

  GoogleString content;
  ResponseHeaders response;
  GoogleString rewritten_url = Encode(kTestDomain, filter_id, "0",
                                      encoded_name, ext);
  ASSERT_TRUE(ServeResourceUrl(rewritten_url, &content, &response));

  // Extra non-blacklisted header is preserved.
  ConstStringStarVector v;
  ASSERT_TRUE(response.Lookup("extra", &v));
  ASSERT_EQ(1U, v.size());
  EXPECT_STREQ("attribute", *v[0]);

  // Note: These tests can fail if ResourceManager::FetchResource failed to
  // rewrite the resource and instead served the original.
  // TODO(sligocki): Add a check that we successfully rewrote the resource.

  // Blacklisted headers are stripped (or changed).
  EXPECT_FALSE(response.Lookup(HttpAttributes::kSetCookie, &v));

  ASSERT_TRUE(response.Lookup(HttpAttributes::kEtag, &v));
  ASSERT_EQ(1U, v.size());
  EXPECT_STREQ("W/0", *v[0]);
}

void ResourceManagerTestBase::ClearStats() {
  statistics()->Clear();
  lru_cache()->ClearStats();
  counting_url_async_fetcher()->Clear();
  file_system()->ClearStats();
}

void ResourceManagerTestBase::SetCacheDelayUs(int64 delay_us) {
  factory_->mock_time_cache()->set_delay_us(delay_us);
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
