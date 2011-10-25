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

#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {
class MessageHandler;

namespace {

const char kTestFilterPrefix[] = "tf";
const char kTestEncoderUrlExtra[] = "UrlExtraStuff";


// These are functions rather than static constants because on MacOS
// we cannot seem to rely on correctly ordered initiazation of static
// constants.
//
// This should be the same as used for freshening. It may not be 100%
// robust against rounding errors, however.
int TtlSec() {
  return ResponseHeaders::kImplicitCacheTtlMs / Timer::kSecondMs;
}

int TtlMs() {
  return TtlSec() * Timer::kSecondMs;
}

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
        reuse_by_content_hash_(false) {
  }

  virtual ~TestRewriter() {
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

  void set_reuse_by_content_hash(bool r) { reuse_by_content_hash_ = r; }

  virtual RewriteResult RewriteLoadedResource(
      const ResourcePtr& input_resource,
      const OutputResourcePtr& output_resource) {
    ++num_rewrites_called_;
    EXPECT_TRUE(input_resource.get() != NULL);
    EXPECT_TRUE(output_resource.get() != NULL);
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
        HttpStatus::kOK, StrCat(contents, contents), output_resource.get(),
        input_resource->response_headers()->CacheExpirationTimeMs(),
        driver_->message_handler());
    return ok ? kRewriteOk : kRewriteFailed;
  }

  virtual int FilterCacheFormatVersion() const {
    return format_version_;
  }

  virtual bool ReuseByContentHash() const {
    return reuse_by_content_hash_;
  }

  virtual const UrlSegmentEncoder* encoder() const {
    if (create_custom_encoder_) {
      return &test_url_encoder_;
    } else {
      return driver_->default_encoder();
    }
  }

 private:
  friend class TestUrlEncoder;

  // This encoder simply adds/remove kTestEncoderUrlExtra in front of
  // name encoding, which is enough to see if it got invoked right.
  class TestUrlEncoder: public UrlSegmentEncoder {
   public:
    virtual ~TestUrlEncoder() {
    }

    virtual void Encode(const StringVector& urls, const ResourceContext* data,
                        GoogleString* rewritten_url) const {
      CHECK(data == NULL);
      CHECK_EQ(1, urls.size());
      *rewritten_url = kTestEncoderUrlExtra;
      UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
    }

    virtual bool Decode(const StringPiece& rewritten_url,
                        StringVector* urls,
                        ResourceContext* data,
                        MessageHandler* handler) const {
      const int magic_len = STATIC_STRLEN(kTestEncoderUrlExtra);
      urls->clear();
      urls->push_back(GoogleString());
      GoogleString& url = urls->back();
      return (rewritten_url.starts_with(kTestEncoderUrlExtra) &&
              UrlEscaper::DecodeFromUrlSegment(rewritten_url.substr(magic_len),
                                               &url));
    }
  };

  void TryRewrite(HtmlElement::Attribute* src) {
    scoped_ptr<CachedResult> result(
        RewriteWithCaching(StringPiece(src->value()), NULL));
    if (result.get() != NULL) {
      ++num_cached_results_;
      if (result->optimizable()) {
        ++num_optimizable_;
        src->SetValue(result->url());
      }
    }
  }

  int format_version_;
  int num_cached_results_;
  int num_optimizable_;
  int num_rewrites_called_;
  bool create_custom_encoder_;
  bool reuse_by_content_hash_;
  TestUrlEncoder test_url_encoder_;
};

}  // namespace

class RewriteSingleResourceFilterTest
    : public ResourceManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    filter_ = new TestRewriter(rewrite_driver(), GetParam());
    AddRewriteFilter(filter_);
    AddOtherRewriteFilter(
        new TestRewriter(other_rewrite_driver(), GetParam()));

    MockResource("a.tst", "good", TtlSec());
    MockResource("bad.tst", "bad", TtlSec());
    MockResource("busy.tst", "$", TtlSec());
    MockMissingResource("404.tst");

    in_tag_ = "<tag src=\"a.tst\"></tag>";
    out_tag_ = ComputeOutTag();
  }

  GoogleString ComputeOutTag() {
    return StrCat("<tag src=\"", kTestDomain, OutputName("a.tst"), "\"></tag>");
  }

  // Create a resource with given data and TTL
  void MockResource(const char* rel_path, const StringPiece& data,
                    int64 ttlSec) {
    InitResponseHeaders(rel_path, kContentTypeText, data, ttlSec);
  }

  // Creates a resource that 404s
  void MockMissingResource(const char* rel_path) {
    ResponseHeaders response_headers;
    SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
    response_headers.SetStatusAndReason(HttpStatus::kNotFound);
    SetFetchResponse(
        StrCat(kTestDomain, rel_path), response_headers, StringPiece());
  }

  // Returns the filename our test filter will produces for the given
  // input filename
  GoogleString OutputName(const StringPiece& in_name) {
    if (filter_->create_custom_encoder()) {
      return Encode("", kTestFilterPrefix, hasher()->Hash(""),
                    StrCat(kTestEncoderUrlExtra, in_name), "txt");
    } else {
      return Encode("", kTestFilterPrefix, hasher()->Hash(""), in_name, "txt");
    }
  }

  // Serves from relative URL
  bool ServeRelativeUrl(const StringPiece& rel_path, GoogleString* content) {
    return ServeResourceUrl(StrCat(kTestDomain, rel_path), content);
  }

  // Transfers ownership and may return NULL.
  CachedResult* CachedResultForInput(const char* url) {
    const UrlSegmentEncoder* encoder = filter_->encoder();
    ResourcePtr input_resource(CreateResource(kTestDomain, url));
    EXPECT_TRUE(input_resource.get() != NULL);
    bool use_async_flow = false;
    OutputResourcePtr output_resource(
        rewrite_driver()->CreateOutputResourceFromResource(
            kTestFilterPrefix, encoder, NULL, input_resource,
            kRewrittenResource, use_async_flow));
    EXPECT_TRUE(output_resource.get() != NULL);

    return output_resource->ReleaseCachedResult();
  }

  GoogleString in_tag_;
  GoogleString out_tag_;
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
  GoogleString in = StrCat(in_tag_, in_tag_, in_tag_);
  GoogleString out = StrCat(out_tag_, out_tag_, out_tag_);
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
  GoogleString in_tag = "<tag src=\"bad.tst\"></tag>";
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
  SetupWaitFetcher();

  // First fetch should not rewrite since resources haven't loaded yet
  ValidateNoChanges("async.not_yet", in_tag_);
  EXPECT_EQ(0, filter_->num_rewrites_called());

  // Now let it load
  CallFetcherCallbacks();

  // This time should rewrite
  ValidateExpected("async.loaded", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, CacheBad) {
  GoogleString in_tag = "<tag src=\"bad.tst\"></tag>";
  GoogleString out_tag = in_tag;
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
  GoogleString in_tag = "<tag src=\"busy.tst\"></tag>";
  GoogleString out_tag = in_tag;
  ValidateExpected("cache.busy", StrCat(in_tag, in_tag, in_tag),
                   StrCat(out_tag, out_tag, out_tag));

  EXPECT_EQ(3, filter_->num_rewrites_called());
  EXPECT_EQ(0, filter_->num_cached_results());
  EXPECT_EQ(0, filter_->num_optimizable());
}


TEST_P(RewriteSingleResourceFilterTest, Cache404) {
  // 404s should come up as unoptimizable as well.
  GoogleString in_tag = "<tag src=\"404.tst\"></tag>";
  GoogleString out_tag = in_tag;
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
  mock_timer()->AdvanceMs(TtlMs() / 2);
  ValidateExpected("initial.2", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, filter_->num_cached_results());
  EXPECT_EQ(2, filter_->num_optimizable());

  // ... but not once we get past the ttl, we will have to re-fetch the
  // input resource from the cache, which will correct the date.
  // reuse_by_content_hash is off in this run, so we must rewrite again.
  // See CacheExpireWithReuseEnabled for expiration behavior but with
  // reuse enabled.
  mock_timer()->AdvanceMs(TtlMs() * 2);
  ValidateExpected("expire", in_tag_, out_tag_);
  EXPECT_EQ(2, filter_->num_rewrites_called());
  EXPECT_EQ(3, filter_->num_cached_results());
  EXPECT_EQ(3, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, CacheExpireWithReuseEnabled) {
  filter_->set_reuse_by_content_hash(true);

  // Make sure we don't cache past the TTL.
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, filter_->num_cached_results());
  EXPECT_EQ(1, filter_->num_optimizable());

  // Everything expires out of the cache but has the same content hash,
  // so no more rewrites should be needed.
  mock_timer()->AdvanceMs(TtlMs() * 2);
  ValidateExpected("expire", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());  // no second rewrite.
  EXPECT_EQ(2, filter_->num_cached_results());
  EXPECT_EQ(2, filter_->num_optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, CacheNoFreshen) {
  // Start with non-zero time
  mock_timer()->AdvanceMs(TtlMs() / 2);
  MockResource("a.tst", "whatever", TtlSec());

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time past TTL, but re-mock the resource so it can be refetched
  mock_timer()->AdvanceMs(TtlMs() + 10);
  MockResource("a.tst", "whatever", TtlSec());
  ValidateExpected("refetch", in_tag_, out_tag_);
  EXPECT_EQ(2, filter_->num_rewrites_called());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_P(RewriteSingleResourceFilterTest, CacheNoFreshenHashCheck) {
  // Like above, but we rely on the input hash check to recover the result
  // for us.
  filter_->set_reuse_by_content_hash(true);

  // Start with non-zero time.
  mock_timer()->AdvanceMs(TtlMs() / 2);
  MockResource("a.tst", "whatever", TtlSec());

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time past TTL, but re-mock the resource so it can be refetched.
  mock_timer()->AdvanceMs(TtlMs() + 10);
  MockResource("a.tst", "whatever", TtlSec());
  ValidateExpected("refetch", in_tag_, out_tag_);

  // Here, we did re-fetch it, but did not recompute.
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_P(RewriteSingleResourceFilterTest, CacheHashCheckChange) {
  // Now the hash check should fail because the hash changed
  filter_->set_reuse_by_content_hash(true);

  // Start with non-zero time
  mock_timer()->AdvanceMs(TtlMs() / 2);
  MockResource("a.tst", "whatever", TtlSec());

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time past TTL, but re-mock the resource so it can be refetched.
  mock_timer()->AdvanceMs(TtlMs() + 10);
  SetMockHashValue("1");
  MockResource("a.tst", "whatever", TtlSec());
  // Here ComputeOutTag() != out_tag_ due to the new hasher.
  ValidateExpected("refetch", in_tag_, ComputeOutTag());

  // Since we changed the hash, this needs to recompute.
  EXPECT_EQ(2, filter_->num_rewrites_called());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}



TEST_P(RewriteSingleResourceFilterTest, CacheFreshen) {
  // Start with non-zero time
  mock_timer()->AdvanceMs(TtlMs() / 2);
  MockResource("a.tst", "whatever", TtlSec());

  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance close to TTL and rewrite, having updated the data.
  // We expect it to be freshened to that.
  mock_timer()->AdvanceMs(TtlMs() * 9 / 10);
  MockResource("a.tst", "whatever", TtlSec());
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // the 2nd fetch is freshening

  // Now advance past original TTL, but it should still be alive
  // due to freshening.
  mock_timer()->AdvanceMs(TtlMs() / 2);
  ValidateExpected("refetch", in_tag_, out_tag_);
  // we have to recompute since the rewrite cache entry has expired
  // (this behavior may change in the future)
  EXPECT_EQ(2, filter_->num_rewrites_called());
  // definitely should not have to fetch here --- freshening should have
  // done it already.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

// Make sure that fetching normal content works
TEST_P(RewriteSingleResourceFilterTest, FetchGood) {
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

// Variants of above that also test caching between fetch & rewrite paths
TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache1) {
  ValidateExpected("compute_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());

  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache2) {
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  ValidateExpected("reused_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Make sure the above also cached the timestamp
  scoped_ptr<CachedResult> cached(CachedResultForInput("a.tst"));
  ASSERT_TRUE(cached.get() != NULL);
  EXPECT_TRUE(cached->has_input_date_ms());
}

// Regression test: Fetch() should update cache version, too.
// TODO(morlovich): We want to run the entire test with different
// version, but the Google Test version we depend on is too old to have
// ::testing::WithParamInterface<int>
TEST_P(RewriteSingleResourceFilterTest, FetchFirstVersioned) {
  filter_->set_format_version(1);
  GoogleString out;
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
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("bad.tst"), &out));
  EXPECT_EQ("bad", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Make sure the above also cached the failure.
  ValidateNoChanges("postfetch.bad", "<tag src=\"bad.tst\"></tag>");
  EXPECT_EQ(1, filter_->num_rewrites_called());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

// Rewriting a 404 however propagates error
TEST_P(RewriteSingleResourceFilterTest, Fetch404) {
  GoogleString out;
  ASSERT_FALSE(ServeRelativeUrl(OutputName("404.tst"), &out));

  // Make sure the above also cached the failure.
  scoped_ptr<CachedResult> cached(CachedResultForInput("404.tst"));
  ASSERT_TRUE(cached.get() != NULL);
  EXPECT_FALSE(cached->optimizable());
}

TEST_P(RewriteSingleResourceFilterTest, FetchInvalidResourceName) {
  GoogleString out;
  ASSERT_FALSE(ServeRelativeUrl("404,.tst.pagespeed.tf.0.txt", &out));
}

TEST_P(RewriteSingleResourceFilterTest, FetchBadStatus) {
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.SetStatusAndReason(HttpStatus::kFound);
  SetFetchResponse(
      StrCat(kTestDomain, "redirect"), response_headers, StringPiece());
  SetFetchFailOnUnexpected(false);
  ValidateNoChanges("redirected_resource", "<tag src=\"/redirect\"></tag>");

  ResponseHeaders response_headers2;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers2);
  response_headers2.SetStatusAndReason(HttpStatus::kImATeapot);
  SetFetchResponse(
      StrCat(kTestDomain, "pot-1"), response_headers2, StringPiece());
  ValidateNoChanges("teapot_resource", "<tag src=\"/pot-1\"></tag>");
  // The second time, this resource will be cached with its bad status code.
  ValidateNoChanges("teapot_resource", "<tag src=\"/pot-1\"></tag>");
}

INSTANTIATE_TEST_CASE_P(RewriteSingleResourceFilterTestInstance,
                        RewriteSingleResourceFilterTest,
                        ::testing::Bool());

}  // namespace net_instaweb
