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
//
// Unit tests for ResourceCombiner and ResourceCombinerTemplate.
//
// TestCombineFilter::TestCombiner inherits off ResourceCombinerTemplate and
// provides overrides with easily testable behavior.
// TestCombineFilter is used to hook TestCombiner up with the framework.
// ResourceCombinerTest is the test fixture.

#include "net/instaweb/rewriter/public/resource_combiner.h"

#include <cstdio>

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
class CommonFilter;
class MessageHandler;
class OutputResource;
class HtmlElement;

namespace {

const char kTestCombinerId[] = "tc";
const char kTestCombinerExt[] = "tcc";
const char kTestPiece1[] = "piece1.tcc";
const char kTestPiece2[] = "piece2.tcc";
const char kTestPiece3[] = "piece3.tcc";
const char kPathPiece[]  = "path/piece.tcc";
const char kNoSuchPiece[] = "nopiece.tcc";
const char kVetoPiece[] = "veto.tcc";
const char kVetoText[] = "veto";

const char kPathCombined[] = "path,_piece.tcc+piece1.tcc";

// TestCombineFilter exists to connect up TestCombineFilter::TestCombiner with
// the normal fetch framework.
class TestCombineFilter : public RewriteFilter {
 public:
  // TestCombiner helps us test two subclass hooks:
  // 1) Preventing combinations based on content --- it vetoes resources with
  //    content equal to kVetoText
  // 2) Altering content of documents when combining --- it terminates each
  //    input's contents with a  | character.
  class TestCombiner : public ResourceCombinerTemplate<HtmlElement*> {
   public:
    explicit TestCombiner(RewriteDriver* driver, CommonFilter* filter)
        : ResourceCombinerTemplate<HtmlElement*>(driver, kTestCombinerId,
                                                 kTestCombinerExt, filter) {
    };

   protected:
    virtual bool WritePiece(const Resource* input, OutputResource* combination,
                    Writer* writer, MessageHandler* handler) {
      ResourceCombinerTemplate<HtmlElement*>::WritePiece(input, combination,
                                                         writer, handler);
      writer->Write("|", handler);
      return true;
    }

   private:
    virtual bool ResourceCombinable(Resource* resource,
                                    MessageHandler* /*handler*/) {
      EXPECT_TRUE(resource->ContentsValid());
      return resource->contents() != kVetoText;
    }
  };

  explicit TestCombineFilter(RewriteDriver* driver)
      : RewriteFilter(driver, kTestCombinerId),
        combiner_(driver, this) {
  }

  virtual void StartDocumentImpl() {
    combiner_.Reset();
  }

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "TestCombine"; }
  virtual bool Fetch(const OutputResourcePtr& resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback) {
    return combiner_.Fetch(resource, writer, request_header, response_headers,
                           message_handler, callback);
  }
  TestCombineFilter::TestCombiner* combiner() { return &combiner_; }
 private:
  TestCombineFilter::TestCombiner combiner_;
};

// Test fixture.
class ResourceCombinerTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    filter_ = new TestCombineFilter(&rewrite_driver_);
    AddRewriteFilter(filter_);
    AddOtherRewriteFilter(new TestCombineFilter(&other_rewrite_driver_));

    // Make sure to set the domain so we authorize fetches.
    SetBaseUrlForFetch(kTestDomain);

    MockResource(kTestPiece1, "piece1", 10000);
    MockResource(kTestPiece2, "piec2", 20000);
    MockResource(kTestPiece3, "pie3", 30000);
    MockResource(kPathPiece, "path", 30000);
    MockResource(kVetoPiece, kVetoText, 30000);
    MockMissingResource(kNoSuchPiece);

    partnership_ = filter_->combiner();
  }

  GoogleString AbsoluteUrl(const char* relative) {
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
  bool FetchResource(const StringPiece& url, GoogleString* content, int flags) {
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
    MockCallback callback;
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &callback);

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

    EXPECT_LE(partnership_->ResolvedBase().length() +
                  LeafLength(partnership_->UrlSafeId().length()),
              options_.max_url_size() - UrlSlack());
  }

  int UrlSlack() const {
    return ResourceCombiner::kUrlSlack;
  }

  HtmlElement* TestElement() {
    return rewrite_driver_.NewElement(NULL, "test");
  }

  GoogleString StringOfLength(int n, char fill) {
    GoogleString out;
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

  bool AddElement(HtmlElement* element, const StringPiece& url,
                  MessageHandler* handler) {
    return partnership_->AddElement(element, url, handler).value;
  }

  TestCombineFilter* filter_;  // owned by the rewrite_driver_.
  TestCombineFilter::TestCombiner* partnership_;  // owned by the filter_
};

TEST_F(ResourceCombinerTest, TestPartnershipBasic) {
  // Make sure we're actually combining names and filling in the
  // data arrays if everything is available.

  HtmlElement* e1 = TestElement();
  HtmlElement* e2 = TestElement();
  HtmlElement* e3 = TestElement();

  printf("driver base url is %s\n", rewrite_driver_.base_url().spec_c_str());

  EXPECT_EQ(0, partnership_->num_urls());
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  EXPECT_EQ(1, partnership_->num_urls());
  EXPECT_TRUE(AddElement(e2, kTestPiece2, &message_handler_));
  EXPECT_EQ(2, partnership_->num_urls());
  EXPECT_TRUE(AddElement(e3, kTestPiece3, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc+piece3.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(3);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
  VerifyResource(2, kTestPiece3, e3);
}

TEST_F(ResourceCombinerTest, TestIncomplete1) {
  // Test with the first URL incomplete - nothing should get added
  HtmlElement* e1 = TestElement();
  EXPECT_FALSE(AddElement(e1, kNoSuchPiece, &message_handler_));
  VerifyUrlCount(0);
}

TEST_F(ResourceCombinerTest, TestIncomplete2) {
  // Test with the second URL incomplete. Should include the first one.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(AddElement(e2, kNoSuchPiece, &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1, e1);
}

TEST_F(ResourceCombinerTest, TestIncomplete3) {
  // Now with the third one incomplete. Two should be in the partnership
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_FALSE(AddElement(e3, kNoSuchPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
}

TEST_F(ResourceCombinerTest, TestRemove) {
  // Add one element, remove it, and then re-add a few.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1, e1);

  partnership_->RemoveLastElement();
  VerifyUrlCount(0);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_TRUE(AddElement(e3, kTestPiece3, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece2, e2);
  VerifyResource(1, kTestPiece3, e3);
  EXPECT_EQ("piece2.tcc+piece3.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestRemoveFrom3) {
  // Add three elements, remove 1
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_TRUE(AddElement(e3, kTestPiece3, &message_handler_));

  VerifyUrlCount(3);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
  VerifyResource(2, kTestPiece3, e3);
  EXPECT_EQ("piece1.tcc+piece2.tcc+piece3.tcc", partnership_->UrlSafeId());

  partnership_->RemoveLastElement();
  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestAddBroken) {
  // Test with the second URL broken enough for CreateInputResource to fail
  // (due to unknown protocol). In that case, we should just include the first
  // URL in the combination.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(
    AddElement(e2, "slwy://example.com/", &message_handler_));
  EXPECT_EQ(kTestPiece1, partnership_->UrlSafeId());

  VerifyUrlCount(1);
  VerifyResource(0, kTestPiece1, e1);
}

TEST_F(ResourceCombinerTest, TestVeto) {
  // Make sure a vetoed element stops the combination
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kTestPiece1, &message_handler_));
  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece2, &message_handler_));
  HtmlElement* e3 = TestElement();
  EXPECT_FALSE(AddElement(e3, kVetoPiece, &message_handler_));
  EXPECT_EQ("piece1.tcc+piece2.tcc", partnership_->UrlSafeId());

  VerifyUrlCount(2);
  VerifyResource(0, kTestPiece1, e1);
  VerifyResource(1, kTestPiece2, e2);
}

TEST_F(ResourceCombinerTest, TestRebase) {
  // A very basic test for re-resolving fragment when base changes
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece1, &message_handler_));
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());
}

TEST_F(ResourceCombinerTest, TestRebaseRemove) {
  // Here the first item we add is: path/piece.tcc, while the second one
  // is piece1.tcc. This means after the two items our state should be
  // roughly 'path/piece.tcc and piece1.tcc in /', while after backing out
  // the last one it should be 'piece.tcc in path/'. This tests makes
  // sure we do this.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece1, &message_handler_));
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());

  partnership_->RemoveLastElement();
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyResource(0, kPathPiece, e1);
}

TEST_F(ResourceCombinerTest, TestRebaseRemoveAdd) {
  // As above, but also add in an additional entry to see that handling of
  // different paths still works.
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece1, &message_handler_));

  partnership_->RemoveLastElement();
  VerifyUrlCount(1);
  EXPECT_EQ(StrCat(kTestDomain, "path/"), partnership_->ResolvedBase());
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e3 = TestElement();
  EXPECT_TRUE(AddElement(e3, kTestPiece2, &message_handler_));
  VerifyUrlCount(2);
  EXPECT_EQ("path,_piece.tcc+piece2.tcc", partnership_->UrlSafeId());
  EXPECT_EQ(kTestDomain, partnership_->ResolvedBase());
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece2, e3);
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow) {
  // Test to make sure that we notice when we go over the limit when
  // we rebase - we lower the segment size limit just for that.
  options_.set_max_url_segment_size(LeafLength(strlen(kPathCombined) - 1) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();

  // Note that we want the base to be reverted to the previous one.
  // otherwise, we may still end up overflowed even without the new segment
  // included, just due to path addition
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow2) {
  // Test to make sure we are exact in our size limit
  options_.set_max_url_segment_size(LeafLength(strlen(kPathCombined)) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
  EXPECT_EQ(kPathCombined, partnership_->UrlSafeId());
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestRebaseOverflow3) {
  // Make sure that if we add url, rebase, and then rollback we
  // don't end up overlimit due to the first piece expanding
  options_.set_max_url_segment_size(LeafLength(strlen("piece.tcc")) +
                                    UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestMaxUrlOverflow) {
  // Make sure we don't produce URLs bigger than the max_url_size().
  options_.set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack() - 1);
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_FALSE(AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestMaxUrlOverflow2) {
  // This one is just right
  options_.set_max_url_size(
      strlen(kTestDomain) + LeafLength(strlen(kPathCombined)) + UrlSlack());
  HtmlElement* e1 = TestElement();
  EXPECT_TRUE(AddElement(e1, kPathPiece, &message_handler_));
  EXPECT_EQ("piece.tcc", partnership_->UrlSafeId());
  VerifyUrlCount(1);
  VerifyResource(0, kPathPiece, e1);

  HtmlElement* e2 = TestElement();
  EXPECT_TRUE(AddElement(e2, kTestPiece1, &message_handler_));
  VerifyUrlCount(2);
  VerifyResource(0, kPathPiece, e1);
  VerifyResource(1, kTestPiece1, e2);
  VerifyLengthLimits();
}

TEST_F(ResourceCombinerTest, TestFetch) {
  // Test if we can reconstruct from pieces.
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+piece2.tcc+piece3.tcc", "txt");

  GoogleString out;
  EXPECT_TRUE(FetchResource(url, &out, kFetchNormal));
  EXPECT_EQ("piece1|piec2|pie3|", out);
}

TEST_F(ResourceCombinerTest, TestFetchAsync) {
  // Test if we can reconstruct from pieces, with callback happening async
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+piece2.tcc+piece3.tcc", "txt");
  GoogleString out;
  EXPECT_TRUE(FetchResource(url, &out, kFetchAsync));
  EXPECT_EQ("piece1|piec2|pie3|", out);
}

TEST_F(ResourceCombinerTest, TestFetchFail) {
  // Test if we can handle failure properly
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");

  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(ResourceCombinerTest, TestFetchFail2) {
  mock_url_fetcher_.set_fail_on_unexpected(false);
  // This is slightly different from above, as we get a complete
  // fetch failure rather than a 404.
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+weird.tcc+piece2.tcc", "txt");

  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(ResourceCombinerTest, TestFetchFailAsync) {
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");

  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(ResourceCombinerTest, TestFetchFailAsync2) {
  mock_url_fetcher_.set_fail_on_unexpected(false);
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+weird.tcc+piece2.tcc", "txt");

  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(ResourceCombinerTest, TestFetchFailSevere) {
  // Test the case where we can't even create resources (wrong protocol)
  GoogleString url = Encode("slwy://example.com/", kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");
  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchNormal));
}

TEST_F(ResourceCombinerTest, TestFetchFailSevereAsync) {
  GoogleString url = Encode("slwy://example.com/", kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");
  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(ResourceCombinerTest, TestFetchNonsense) {
  // Make sure we handle URL decoding failing OK
  GoogleString url = StrCat(kTestDomain, "piece1.tcc+nopiece.tcc,.pagespeeed.",
                            kTestCombinerId, ".0.txt");
  GoogleString out;
  EXPECT_FALSE(FetchResource(url, &out, kFetchAsync));
}

TEST_F(ResourceCombinerTest, TestContinuingFetchWhenFastFailed) {
  // We may quickly detect that a piece isn't fetchable (with fetch failure
  // cached) after we have already initiated a fetch for an another resource.
  // In that case, we need to be careful to make sure we don't try to write
  // the headers anyway, as their lifetime can't be guaranteed if ::Fetch
  // returned false.
  WaitUrlAsyncFetcher simulate_async(&mock_url_fetcher_);
  rewrite_driver_.set_async_fetcher(&simulate_async);
  resource_manager_->set_url_async_fetcher(&simulate_async);

  // Seed our cache with the fact that nopiece.tcc isn't there.
  ResourcePtr missing(
      rewrite_driver_.CreateInputResourceAbsoluteUnchecked(
          StrCat(kTestDomain, "nopiece.tcc")));
  ASSERT_TRUE(missing.get() != NULL);
  EXPECT_FALSE(rewrite_driver_.ReadIfCached(missing));
  simulate_async.CallCallbacks();

  // Now try to fetch a combination with 3 pieces.
  // The first one will start loading, on the second, nopiece.tcc,
  // we will quickly notice failure, and on third one we will
  // notice we've failed already.
  GoogleString url = Encode(kTestDomain, kTestCombinerId, "0",
                            "piece1.tcc+nopiece.tcc+piece2.tcc", "txt");
  GoogleString content;
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  StringWriter writer(&content);
  MockCallback callback;
  bool called = rewrite_driver_.FetchResource(url, request_headers,
                                              &response_headers, &writer,
                                              &callback);
  EXPECT_TRUE(called);  // RewriteDriver took care of it after filter failure.
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_FALSE(response_headers.headers_complete());

  // Now finish loading of the initialized piece1.tcc fetch...
  simulate_async.CallCallbacks();
  EXPECT_FALSE(response_headers.headers_complete())
      << "Writing to headers which might be dead. Can cause crashes!";
}

}  // namespace

}  // namespace net_instaweb
