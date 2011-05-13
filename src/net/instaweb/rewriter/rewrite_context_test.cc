/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the RewriteContext class.  This is made simplest by
// setting up some dummy rewriters in our test framework.

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kTrimWhitespaceFilterId[] = "tw";
const char kUpperCaseFilterId[] = "uc";

}  // namespace

namespace net_instaweb {

// Simple test filter just trims whitespace from the input resource.
class TrimWhitespaceRewriter : public SimpleTextFilter::Rewriter {
 public:
  explicit TrimWhitespaceRewriter(OutputResourceKind kind) : kind_(kind) {}
  static SimpleTextFilter* MakeFilter(OutputResourceKind kind,
                                      RewriteDriver* driver) {
    return new SimpleTextFilter(new TrimWhitespaceRewriter(kind), driver);
  }

 protected:
  REFCOUNT_FRIEND_DECLARATION(TrimWhitespaceRewriter);
  virtual ~TrimWhitespaceRewriter() {}

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ResourceManager* resource_manager) {
    TrimWhitespace(in, out);
    return in != *out;
  }
  virtual HtmlElement::Attribute* FindResourceAttribute(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      return element->FindAttribute(HtmlName::kHref);
    }
    return NULL;
  }
  virtual OutputResourceKind kind() const { return kind_; }
  virtual const char* id() const { return kTrimWhitespaceFilterId; }
  virtual const char* name() const { return "TrimWhitespace"; }

 private:
  OutputResourceKind kind_;

  DISALLOW_COPY_AND_ASSIGN(TrimWhitespaceRewriter);
};

// A similarly structured test-filter: this one just upper-cases its text.
class UpperCaseRewriter : public SimpleTextFilter::Rewriter {
 public:
  explicit UpperCaseRewriter(OutputResourceKind kind) : kind_(kind) {}
  static SimpleTextFilter* MakeFilter(OutputResourceKind kind,
                                      RewriteDriver* driver) {
    return new SimpleTextFilter(new UpperCaseRewriter(kind), driver);
  }

 protected:
  REFCOUNT_FRIEND_DECLARATION(UpperCaseRewriter);
  virtual ~UpperCaseRewriter() {}

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ResourceManager* resource_manager) {
    in.CopyToString(out);
    UpperString(out);
    return in != *out;
  }
  virtual HtmlElement::Attribute* FindResourceAttribute(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      return element->FindAttribute(HtmlName::kHref);
    }
    return NULL;
  }
  virtual OutputResourceKind kind() const { return kind_; }
  virtual const char* id() const { return kUpperCaseFilterId; }
  virtual const char* name() const { return "UpperCase"; }

 private:
  OutputResourceKind kind_;

  DISALLOW_COPY_AND_ASSIGN(UpperCaseRewriter);
};

class RewriteContextTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    rewrite_driver_.SetAsynchronousRewrites(true);
  }

  virtual bool AddBody() const { return false; }

  void InitResources() {
    ResponseHeaders default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse("http://test.com/a.css", default_css_header,
                                  " a ");  // trimmable
    mock_url_fetcher_.SetResponse("http://test.com/b.css", default_css_header,
                                  "b");    // not trimmable
  }

  void InitTrimFilters(OutputResourceKind kind) {
    InitTrimFilter(kind, &rewrite_driver_);
    InitTrimFilter(kind, &other_rewrite_driver_);
  }

  void InitTwoFilters(OutputResourceKind kind) {
    InitTwoFilters(kind, &rewrite_driver_);
    InitTwoFilters(kind, &other_rewrite_driver_);
  }

  void InitTrimFilter(OutputResourceKind kind, RewriteDriver* rewrite_driver) {
    rewrite_driver->AddRewriteFilter(
        TrimWhitespaceRewriter::MakeFilter(kind, &rewrite_driver_));
    rewrite_driver->AddFilters();
  }

  void InitTwoFilters(OutputResourceKind kind, RewriteDriver* rewrite_driver) {
    rewrite_driver->AddRewriteFilter(
        UpperCaseRewriter::MakeFilter(kind, rewrite_driver));
    InitTrimFilter(kind, rewrite_driver);
  }

  GoogleString CssLink(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }

  void ClearStats() {
    lru_cache_->ClearStats();
    counting_url_async_fetcher_.Clear();
  }
};

TEST_F(RewriteContextTest, TrimOnTheFlyOptimizable) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // The first rewrite was successful because we got an 'instant' url
  // fetch, not because we did any cache lookups. We'll have 2 cache
  // misses: one for the OutputPartitions, one for the fetch.  We
  // should need two items in the cache: the element and the resource
  // mapping (OutputPartitions).  The output resource should not be
  // stored.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // The second time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a
  // single cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyUnOptimizable) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

// In this variant, we use the same whitespace trimmer, but we pretend that this
// is an expensive operation, so we want to cache the output resource.  This
// means we will do an extra cache insert on the first iteration for each input.
TEST_F(RewriteContextTest, TrimRewrittenOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The first rewrite was successful because we got an 'instant' url
  // fetch, not because we did any cache lookups. We'll have 2 cache
  // misses: one for the OutputPartitions, one for the fetch.  We
  // should need two items in the cache: the element and the resource
  // mapping (OutputPartitions).  The output resource should not be
  // stored.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(3, lru_cache_->num_inserts());  // 3 because it's kRewrittenResource
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, TrimRewrittenNonOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.  We'll cache
  // the successfully fetched resource, and the OutputPartitions which
  // indicates the unsuccessful optimization.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

// In the above tests, our URL fetcher called its callback directly, allowing
// the Rewrite to occur while the RewriteDriver was still attached.  In this
// run, we will delay the URL fetcher's callback so that the initial Rewrite
// will not take place until after the HTML has been flushed.
TEST_F(RewriteContextTest, TrimDelayed) {
  SetupWaitFetcher();
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable", CssLink("a.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // Now we'll let the fetcher call its callbacks -- we'll see the
  // cache-inserts now, and the next rewrite will succeed.
  //
  // TODO(jmarantz): Implement and test a threaded Rewrite.
  CallFetcherCallbacks();
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
  ClearStats();
}

TEST_F(RewriteContextTest, TrimFetchOnTheFly) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());   // 1 because output is not saved
  EXPECT_EQ(1, lru_cache_->num_inserts());  // ditto
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again.  This time the input URL is cached.
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, TrimFetchRewritten) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache_->num_hits());
  // We did the output_resource lookup twice: once before acquiring the lock,
  // and the second time after acquiring the lock, because presumably whoever
  // released the lock has now written the resource.
  //
  // TODO(jmarantz): have the lock-code return whether it had to wait to
  // get the lock or was able to acquire it immediately to avoid the
  // second cache lookup.
  EXPECT_EQ(3, lru_cache_->num_misses());   // output resource(twice), input
  EXPECT_EQ(2, lru_cache_->num_inserts());  // output resource(once), input
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again: the output URL is cached.
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, FetchColdCacheOnTheFly) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  ClearStats();
  TestServeFiles(&kContentTypeCss, kTrimWhitespaceFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");
}

TEST_F(RewriteContextTest, FetchColdCacheRewritten) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  ClearStats();
  TestServeFiles(&kContentTypeCss, kTrimWhitespaceFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");
}

TEST_F(RewriteContextTest, OnTheFlyNotFound) {
  InitTrimFilters(kOnTheFlyResource);

  // note: no InitResources so we'll get a file-not found.
  mock_url_fetcher_.set_fail_on_unexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, RewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  mock_url_fetcher_.set_fail_on_unexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(2, lru_cache_->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

// In this testcase we'll attempt to serve a rewritten resource, but having
// failed to call InitResources we will not be able to do the on-the-fly
// rewrite.
TEST_F(RewriteContextTest, FetchColdCacheOnTheFlyNotFound) {
  InitTrimFilters(kOnTheFlyResource);

  // note: no InitResources so we'll get a file-not found.
  mock_url_fetcher_.set_fail_on_unexpected(false);

  GoogleString content;
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
  EXPECT_EQ(1, lru_cache_->num_inserts());  // We "remember" the fetch failure
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a 'hit' which will inform us
  // that this resource is not fetchable.
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(1, lru_cache_->num_hits());
  EXPECT_EQ(0, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());  // We "remember" the fetch failure
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

// Same testcase, but with a non-on-the-fly resource.
TEST_F(RewriteContextTest, FetchColdCacheRewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  mock_url_fetcher_.set_fail_on_unexpected(false);

  GoogleString content;
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(0, lru_cache_->num_hits());

  // We lookup the output resource twice plus the inputs.
  EXPECT_EQ(3, lru_cache_->num_misses());

  // We currently "remember" the fetch failure, but *not* the failed
  // rewrite.
  EXPECT_EQ(1, lru_cache_->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher_.fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a 'hit' which will inform us
  // that this resource is not fetchable.
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(1, lru_cache_->num_hits());

  // Because we don't (currently) remember the failed output cache lookup we
  // will get two new cache misses here as well: once before we try to acquire
  // the lock, and the second after having acquired the lock.
  EXPECT_EQ(2, lru_cache_->num_misses());
  EXPECT_EQ(0, lru_cache_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher_.fetch_count());
}

TEST_F(RewriteContextTest, TwoFilters) {
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateExpected(
      "trimmable", CssLink("a.css"),
      CssLink("http://test.com/a.css,Muc.0.css.pagespeed.tw.0.css"));
}

TEST_F(RewriteContextTest, TwoFiltersDelayedFetches) {
  SetupWaitFetcher();
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable1", CssLink("a.css"));
  CallFetcherCallbacks();
  ValidateExpected(
      "trimmable2", CssLink("a.css"),
      CssLink("http://test.com/a.css,Muc.0.css.pagespeed.tw.0.css"));

  // TODO(jmarantz): This is broken because we do not have the right graph
  // built yet between different RewriteContexts running on the same slots.
  // Fix this.
}

}  // namespace net_instaweb
