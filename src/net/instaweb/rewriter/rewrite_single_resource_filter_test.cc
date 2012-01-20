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
//
// This contains tests for a basic fake filter that rewrites a single resource,
// making sure the various caching and invalidation mechanisms work.

#include "net/instaweb/rewriter/public/rewrite_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
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
class ResourceContext;
class RewriteContext;

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

// A simple RewriteFilter subclass that rewrites
// <tag src=...> and keeps some statistics.
//
// It rewrites resources as follows:
// 1) If original contents are equal to bad, it fails the rewrite
// 2) If the contents are a $ sign, it claims the system is too busy
// 3) otherwise it repeats the contents twice.
class TestRewriter : public RewriteFilter {
 public:
  TestRewriter(RewriteDriver* driver, bool create_custom_encoder)
      : RewriteFilter(driver),
        num_rewrites_called_(0),
        create_custom_encoder_(create_custom_encoder) {
  }

  virtual ~TestRewriter() {
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}

  virtual void EndElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kTag) {
      HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
      if (src != NULL) {
        ResourcePtr resource = CreateInputResource(src->value());
        if (resource.get() != NULL) {
          ResourceSlotPtr slot(driver_->GetSlot(resource, element, src));
          Context* context = new Context(driver_, this);
          context->AddSlot(slot);
          driver_->InitiateRewrite(context);
        }
      }
    }
  }

  virtual const char* Name() const { return "TestRewriter"; }
  virtual const char* id() const { return kTestFilterPrefix; }

  // Number of times RewriteLoadedResource got called
  int num_rewrites_called() const { return num_rewrites_called_; }

  // Do we use a custom encoder (which prepends kTestEncoderUrlExtra?)
  bool create_custom_encoder() const { return create_custom_encoder_; }

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
        driver_->message_handler());
    return ok ? kRewriteOk : kRewriteFailed;
  }

  virtual const UrlSegmentEncoder* encoder() const {
    if (create_custom_encoder_) {
      return &test_url_encoder_;
    } else {
      return driver_->default_encoder();
    }
  }

  class Context : public SingleRewriteContext {
   public:
    Context(RewriteDriver* driver, TestRewriter* rewriter)
        : SingleRewriteContext(driver, NULL, NULL),
          filter_(rewriter) {
    }
    virtual ~Context() {
    }
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      RewriteDone(filter_->RewriteLoadedResource(input, output), 0);
    }

    virtual OutputResourceKind kind() const { return kRewrittenResource; }
    virtual const char* id() const { return filter_->id(); }
    virtual const UrlSegmentEncoder* encoder() const {
      return filter_->encoder();
    }

   private:
    TestRewriter* filter_;
  };

  virtual RewriteContext* MakeRewriteContext() {
    return new Context(driver_, this);
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

  int format_version_;
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
    options()->ComputeSignature(hasher());

    MockResource("a.tst", "good", TtlSec());
    MockResource("bad.tst", "bad", TtlSec());
    MockResource("busy.tst", "$", TtlSec());
    MockMissingResource("404.tst");

    in_tag_ = "<tag src=\"a.tst\"></tag>";
    out_tag_ = ComputeOutTag();
  }

  GoogleString ComputeOutTag() {
    return StrCat("<tag src=\"", OutputName(kTestDomain, "a.tst"), "\"></tag>");
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
  GoogleString OutputName(const StringPiece& in_domain,
                          const StringPiece& in_name) {
    return Encode(in_domain, kTestFilterPrefix, hasher()->Hash(""),
                  in_name, "txt");
  }

  // Serves from relative URL
  bool ServeRelativeUrl(const StringPiece& rel_path, GoogleString* content) {
    return ServeResourceUrl(StrCat(kTestDomain, rel_path), content);
  }

  void ResetSignature(int outline_min_bytes) {
    options()->ClearSignatureForTesting();
    options()->set_css_outline_min_bytes(outline_min_bytes);
    resource_manager_->ComputeSignature(options());
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
}

TEST_P(RewriteSingleResourceFilterTest, VersionChange) {
  options()->ClearSignatureForTesting();
  const int kOrigOutlineMinBytes = 1234;
  ResetSignature(kOrigOutlineMinBytes);

  GoogleString in = StrCat(in_tag_, in_tag_, in_tag_);
  GoogleString out = StrCat(out_tag_, out_tag_, out_tag_);
  ValidateExpected("vc1", in, out);

  // Should only have to rewrite once here
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // The next attempt should still use cache
  ValidateExpected("vc2", in, out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Change the rewrite options -- this won't affect the actual
  // result but will result in an effective cache flush.
  ResetSignature(kOrigOutlineMinBytes + 1);

  ValidateExpected("vc3", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // And now we're caching again
  ValidateExpected("vc4", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // Restore. The old meta-data cache entries can be re-used.
  ResetSignature(kOrigOutlineMinBytes);
  ValidateExpected("vc5", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // And now we're caching again
  ValidateExpected("vc6", in, out);
  EXPECT_EQ(2, filter_->num_rewrites_called());
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
  ResetSignature(42);
  ValidateNoChanges("vc.bad3", in_tag);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // And now cached again
  ValidateNoChanges("vc.bad4", in_tag);
  EXPECT_EQ(2, filter_->num_rewrites_called());

  // downgrade -- retried.
  ResetSignature(21);
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
}

TEST_P(RewriteSingleResourceFilterTest, CacheBusy) {
  // In case of busy, it should keep trying every time,
  // as it's meant to represent intermitent system load and
  // not a conclusion about the resource.
  GoogleString in_tag = "<tag src=\"busy.tst\"></tag>";
  GoogleString out_tag = in_tag;
  ValidateExpected("cache.busy", StrCat(in_tag, in_tag, in_tag),
                   StrCat(out_tag, out_tag, out_tag));

  EXPECT_EQ(1, filter_->num_rewrites_called());
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
}

TEST_P(RewriteSingleResourceFilterTest, InvalidUrl) {
  // Make sure we don't have problems with bad URLs.
  ValidateNoChanges("bad_url", "<tag src=\"http://evil.com\"></tag>");
}

TEST_P(RewriteSingleResourceFilterTest, CacheExpire) {
  // Make sure we don't cache past the TTL.
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Next fetch should be still in there.
  mock_timer()->AdvanceMs(TtlMs() / 2);
  ValidateExpected("initial.2", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // ... but not once we get past the ttl, we will have to re-fetch the
  // input resource from the cache, which will correct the date.
  // reuse_by_content_hash is off in this run, so we must rewrite again.
  // See CacheExpireWithReuseEnabled for expiration behavior but with
  // reuse enabled.
  mock_timer()->AdvanceMs(TtlMs() * 2);
  ValidateExpected("expire", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, CacheExpireWithReuseEnabled) {
  // Make sure we don't cache past the TTL.
  ValidateExpected("initial", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  // Everything expires out of the cache but has the same content hash,
  // so no more rewrites should be needed.
  mock_timer()->AdvanceMs(TtlMs() * 2);
  ValidateExpected("expire", in_tag_, out_tag_);
  EXPECT_EQ(1, filter_->num_rewrites_called());  // no second rewrite.
}

// Make sure that fetching normal content works
TEST_P(RewriteSingleResourceFilterTest, FetchGood) {
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("", "a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

// Variants of above that also test caching between fetch & rewrite paths
TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache1) {
  ValidateExpected("compute_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());

  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("", "a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

TEST_P(RewriteSingleResourceFilterTest, FetchGoodCache2) {
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("", "a.tst"), &out));
  EXPECT_EQ("goodgood", out);
  EXPECT_EQ(1, filter_->num_rewrites_called());

  ValidateExpected("reused_cached", StrCat(in_tag_, in_tag_, in_tag_),
                   StrCat(out_tag_, out_tag_, out_tag_));
  EXPECT_EQ(1, filter_->num_rewrites_called());
}

// In the old RewriteSingleResourceFilter cache versioning machinery
// there used to be a bug where first Fetches didn't update
// cache correctly for further rewrites. The relevant code no
// longer exists, but the test is retained as simple exercise of caching
// on fetch.
TEST_P(RewriteSingleResourceFilterTest, FetchFirstVersioned) {
  GoogleString out;
  ASSERT_TRUE(ServeRelativeUrl(OutputName("", "a.tst"), &out));
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
  ASSERT_TRUE(ServeRelativeUrl(OutputName("", "bad.tst"), &out));
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
  ASSERT_FALSE(ServeRelativeUrl(OutputName("", "404.tst"), &out));
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
