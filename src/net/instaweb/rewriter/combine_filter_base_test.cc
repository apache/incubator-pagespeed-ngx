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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/rewriter/public/combine_filter_base.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

namespace {

const char kTestCombinerId[] = "tc";
const char kTestPiece1[] = "piece1.tcc";
const char kTestPiece2[] = "piece2.tcc";
const char kTestPiece3[] = "piece3.tcc";
const char kPathPiece[]  = "path/piece.tcc";
const char kNoSuchPiece[] = "nopiece.tcc";
const char kVetoPiece[] = "veto.tcc";
const char kVetoText[] = "veto";

const char kPathCombined[] = "path,_piece.tcc+piece1.tcc";

// The test filter subclass exists to help us test the
// two subclass hooks:
// 1) Preventing combinations based on content
// 2) Altering content of documents when combining
class TestCombineFilter : public CombineFilterBase {
 public:
  // The partnership subclass vetoes resources with content equal to
  // kVetoText
  class Partnership : public CombineFilterBase::Partnership {
   public:
    Partnership(TestCombineFilter* filter, RewriteDriver* driver,
                int url_overhead)
        : CombineFilterBase::Partnership(filter, driver, url_overhead) {
    }

   private:
    bool ResourceCombinable(Resource* resource, MessageHandler* /*handler*/) {
      EXPECT_TRUE(resource->ContentsValid());
      return resource->contents() != kVetoText;
    }
  };

  explicit TestCombineFilter(RewriteDriver* driver)
      : CombineFilterBase(driver, kTestCombinerId, "tcc") {
    InitPartnership();
  }

  virtual void StartDocumentImpl() {
    InitPartnership();
  }

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "TestCombine"; }

  Partnership* partnership() { return partnership_.get(); }
  void InitPartnership() {
    partnership_.reset(new Partnership(this, driver_, url_overhead_));
  }

 protected:
  bool WritePiece(Resource* input, OutputResource* combination,
                  Writer* writer, MessageHandler* handler) {
    CombineFilterBase::WritePiece(input, combination, writer, handler);
    writer->Write("|", handler);
    return true;
  }

 private:
  scoped_ptr<Partnership> partnership_;
};

class CombineFilterBaseTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    filter_ = new TestCombineFilter(&rewrite_driver_);
    AddRewriteFilter(filter_);
    AddOtherRewriteFilter(new TestCombineFilter(&other_rewrite_driver_));

    // For all of these, we do not actually use the parser output,
    // so we just feed an empty page
    ParseUrl(kTestDomain, "");

    MockResource(kTestPiece1, "piece1", 10000);
    MockResource(kTestPiece2, "piec2", 20000);
    MockResource(kTestPiece3, "pie3", 30000);
    MockResource(kPathPiece, "path", 30000);
    MockResource(kVetoPiece, kVetoText, 30000);
    MockMissingResource(kNoSuchPiece);

    s_test_ = html_parse()->Intern("test");
    partnership_ = filter_->partnership();
  }

  std::string AbsoluteUrl(const char* relative) {
    return StrCat(kTestDomain, relative);
  }

  // Create a resource with given data and TTL
  void MockResource(const char* rel_path, const StringPiece& data, int64 ttl) {
    InitResponseHeaders(rel_path, kContentTypeText, data, ttl);
  }

  // Creates a resource that 404s
  void MockMissingResource(const char* rel_path) {
    ResponseHeaders response_headers;
    resource_manager_->SetDefaultHeaders(&kContentTypeText, &response_headers);
    response_headers.SetStatusAndReason(HttpStatus::kNotFound);
    mock_url_fetcher_.SetResponse(AbsoluteUrl(rel_path), response_headers,
                                  StringPiece());
  }

  enum FetchFlags {
    kFetchNormal = 0,
    kFetchAsync  = 1,
  };

  // Fetches a resource, optionally permitting asynchronous loading (delayed
  // invocation and fetches that may fail. Returns whether succeeded
  bool FetchResource(const StringPiece& url, std::string* content, int flags) {
    WaitUrlAsyncFetcher simulate_async(&mock_url_fetcher_);
    if (flags & kFetchAsync) {
      rewrite_driver_.set_async_fetcher(&simulate_async);
      resource_manager_->set_url_async_fetcher(&simulate_async);
    }

    // TODO(morlovich): This is basically copy-paste from ServeResourceUrl.
    content->clear();
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    StringWriter writer(content);
    FetchCallback callback;
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &message_handler_,
        &callback);

    if (!fetched) {
      return false;
    }

    if (flags & kFetchAsync) {
      simulate_async.CallCallbacks();
    }

    EXPECT_TRUE(callback.done());
    return callback.success();
  }

  // Makes sure that the resource at given position in the partnership_
  // is valid and matches expected URL and element
  void VerifyResource(int pos, const char* url, HtmlElement* element) {
    EXPECT_EQ(element, partnership_->element(pos));
    EXPECT_TRUE(partnership_->resources()[pos]->ContentsValid());
    EXPECT_EQ(AbsoluteUrl(url), partnership_->resources()[pos]->url());
  }

  // Check that we have the expected number of things in the partnership
  void VerifyUrlCount(int num_expected) {
    ASSERT_EQ(num_expected, partnership_->num_urls());
    ASSERT_EQ(num_expected, partnership_->resources().size());
  }

  // Check to make sure we are within various URL limits
  void VerifyLengthLimits() {
    EXPECT_LE(LeafLength(partnership_->UrlSafeId().length()),
              options_.max_url_segment_size() - UrlSlack());

    LOG(ERROR) << partnership_->ResolvedBase();
    EXPECT_LE(partnership_->ResolvedBase().length() +
                  LeafLength(partnership_->UrlSafeId().length()),
              options_.max_url_size() - UrlSlack());
  }

  int UrlSlack() const {
    return CombineFilterBase::kUrlSlack;
  }

  HtmlElement* TestElement() {
    return html_parse()->NewElement(NULL, s_test_);
  }

  std::string StringOfLength(int n, char fill) {
    std::string out;
    out.insert(0, n, fill);
    return out;
  }

  // Returns the number of characters in the leaf file name given the
  // resource name, counting what will be spent on the hash, id, etc.
  int LeafLength(int resource_len) {
    ResourceNamer namer;
    namer.set_hash(
        StringOfLength(resource_manager_->hasher()->HashSizeInChars(), '#'));
    namer.set_name(StringOfLength(resource_len, 'P'));
    namer.set_id(kTestCombinerId);
    namer.set_ext("tcc");
    return namer.Encode().length();
  }

  TestCombineFilter* filter_;  // owned by the rewrite_driver_.
  TestCombineFilter::Partnership* partnership_;
  Atom s_test_;
};

TEST_F(CombineFilterBaseTest, TestPartnershipBasic) {
  // Make sure we're actually combining names and filling in the
  // data arrays if everything is available.

  HtmlElement* e1 = TestElement();
  HtmlElement* e2 = TestElement();
  HtmlElement* e3 = TestElement();

  EXPECT_EQ(0, partnership_->num_urls());
  EXPECT_TRUE(partnership_->AddElement(e1, kTestPiece1, &message_handler_));
  EXPECT_EQ(1, partnership_->num_urls());
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece2, &message_handler_));
  EXPECT_EQ(2, partnership_->num_urls());
  EXPECT_TRUE(partnership_->AddElement(e3, kTestPiece3, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc+piece3.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(3);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
  VerifyResource(2, kTestPiece3, e3);
}

TEST_F(CombineFilterBaseTest, TestIncomplete1) {
  // Test with the first URL incomplete - nothing should get added
  HtmlElement* e1 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e1, kNoSuchPiece, &message_handler_));
  VerifyUrlCount(0);
}

TEST_F(CombineFilterBaseTest, TestIncomplete2) {
  // Test with the second URL incomplete. Should include the first one.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e2, kNoSuchPiece, &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1, e1);
}

TEST_F(CombineFilterBaseTest, TestIncomplete3) {
  // Now with the third one incomplete. Two should be in the partnership
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e3, kNoSuchPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
}

TEST_F(CombineFilterBaseTest, TestAddBroken) {
  // Test with the second URL broken enough for CreateInputResource to fail
  // (due to unknown protocol). In that case, we should just include the first
  // URL in the combination.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(
    partnership_->AddElement(e2, "slwy://example.com/", &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1, e1);
}

TEST_F(CombineFilterBaseTest, TestVeto) {
  // Make sure a vetoed element stops the combination
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e3, kVetoPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
}

TEST_F(CombineFilterBaseTest, TestRebase) {
  // A very basic test for re-resolving fragment when base changes
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
}

TEST_F(CombineFilterBaseTest, TestRebaseOverflow) {
  // Test to make sure that we notice when we go over the limit when
  // we rebase - we lower the segment size limit just for that.
  options_.set_max_url_segment_size(LeafLength(strlen(kPathCombined) - 1) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();

  // Note that we want the base to be reverted to the previous one.
  // otherwise, we may still end up overflowed even without the new segment
  // included, just due to path addition
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
}

TEST_F(CombineFilterBaseTest, TestRebaseOverflow2) {
  // Test to make sure we are exact in our size limit
  options_.set_max_url_segment_size(LeafLength(strlen(kPathCombined)) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyLengthLimits();
}

TEST_F(CombineFilterBaseTest, TestRebaseOverflow3) {
  // Make sure that if we add url, rebase, and then rollback we
  // don't end up overlimit due to the first piece expanding
  options_.set_max_url_segment_size(LeafLength(strlen("piece.tcc")) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();
}

TEST_F(CombineFilterBaseTest, TestMaxUrlOverflow) {
  // Make sure we don't produce URLs bigger than the max_url_size().
  options_.set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack() - 1);
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();
}

TEST_F(CombineFilterBaseTest, TestMaxUrlOverflow2) {
  // This one is just right
  options_.set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(partnership_->AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
  VerifyLengthLimits();
}

TEST_F(CombineFilterBaseTest, TestFetch) {
  // Test if we can reconstruct from pieces.
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+piece2.tcc+piece3.tcc", "txt");

  std::string out;
  EXPECT_TRUE(FetchResource(url, &out, kFetchNormal));
  EXPECT_EQ("piece1|piec2|pie3|", out);
}

TEST_F(CombineFilterBaseTest, TestFetchAsync) {
  // Test if we can reconstruct from pieces, with callback happening async
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+piece2.tcc+piece3.tcc", "txt");
  std::string out;
  EXPECT_TRUE(FetchResource(url, &out, kFetchAsync));
  EXPECT_EQ("piece1|piec2|pie3|", out);
}

TEST_F(CombineFilterBaseTest, TestFetchFail) {
  // Test if we can handle failure properly
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");

  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(CombineFilterBaseTest, TestFetchFail2) {
  mock_url_fetcher_.set_fail_on_unexpected(false);
  // This is slightly different from above, as we get a complete
  // fetch failure rather than a 404.
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+weird.tcc+piece2.tcc", "txt");

  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(CombineFilterBaseTest, TestFetchFailAsync) {
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");

  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(CombineFilterBaseTest, TestFetchFailAsync2) {
  mock_url_fetcher_.set_fail_on_unexpected(false);
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+weird.tcc+piece2.tcc", "txt");

  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(CombineFilterBaseTest, TestFetchFailSevere) {
  // Test the case where we can't even create resources (wrong protocol)
  std::string url = Encode("slwy://example.com/", kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");
  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(CombineFilterBaseTest, TestFetchFailSevereAsync) {
  std::string url = Encode("slwy://example.com/", kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");
  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(CombineFilterBaseTest, TestFetchNonsense) {
  // Make sure we handle URL decoding failing OK
  std::string url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc,", "txt");
  std::string out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

}  // namespace

}  // namespace net_instaweb
