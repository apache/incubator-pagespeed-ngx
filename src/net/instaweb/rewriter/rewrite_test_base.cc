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

#include "net/instaweb/rewriter/public/rewrite_test_base.h"

#include <vector>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_time_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
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

const char RewriteTestBase::kTestData[] =
    "/net/instaweb/rewriter/testdata/";

RewriteTestBase::RewriteTestBase()
    : statistics_(new SimpleStats()),
      factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                            &mock_url_fetcher_)),
      other_factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                                  &mock_url_fetcher_)),
      use_managed_rewrite_drivers_(false),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()) {
  Init();
}

// Takes ownership of the statistics.
RewriteTestBase::RewriteTestBase(Statistics* statistics)
    : statistics_(statistics),
      factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                            &mock_url_fetcher_)),
      other_factory_(new TestRewriteDriverFactory(GTestTempDir(),
                                                  &mock_url_fetcher_)),
      use_managed_rewrite_drivers_(false),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()) {
  Init();
}

RewriteTestBase::RewriteTestBase(
    TestRewriteDriverFactory* factory,
    TestRewriteDriverFactory* other_factory)
    : statistics_(new SimpleStats()),
      factory_(factory),
      other_factory_(other_factory),
      use_managed_rewrite_drivers_(false),
      options_(factory_->NewRewriteOptions()),
      other_options_(other_factory_->NewRewriteOptions()) {
  Init();
}

void RewriteTestBase::Init() {
  DCHECK(statistics_ != NULL);
  RewriteDriverFactory::Initialize(statistics_.get());
  factory_->SetStatistics(statistics_.get());
  other_factory_->SetStatistics(statistics_.get());
  resource_manager_ = factory_->CreateResourceManager();
  other_resource_manager_ = other_factory_->CreateResourceManager();
  other_rewrite_driver_ = MakeDriver(other_resource_manager_, other_options_);
}

RewriteTestBase::~RewriteTestBase() {
}

// The Setup/Constructor split is designed so that test subclasses can
// add options prior to calling ResourceManagerTestBase::SetUp().
void RewriteTestBase::SetUp() {
  HtmlParseTestBaseNoAlloc::SetUp();
  rewrite_driver_ = MakeDriver(resource_manager_, options_);
}

void RewriteTestBase::TearDown() {
  if (use_managed_rewrite_drivers_) {
    factory_->ShutDown();
  } else {
    rewrite_driver_->WaitForShutDown();

    // We need to make sure we shutdown the threads here before
    // deleting the driver, as the last task on the rewriter's job
    // queue may still be wrapping up some cleanups and notifications.
    factory_->ShutDown();
    rewrite_driver_->Clear();
    delete rewrite_driver_;
  }
  other_rewrite_driver_->WaitForShutDown();
  other_factory_->ShutDown();
  other_rewrite_driver_->Clear();
  delete other_rewrite_driver_;
  HtmlParseTestBaseNoAlloc::TearDown();
}

// Adds rewrite filters related to recompress images.
void RewriteTestBase::AddRecompressImageFilters() {
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRecompressWebp);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
}

// Add a single rewrite filter to rewrite_driver_.
void RewriteTestBase::AddFilter(RewriteOptions::Filter filter) {
  options()->EnableFilter(filter);
  rewrite_driver_->AddFilters();
}

// Add a single rewrite filter to other_rewrite_driver_.
void RewriteTestBase::AddOtherFilter(RewriteOptions::Filter filter) {
  other_options()->EnableFilter(filter);
  other_rewrite_driver_->AddFilters();
}

void RewriteTestBase::AddRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_->RegisterRewriteFilter(filter);
  rewrite_driver_->EnableRewriteFilter(filter->id());
}

void RewriteTestBase::AddFetchOnlyRewriteFilter(RewriteFilter* filter) {
  rewrite_driver_->RegisterRewriteFilter(filter);
}

void RewriteTestBase::AddOtherRewriteFilter(RewriteFilter* filter) {
  other_rewrite_driver_->RegisterRewriteFilter(filter);
  other_rewrite_driver_->EnableRewriteFilter(filter->id());
}

void RewriteTestBase::SetBaseUrlForFetch(const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(url);
}

ResourcePtr RewriteTestBase::CreateResource(const StringPiece& base,
                                                    const StringPiece& url) {
  rewrite_driver_->SetBaseUrlForFetch(base);
  GoogleUrl base_url(base);
  GoogleUrl resource_url(base_url, url);
  return rewrite_driver_->CreateInputResource(resource_url);
}

void RewriteTestBase::PopulateDefaultHeaders(
    const ContentType& content_type, int64 original_content_length,
    ResponseHeaders* headers) {
  int64 time = mock_timer()->NowUs();
  // Reset mock timer so synthetic headers match original.
  mock_timer()->SetTimeUs(start_time_ms() * Timer::kMsUs);
  resource_manager_->SetDefaultLongCacheHeaders(&content_type, headers);
  // Then set it back.  Note that no alarms should fire at this point
  // because alarms work on absolute time.
  mock_timer()->SetTimeUs(time);
  if (original_content_length > 0) {
    headers->SetOriginalContentLength(original_content_length);
  }
}

void RewriteTestBase::AppendDefaultHeaders(
    const ContentType& content_type, GoogleString* text) {
  ResponseHeaders headers;
  PopulateDefaultHeaders(content_type, 0, &headers);
  StringWriter writer(text);
  headers.WriteAsHttp(&writer, message_handler());
}

void RewriteTestBase::AppendDefaultHeaders(
    const ContentType& content_type, int64 original_content_length,
    GoogleString* text) {
  ResponseHeaders headers;
  PopulateDefaultHeaders(content_type, original_content_length, &headers);
  StringWriter writer(text);
  headers.WriteAsHttp(&writer, message_handler());
}

void RewriteTestBase::ServeResourceFromManyContexts(
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
void RewriteTestBase::ServeResourceFromNewContext(
    const GoogleString& resource_url,
    const StringPiece& expected_content,
    UrlNamer* new_rms_url_namer) {

  // New objects for the new server.
  SimpleStats stats;
  TestRewriteDriverFactory new_factory(GTestTempDir(), &mock_url_fetcher_);
  TestRewriteDriverFactory::Initialize(&stats);
  new_factory.SetUseTestUrlNamer(factory_->use_test_url_namer());
  new_factory.SetStatistics(&stats);
  ServerContext* new_resource_manager = new_factory.CreateResourceManager();
  if (new_rms_url_namer != NULL) {
    new_resource_manager->set_url_namer(new_rms_url_namer);
  }
  new_resource_manager->set_hasher(resource_manager_->hasher());
  RewriteOptions* new_options = options_->Clone();
  resource_manager_->ComputeSignature(new_options);
  RewriteDriver* new_rewrite_driver = MakeDriver(new_resource_manager,
                                                 new_options);
  new_factory.SetupWaitFetcher();

  // TODO(sligocki): We should set default request headers.
  ExpectStringAsyncFetch response_contents(true);

  // Check that we don't already have it in cache.
  HTTPValue value;
  ResponseHeaders response_headers;
  EXPECT_EQ(HTTPCache::kNotFound, HttpBlockingFind(
      resource_url, new_resource_manager->http_cache(), &value,
      &response_headers));
  // Initiate fetch.
  EXPECT_EQ(true, new_rewrite_driver->FetchResource(
      resource_url, &response_contents));

  // Content should not be set until we call the callback.
  EXPECT_FALSE(response_contents.done());
  EXPECT_EQ("", response_contents.buffer());

  // After we call the callback, it should be correct.
  new_factory.CallFetcherCallbacksForDriver(new_rewrite_driver);
  EXPECT_EQ(true, response_contents.done());
  EXPECT_STREQ(expected_content, response_contents.buffer());

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

GoogleString RewriteTestBase::AbsolutifyUrl(
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

void RewriteTestBase::DefaultResponseHeaders(
    const ContentType& content_type, int64 ttl_sec,
    ResponseHeaders* response_headers) {
  SetDefaultLongCacheHeaders(&content_type, response_headers);
  response_headers->Replace(HttpAttributes::kCacheControl,
                           StrCat("public, max-age=",
                                  Integer64ToString(ttl_sec)));
  response_headers->ComputeCaching();
}

// Initializes a resource for mock fetching.
void RewriteTestBase::SetResponseWithDefaultHeaders(
    const StringPiece& resource_name,
    const ContentType& content_type,
    const StringPiece& content,
    int64 ttl_sec) {
  GoogleString url = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  DefaultResponseHeaders(content_type, ttl_sec, &response_headers);
  // Do not set Etag and Last-Modified headers to the constants since they make
  // conditional refreshes always succeed and aren't updated in tests when the
  // actual response is updated.
  response_headers.RemoveAll(HttpAttributes::kEtag);
  response_headers.RemoveAll(HttpAttributes::kLastModified);
  SetFetchResponse(url, response_headers, content);
}

void RewriteTestBase::SetFetchResponse404(
    const StringPiece& resource_name) {
  GoogleString name = AbsolutifyUrl(resource_name);
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.SetStatusAndReason(HttpStatus::kNotFound);
  SetFetchResponse(name, response_headers, StringPiece());
}

void RewriteTestBase::AddFileToMockFetcher(
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
  SetResponseWithDefaultHeaders(url, content_type, contents, ttl_sec);
}

// Helper function to test resource fetching, returning true if the fetch
// succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
// on the status and EXPECT_EQ on the content.
bool RewriteTestBase::FetchResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    GoogleString* content, ResponseHeaders* response) {
  GoogleString url = Encode(path, filter_id, "0", name, ext);
  return FetchResourceUrl(url, content, response);
}

bool RewriteTestBase::FetchResource(
    const StringPiece& path, const StringPiece& filter_id,
    const StringPiece& name, const StringPiece& ext,
    GoogleString* content) {
  ResponseHeaders response;
  return FetchResource(path, filter_id, name, ext, content, &response);
}

bool RewriteTestBase::FetchResourceUrl(
    const StringPiece& url, GoogleString* content) {
  ResponseHeaders response;
  return FetchResourceUrl(url, content, &response);
}

bool RewriteTestBase::FetchResourceUrl(
    const StringPiece& url, GoogleString* content, ResponseHeaders* response) {
  content->clear();
  StringAsyncFetch async_fetch(content);
  async_fetch.set_response_headers(response);
  bool fetched = rewrite_driver_->FetchResource(url, &async_fetch);

  // Make sure we let the rewrite complete, and also wait for the driver to be
  // idle so we can reuse it safely.
  rewrite_driver_->WaitForShutDown();
  rewrite_driver_->Clear();

  // The callback should be called if and only if FetchResource returns true.
  EXPECT_EQ(fetched, async_fetch.done());
  return fetched && async_fetch.success();
}

void RewriteTestBase::TestServeFiles(
    const ContentType* content_type,
    const StringPiece& filter_id,
    const StringPiece& rewritten_ext,
    const StringPiece& orig_name,
    const StringPiece& orig_content,
    const StringPiece& rewritten_name,
    const StringPiece& rewritten_content) {

  GoogleString expected_rewritten_path = Encode(kTestDomain, filter_id, "0",
                                                rewritten_name, rewritten_ext);
  GoogleString content;

  // When we start, there are no mock fetchers, so we'll need to get it
  // from the cache.
  ResponseHeaders headers;
  resource_manager_->SetDefaultLongCacheHeaders(content_type, &headers);
  HTTPCache* http_cache = resource_manager_->http_cache();
  http_cache->Put(expected_rewritten_path, &headers,
                  rewritten_content, message_handler());
  EXPECT_EQ(0U, lru_cache()->num_hits());
  EXPECT_TRUE(FetchResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(1U, lru_cache()->num_hits());
  EXPECT_EQ(rewritten_content, content);

  // Now nuke the cache, get it via a fetch.
  lru_cache()->Clear();
  SetResponseWithDefaultHeaders(orig_name, *content_type,
                                orig_content, 100 /* ttl in seconds */);
  EXPECT_TRUE(FetchResource(kTestDomain, filter_id,
                            rewritten_name, rewritten_ext, &content));
  EXPECT_EQ(rewritten_content, content);

  // Now we expect the cache entry to be there.
  RewriteFilter* filter = rewrite_driver_->FindFilter(filter_id);
  if (!filter->ComputeOnTheFly()) {
    HTTPValue value;
    ResponseHeaders response_headers;
    EXPECT_EQ(HTTPCache::kFound, HttpBlockingFind(
        expected_rewritten_path, http_cache, &value, &response_headers));
  }
}

// Just check if we can fetch a resource successfully, ignore response.
bool RewriteTestBase::TryFetchResource(const StringPiece& url) {
  GoogleString contents;
  ResponseHeaders response;
  return FetchResourceUrl(url, &contents, &response);
}


RewriteTestBase::CssLink::CssLink(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock)
    : url_(url.data(), url.size()),
      content_(content.data(), content.size()),
      media_(media.data(), media.size()),
      supply_mock_(supply_mock) {
}

RewriteTestBase::CssLink::Vector::~Vector() {
  STLDeleteElements(this);
}

void RewriteTestBase::CssLink::Vector::Add(
    const StringPiece& url, const StringPiece& content,
    const StringPiece& media, bool supply_mock) {
  push_back(new CssLink(url, content, media, supply_mock));
}

bool RewriteTestBase::CssLink::DecomposeCombinedUrl(
    GoogleString* base, StringVector* segments, MessageHandler* handler) {
  GoogleUrl gurl(url_);
  bool ret = false;
  if (gurl.is_valid()) {
    gurl.AllExceptLeaf().CopyToString(base);
    ResourceNamer namer;
    if (namer.Decode(gurl.LeafWithQuery()) &&
        (namer.id() == RewriteOptions::kCssCombinerId)) {
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
               RewriteTestBase::CssLink::Vector* css_links)
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
      css_links_->Add(href->DecodedValueOrNull(), content, media, false);
    }
  }

  virtual const char* Name() const { return "CssCollector"; }

 private:
  RewriteTestBase::CssLink::Vector* css_links_;
  CssTagScanner css_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(CssCollector);
};

}  // namespace

// Collects just the hrefs from CSS links into a string vector.
void RewriteTestBase::CollectCssLinks(
    const StringPiece& id, const StringPiece& html, StringVector* css_links) {
  CssLink::Vector v;
  CollectCssLinks(id, html, &v);
  for (int i = 0, n = v.size(); i < n; ++i) {
    css_links->push_back(v[i]->url_);
  }
}

// Collects all information about CSS links into a CssLink::Vector.
void RewriteTestBase::CollectCssLinks(
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

void RewriteTestBase::EncodePathAndLeaf(const StringPiece& id,
                                                const StringPiece& hash,
                                                const StringVector& name_vector,
                                                const StringPiece& ext,
                                                ResourceNamer* namer) {
  namer->set_id(id);
  namer->set_hash(hash);

  // We only want to encode the last path-segment of 'name'.
  // Note that this block of code could be avoided if all call-sites
  // put subdirectory info in the 'path' argument, but it turns out
  // to be a lot more convenient for tests if we allow relative paths
  // in the 'name' argument for this method, so the one-time effort of
  // teasing out the leaf and encoding that saves a whole lot of clutter
  // in, at least, CacheExtenderTest.
  //
  // Note that this can only be done for 1-element name_vectors.
  for (int i = 0, n = name_vector.size(); i < n; ++i) {
    const GoogleString& name = name_vector[i];
    CHECK(name.find('/') == GoogleString::npos) << "No slashes should be "
        "found in " << name << " but we found at least one.  "
        "Put it in the path";
  }

  ResourceContext context;
  const UrlSegmentEncoder* encoder = FindEncoder(id);
  GoogleString encoded_name;
  encoder->Encode(name_vector, &context, &encoded_name);
  namer->set_name(encoded_name);
  namer->set_ext(ext);
}

const UrlSegmentEncoder* RewriteTestBase::FindEncoder(
    const StringPiece& id) const {
  RewriteFilter* filter = rewrite_driver_->FindFilter(id);
  ResourceContext context;
  return (filter == NULL) ? &default_encoder_ : filter->encoder();
}

GoogleString RewriteTestBase::Encode(const StringPiece& path,
                                             const StringPiece& id,
                                             const StringPiece& hash,
                                             const StringVector& name_vector,
                                             const StringPiece& ext) {
  return EncodeWithBase(kTestDomain, path, id, hash, name_vector, ext);
}

GoogleString RewriteTestBase::EncodeNormal(
    const StringPiece& path,
    const StringPiece& id,
    const StringPiece& hash,
    const StringVector& name_vector,
    const StringPiece& ext) {
  ResourceNamer namer;
  EncodePathAndLeaf(id, hash, name_vector, ext, &namer);
  return StrCat(path, namer.Encode());
}

GoogleString RewriteTestBase::EncodeWithBase(
    const StringPiece& base,
    const StringPiece& path,
    const StringPiece& id,
    const StringPiece& hash,
    const StringVector& name_vector,
    const StringPiece& ext) {
  if (factory()->use_test_url_namer() &&
      !TestUrlNamer::UseNormalEncoding() &&
      !options()->domain_lawyer()->can_rewrite_domains() &&
      !path.empty()) {
    ResourceNamer namer;
    EncodePathAndLeaf(id, hash, name_vector, ext, &namer);
    GoogleUrl path_gurl(path);
    if (path_gurl.is_valid()) {
      return TestUrlNamer::EncodeUrl(base, path_gurl.Origin(),
                                     path_gurl.PathSansLeaf(), namer);
    } else {
      return TestUrlNamer::EncodeUrl(base, "", path, namer);
    }
  }

  return EncodeNormal(path, id, hash, name_vector, ext);
}

// Helper function which instantiates an encoder, collects the
// required arguments and calls the virtual Encode().
GoogleString RewriteTestBase::EncodeCssName(const StringPiece& name,
                                                    bool supports_webp,
                                                    bool can_inline) {
  CssUrlEncoder encoder;
  ResourceContext resource_context;
  resource_context.set_inline_images(can_inline);
  resource_context.set_attempt_webp(supports_webp);
  StringVector urls;
  GoogleString encoded_url;
  name.CopyToString(StringVectorAdd(&urls));
  encoder.Encode(urls, &resource_context, &encoded_url);
  return encoded_url;
}

GoogleString RewriteTestBase::ChangeSuffix(
    GoogleString old_url, bool append_new_suffix,
    StringPiece old_suffix, StringPiece new_suffix) {
  if (!StringCaseEndsWith(old_url, old_suffix)) {
    ADD_FAILURE() << "Can't seem to find old extension!";
    return GoogleString();
  }

  if (append_new_suffix) {
    return StrCat(old_url, new_suffix);
  } else {
    return StrCat(
        old_url.substr(0, old_url.length() - old_suffix.length()),
        new_suffix);
  }
}

void RewriteTestBase::SetupWaitFetcher() {
  factory_->SetupWaitFetcher();
}

void RewriteTestBase::CallFetcherCallbacks() {
  factory_->CallFetcherCallbacksForDriver(rewrite_driver_);
}

void RewriteTestBase::SetUseManagedRewriteDrivers(
    bool use_managed_rewrite_drivers) {
  use_managed_rewrite_drivers_ = use_managed_rewrite_drivers;
}

RewriteDriver* RewriteTestBase::MakeDriver(
    ServerContext* resource_manager, RewriteOptions* options) {
  // We use unmanaged drivers rather than NewCustomDriver here so
  // that _test.cc files can add options after the driver was created
  // and before the filters are added.
  //
  // TODO(jmarantz): change call-sites to make this use a more
  // standard flow.
  RewriteDriver* rd;
  if (!use_managed_rewrite_drivers_) {
    rd = resource_manager->NewUnmanagedRewriteDriver(true, options);
    rd->set_externally_managed(true);
  } else {
    rd = resource_manager->NewCustomRewriteDriver(options);
  }
  // As we are using mock time, we need to set a consistent deadline here,
  // as otherwise when running under Valgrind some tests will finish
  // with different HTML headers than expected.
  rd->set_rewrite_deadline_ms(20);
  return rd;
}

void RewriteTestBase::TestRetainExtraHeaders(
    const StringPiece& name,
    const StringPiece& filter_id,
    const StringPiece& ext) {
  GoogleString url = AbsolutifyUrl(name);

  // Add some extra headers.
  AddToResponse(url, HttpAttributes::kEtag, "Custom-Etag");
  AddToResponse(url, "extra", "attribute");
  AddToResponse(url, HttpAttributes::kSetCookie, "Custom-Cookie");

  GoogleString content;
  ResponseHeaders response;

  GoogleString rewritten_url = Encode(kTestDomain, filter_id, "0", name, ext);
  ASSERT_TRUE(FetchResourceUrl(rewritten_url, &content, &response));

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

void RewriteTestBase::ClearStats() {
  statistics()->Clear();
  lru_cache()->ClearStats();
  counting_url_async_fetcher()->Clear();
  file_system()->ClearStats();
}

void RewriteTestBase::SetCacheDelayUs(int64 delay_us) {
  factory_->mock_time_cache()->set_delay_us(delay_us);
}

void RewriteTestBase::SetUseTestUrlNamer(bool use_test_url_namer) {
  factory_->SetUseTestUrlNamer(use_test_url_namer);
  resource_manager_->set_url_namer(factory_->url_namer());
  other_factory_->SetUseTestUrlNamer(use_test_url_namer);
  other_resource_manager_->set_url_namer(other_factory_->url_namer());
}

namespace {

class BlockingResourceCallback : public Resource::AsyncCallback {
 public:
  explicit BlockingResourceCallback(const ResourcePtr& resource)
      : Resource::AsyncCallback(resource),
        done_(false),
        success_(false) {
  }
  virtual ~BlockingResourceCallback() {}
  virtual void Done(bool success) {
    done_ = true;
    success_ = success;
  }
  bool done() const { return done_; }
  bool success() const { return success_; }

 private:
  bool done_;
  bool success_;
};

class DeferredResourceCallback : public Resource::AsyncCallback {
 public:
  explicit DeferredResourceCallback(const ResourcePtr& resource)
      : Resource::AsyncCallback(resource) {
  }
  virtual ~DeferredResourceCallback() {}
  virtual void Done(bool success) {
    CHECK(success);
    delete this;
  }
};

class HttpCallback : public HTTPCache::Callback {
 public:
  HttpCallback() : done_(false) {}
  virtual ~HttpCallback() {}
  virtual bool IsCacheValid(const GoogleString& key,
                            const ResponseHeaders& headers) {
    return true;
  }
  virtual void Done(HTTPCache::FindResult find_result) {
    done_ = true;
    result_ = find_result;
  }
  bool done() const { return done_; }
  HTTPCache::FindResult result() { return result_; }

 private:
  bool done_;
  HTTPCache::FindResult result_;
};

}  // namespace

bool RewriteTestBase::ReadIfCached(const ResourcePtr& resource) {
  BlockingResourceCallback callback(resource);
  rewrite_driver()->ReadAsync(&callback, message_handler());
  CHECK(callback.done());
  if (callback.success()) {
    CHECK(resource->loaded());
  }
  return callback.success();
}

void RewriteTestBase::InitiateResourceRead(
    const ResourcePtr& resource) {
  DeferredResourceCallback* callback = new DeferredResourceCallback(resource);
  rewrite_driver()->ReadAsync(callback, message_handler());
}

HTTPCache::FindResult RewriteTestBase::HttpBlockingFind(
    const GoogleString& key, HTTPCache* http_cache, HTTPValue* value_out,
    ResponseHeaders* headers) {
  HttpCallback callback;
  callback.set_response_headers(headers);
  http_cache->Find(key, message_handler(), &callback);
  CHECK(callback.done());
  value_out->Link(callback.http_value());
  return callback.result();
}

void RewriteTestBase::SetMimetype(const StringPiece& mimetype) {
  rewrite_driver()->set_response_headers_ptr(&response_headers_);
  response_headers_.Add(HttpAttributes::kContentType, mimetype);
  response_headers_.ComputeCaching();
}

// Logging at the INFO level slows down tests, adds to the noise, and
// adds considerably to the speed variability.
class ResourceManagerProcessContext {
 public:
  ResourceManagerProcessContext() {
    logging::SetMinLogLevel(logging::LOG_WARNING);
  }

 private:
  ProcessContext process_context_;
};
ResourceManagerProcessContext resource_manager_process_context;

}  // namespace net_instaweb
