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

// Author: morlovich@google.com (Maksim Orlovich)

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

const char kTestFilterPrefix[] = "tf";
const char kTestEncoderUrlExtra[] = "UrlExtraStuff";

// This should be the same as used for freshening. It may not be 100%
// robust against rounding errors, however.
const int kTtlSec = ResponseHeaders::kImplicitCacheTtlMs / Timer::kSecondMs;
const int kTtlMs  = kTtlSec * Timer::kSecondMs;

// A simple RewriteSingleResourceFilter subclass that rewrites
// <tag src=...> and keeps some statistics.
//
// It rewrites resources as follows:
// 1) If original contents are equal to bad, it fails the rewrite
// 2) If the contents are a $ sign, it claims the system is too busy
// 3) otherwise it repeats the contents twice.
class TestRewriter : public RewriteSingleResourceFilter {
 public:
  TestRewriter(RewriteDriver* driver, bool create_custom_encoder)
      : RewriteSingleResourceFilter(driver, kTestFilterPrefix),
        format_version_(0),
        num_cached_results_(0),
        num_optimizable_(0),
        num_rewrites_called_(0),
        create_custom_encoder_(create_custom_encoder),
        active_custom_encoder_(0) {
  }

  virtual ~TestRewriter() {
    EXPECT_EQ(NULL, active_custom_encoder_);
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}

  virtual void EndElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kTag) {
      HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
      if (src != NULL) {
        TryRewrite(src);
      }
    }
  }

  virtual const char* Name() const { return "TestRewriter"; }

  // Number of times RewriteLoadedResource got called
  int num_rewrites_called() const { return num_rewrites_called_; }

  // Number of times a cached result was available when rewriting;
  // including both when looked up from cache or created by the
  // base class
  int num_cached_results() const { return num_cached_results_; }

  // How many times resource was known optimizable when rewriting
  int num_optimizable() const { return num_optimizable_; }

  // Changes which file format we advertize
  void set_format_version(int ver) { format_version_ = ver; }

  // Do we use a custom encoder (which prepends kTestEncoderUrlExtra?)
  bool create_custom_encoder() const { return create_custom_encoder_; }

  virtual RewriteResult RewriteLoadedResource(const Resource* input_resource,
                                              OutputResource* output_resource,
                                              UrlSegmentEncoder* encoder) {
    if (create_custom_encoder_) {
      EXPECT_EQ(active_custom_encoder_, encoder);
    } else {
      EXPECT_EQ(resource_manager_->url_escaper(), encoder);
    }

    ++num_rewrites_called_;
    EXPECT_TRUE(input_resource != NULL);
    EXPECT_TRUE(output_resource != NULL);
    EXPECT_TRUE(input_resource->ContentsValid());

    StringPiece contents = input_resource->contents();
    if (contents == "bad") {
      return kRewriteFailed;
    }

    if (contents == "$") {
      return kTooBusy;
    }

    output_resource->SetType(&kContentTypeText);
    bool ok = resource_manager_->Write(
        HttpStatus::kOK, StrCat(contents, contents), output_resource,
        input_resource->metadata()->CacheExpirationTimeMs(),
        html_parse_->message_handler());
    return ok ? kRewriteOk : kRewriteFailed;
  }

  virtual int FilterCacheFormatVersion() const {
    return format_version_;
  }

  virtual UrlSegmentEncoder* CreateCustomUrlEncoder() const {
    if (create_custom_encoder_) {
      return new TestUrlEncoder(this, resource_manager_->url_escaper());
    } else {
      return RewriteSingleResourceFilter::CreateCustomUrlEncoder();
    }
  }

 private:
  friend class TestUrlEncoder;

  // This encoder simply adds/remove kTestEncoderUrlExtra in front of
  // name encoding, which is enough to see if it got invoked right.
  class TestUrlEncoder: public UrlSegmentEncoder {
   public:
    TestUrlEncoder(const TestRewriter* parent, UrlEscaper* url_escaper)
        : url_escaper_(url_escaper),
          parent_(parent) {
      EXPECT_EQ(NULL, parent_->active_custom_encoder_);
      parent_->active_custom_encoder_ = this;
    }

    virtual ~TestUrlEncoder() {
      EXPECT_EQ(this, parent_->active_custom_encoder_);
      parent_->active_custom_encoder_ = NULL;
    }

    virtual void EncodeToUrlSegment(
        const StringPiece& origin_url, std::string* rewritten_url) {
      *rewritten_url = kTestEncoderUrlExtra;
      url_escaper_->EncodeToUrlSegment(origin_url, rewritten_url);
    }

    virtual bool DecodeFromUrlSegment(const StringPiece& rewritten_url,
                                      std::string* origin_url) {
      const int magic_len = STATIC_STRLEN(kTestEncoderUrlExtra);
      return rewritten_url.starts_with(kTestEncoderUrlExtra) &&
             url_escaper_->DecodeFromUrlSegment(rewritten_url.substr(magic_len),
                                                origin_url);
    }

   private:
    UrlEscaper* url_escaper_;
    const TestRewriter* parent_;
  };

  void TryRewrite(HtmlElement::Attribute* src) {
    UrlSegmentEncoder* encoder = resource_manager_->url_escaper();
    if (create_custom_encoder_) {
      encoder = CreateCustomUrlEncoder();
    }

    scoped_ptr<OutputResource::CachedResult> result(
        RewriteWithCaching(StringPiece(src->value()), encoder));
    if (result.get() != NULL) {
      ++num_cached_results_;
      if (result->optimizable()) {
        ++num_optimizable_;
        src->SetValue(result->url());
      }
    }

    if (create_custom_encoder_) {
      delete encoder;
    }
  }

  int format_version_;
  int num_cached_results_;
  int num_optimizable_;
  int num_rewrites_called_;
  bool create_custom_encoder_;
  mutable TestUrlEncoder* active_custom_encoder_;
};

}  // namespace

class RewriteSingleResourceFilterTest
    : public ResourceManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    filter_ = new TestRewriter(&rewrite_driver_, GetParam());
    AddRewriteFilter(filter_);
    AddOtherRewriteFilter(
        new TestRewriter(&other_rewrite_driver_, GetParam()));

    MockResource("a.tst", "good", kTtlSec);
    MockResource("bad.tst", "bad", kTtlSec);
    MockResource("busy.tst", "$", kTtlSec);
    MockMissingResource("404.tst");

    in_tag_ = "<tag src=\"a.tst\"></tag>";
    out_tag_ = StrCat("<tag src=\"", kTestDomain, OutputName("a.tst"),
                      "\"></tag>");
  }

  // Create a resource with given data and TTL
  void MockResource(const char* rel_path, const StringPiece& data,
                    int64 ttlSec) {
    InitResponseHeaders(rel_path, kContentTypeText, data, ttlSec);
  }

  // Creates a resource that 404s
  void MockMissingResource(const char* rel_path) {
    ResponseHeaders response_headers;
    resource_manager_->SetDefaultHeaders(&kContentTypeText, &response_headers);
    response_headers.SetStatusAndReason(HttpStatus::kNotFound);
    mock_url_fetcher_.SetResponse(
        StrCat(kTestDomain, rel_path), response_headers, StringPiece());
  }

  // Returns the filename our test filter will produces for the given
  // input filename
  std::string OutputName(const StringPiece& in_name) {
    if (filter_->create_custom_encoder()) {
      return Encode("", kTestFilterPrefix, "0",
                    StrCat(kTestEncoderUrlExtra, in_name), "txt");
    } else {
      return Encode("", kTestFilterPrefix, "0", in_name, "txt");
    }
  }

  // Serves from relative URL
  bool ServeRelativeUrl(const StringPiece& rel_path, std::string* content) {
    return ServeResourceUrl(StrCat(kTestDomain, rel_path), content);
  }

  // Transfers ownership and may return NULL.
  OutputResource::CachedResult* CachedResultForInput(const char* url) {
    UrlSegmentEncoder* encoder = resource_manager_->url_escaper();
    if (filter_->create_custom_encoder()) {
      encoder = filter_->CreateCustomUrlEncoder();
    }

    scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResource(
        GURL(kTestDomain), url, &options_, &message_handler_));
    EXPECT_TRUE(input_resource.get() != NULL);
    scoped_ptr<OutputResource> output_resource(
        filter_->CreateOutputResourceFromResource(
            &kContentTypeText, encoder, input_resource.get()));
    EXPECT_TRUE(output_resource.get() != NULL);

    if (filter_->create_custom_encoder()) {
      delete encoder;
    }

    return output_resource->ReleaseCachedResult();
  }

  bool HasTimestamp(const OutputResource::CachedResult* cached) {
    std::string timestamp_val;
    return cached->Remembered(RewriteSingleResourceFilter::kInputTimestampKey,
                              &timestamp_val);
  }

  CountingUrlAsyncFetcher* SetupCountingFetcher() {
    CountingUrlAsyncFetcher* counter =
        new CountingUrlAsyncFetcher(&mock_url_async_fetcher_);
    rewrite_driver_.set_async_fetcher(counter);
    resource_manager_->set_url_async_fetcher(counter);
    return counter;
  }

  WaitUrlAsyncFetcher* SetupWaitFetcher() {
    WaitUrlAsyncFetcher* delayer =
        new WaitUrlAsyncFetcher(&mock_url_fetcher_);
    rewrite_driver_.set_async_fetcher(delayer);
    resource_manager_->set_url_async_fetcher(delayer);
    return delayer;
  }

  std::string in_tag_;
  std::string out_tag_;
  TestRewriter* filter_;  // owned by the rewrite_driver_.
};

TEST_P(RewriteSingleResourceFilterTest, BasicOperation) {
  ValidateExpected("basic1", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));

  // Should only have to rewrite once here
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(3, filter_->num_cached_results());
  EXPECT_EQ(3, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, VersionChange) {
  std::string in = StrCat(in_tag_, in_tag_, in_tag_);
  std::string out = StrCat(out_tag_, out_tag_, out_tag_);
  ValidateExpected("vc1", in, out);

  // Should only have to rewrite once here
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(3, filter_->num_cached_results());
  EXPECT_EQ(3, filter_->num_optimizable());

  // The next attempt should still use cache
  ValidateExpected("vc2", in, out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Now pretend to have upgraded --- this should dump the cache
  filter_->set_format_version(42);
  ValidateExpected("vc3", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // And now we're caching again
  ValidateExpected("vc4", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // Downgrade. Should dump the cache, too
  filter_->set_format_version(21);
  ValidateExpected("vc5", in, out);
  EXPECT_EQ(3, filter_->num_rewrites_called());

  // And now we're caching again
  ValidateExpected("vc6", in, out);
  EXPECT_EQ(3, filter_->num_rewrites_called());
}

// We should re-check bad resources when version number changes.
TEST_P(RewriteSingleResourceFilterTest, VersionChangeBad) {
  std::string in_tag = "<tag src=\"bad.tst\"></tag>";
  ValidateNoChanges("vc.bad", in_tag);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // cached with old version
  ValidateNoChanges("vc.bad2", in_tag);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // upgraded -- retried
  filter_->set_format_version(42);
  ValidateNoChanges("vc.bad3", in_tag);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // And now cached again
  ValidateNoChanges("vc.bad4", in_tag);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // downgrade -- retried.
  filter_->set_format_version(21);
  ValidateNoChanges("vc.bad5", in_tag);
  EXPECT_EQ(3, filter_->num_rewrites_called());

  // And now cached again
  ValidateNoChanges("vc.bad6", in_tag);
  EXPECT_EQ(3, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, BasicAsync) {
  scoped_ptr<WaitUrlAsyncFetcher> delayer(SetupWaitFetcher());

  // First fetch should not rewrite since resources haven't loaded yet
  ValidateNoChanges("async.not_yet", in_tag_);
  EXPECT_EQ(0, filter_->num_rewrites_called());

  // Now let it load
  delayer->CallCallbacks();

  // This time should rewrite
  ValidateExpected("async.loaded", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, CacheBad) {
  std::string in_tag = "<tag src=\"bad.tst\"></tag>";
  std::string out_tag = in_tag;
  ValidateExpected("cache.bad", StrCat(in_tag, in_tag, in_tag),
                   StrCat(out_tag, out_tag, out_tag));

  // Should call rewrite once, and then remember it's not optimizable
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(3, filter_->num_cached_results());
  EXPECT_EQ(0, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, CacheBusy) {
  // In case of busy, it should keep trying every time,
  // as it's meant to represent intermitent system load and
  // not a conclusion about the resource.
  std::string in_tag = "<tag src=\"busy.tst\"></tag>";
  std::string out_tag = in_tag;
  ValidateExpected("cache.busy", StrCat(in_tag, in_tag, in_tag),
                   StrCat(out_tag, out_tag, out_tag));

  EXPECT_EQ(3, filter_->num_rewrites_called());
  EXPECT_EQ(0, filter_->num_cached_results());
  EXPECT_EQ(0, filter_->num_optimizable());
}


TEST_P(RewriteSingleResourceFilterTest, Cache404) {
  // 404s should come up as unoptimizable as well.
  std::string in_tag = "<tag src=\"404.tst\"></tag>";
  std::string out_tag = in_tag;
  ValidateExpected("cache.404", StrCat(in_tag, in_tag, in_tag),
                   StrCat(out_tag, out_tag, out_tag));

  // Should call rewrite zero times (as 404), and remember it's not optimizable
  // past the first fetch, where it's not immediately sure
  // (but it will be OK if that changes)
  EXPECT_EQ(0, filter_->num_rewrites_called());
  EXPECT_EQ(2, filter_->num_cached_results());
  EXPECT_EQ(0, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, InvalidUrl) {
  // Make sure we don't have problems with bad URLs.
  ValidateNoChanges("bad_url", "<tag src=\"http://evil.com\"></tag>");
}

TEST_P(RewriteSingleResourceFilterTest, CacheExpire) {
  // Make sure we don't cache past the TTL.
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, filter_->num_cached_results());
  EXPECT_EQ(1, filter_->num_optimizable());

  // Next fetch should be still in there.
  mock_timer()->advance_ms(kTtlMs / 2);
  ValidateExpected("initial.2", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, filter_->num_cached_results());
  EXPECT_EQ(2, filter_->num_optimizable());

  // ... but not once we get past the ttl, when we don't rewrite since the data
  // will have expiration time in the past, making it uncacheable for us.
  mock_timer()->advance_ms(kTtlMs * 2);
  ValidateNoChanges("expire", in_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, filter_->num_cached_results());
  EXPECT_EQ(2, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, CacheNoFreshen) {
  scoped_ptr<CountingUrlAsyncFetcher> counter(SetupCountingFetcher());

  // Start with non-zero time
  mock_timer()->advance_ms(kTtlMs / 2);
  MockResource("a.tst", "whatever", kTtlSec);

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counter->fetch_count());

  // Advance time past TTL, but re-mock the resource so it can be refetched
  mock_timer()->advance_ms(kTtlMs + 10);
  MockResource("a.tst", "whatever", kTtlSec);
  ValidateExpected("refetch", in_tag_, out_tag_);
  EXPECT_EQ(2, filter_->num_rewrites_called());
  EXPECT_EQ(2, counter->fetch_count());
}

TEST_P(RewriteSingleResourceFilterTest, CacheFreshen) {
  scoped_ptr<CountingUrlAsyncFetcher> counter(SetupCountingFetcher());

  // Start with non-zero time
  mock_timer()->advance_ms(kTtlMs / 2);
  MockResource("a.tst", "whatever", kTtlSec);

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counter->fetch_count());

  // Advance close to TTL and rewrite, having updated the data.
  // We expect it to be freshened to that.
  mock_timer()->advance_ms(kTtlMs * 9 / 10);
  MockResource("a.tst", "whatever", kTtlSec);
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, counter->fetch_count());  // the 2nd fetch is freshening

  // Now advance past original TTL, but it should still be alive
  // due to freshening.
  mock_timer()->advance_ms(kTtlMs / 2);
  ValidateExpected("refetch", in_tag_, out_tag_);
  // we have to recompute since the rewrite cache entry has expired
  // (this behavior may change in the future)
  EXPECT_EQ(2, filter_->num_rewrites_called());
  // definitely should not have to fetch here --- freshening should have
  // done it already.
  EXPECT_EQ(2, counter->fetch_count());
}

// Make sure that fetching normal content works
TEST_P(RewriteSingleResourceFilterTest, FetchGood) {
  std::string out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

// Variants of above that also test caching between fetch & rewrite paths
TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache1) {
  ValidateExpected("compute_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());

  std::string out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache2) {
  std::string out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  ValidateExpected("reused_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Make sure the above also cached the timestamp
  scoped_ptr<OutputResource::CachedResult> cached(
      CachedResultForInput("a.tst"));
  ASSERT_TRUE(cached.get() != NULL);
  EXPECT_TRUE(HasTimestamp(cached.get()));
}

// Regression test: Fetch() should update cache version, too.
// TODO(morlovich): We want to run the entire test with different
// version, but the Google Test version we depend on is too old to have
// ::testing::WithParamInterface<int>
TEST_P(RewriteSingleResourceFilterTest, FetchFirstVersioned) {
  filter_->set_format_version(1);
  std::string out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  ValidateExpected("reused_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

// Failure path #1: fetch of a URL we refuse to rewrite. Should return
// unchanged
TEST_P(RewriteSingleResourceFilterTest, FetchRewriteFailed) {
  scoped_ptr<CountingUrlAsyncFetcher> counter(SetupCountingFetcher());

  std::string out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("bad.tst"), &out));
  EXPECT_EQ("bad", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counter->fetch_count());

  // Make sure the above also cached the failure.
  ValidateNoChanges("postfetch.bad", "<tag src=\"bad.tst\"></tag>");
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counter->fetch_count());
}

// Rewriting a 404 however propagates error
TEST_P(RewriteSingleResourceFilterTest, Fetch404) {
  std::string out;
  ASSERT_FALSE(ServeRelativeUrl(OutputName("404.tst"), &out));

  // Make sure the above also cached the failure.
  scoped_ptr<OutputResource::CachedResult> cached(
      CachedResultForInput("404.tst"));
  ASSERT_TRUE(cached.get() != NULL);
  EXPECT_FALSE(cached->optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, FetchInvalidResourceName) {
  std::string out;
  ASSERT_FALSE(ServeRelativeUrl("404,.tst.pagespeed.tf.0.txt", &out));
}

INSTANTIATE_TEST_CASE_P(RewriteSingleResourceFilterTestInstance,
                        RewriteSingleResourceFilterTest,
                        ::testing::Bool());

}  // namespace net_instaweb
