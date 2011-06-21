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

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include <vector>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr, etc
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"  // for GoogleUrl
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace {

const char kTrimWhitespaceFilterId[] = "tw";
const char kUpperCaseFilterId[] = "uc";
const char kNestedFilterId[] = "nf";
const char kCombiningFilterId[] = "cr";


}  // namespace

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class UrlSegmentEncoder;
class Writer;

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

// Filter that contains nested resources that must themselves
// be rewritten.
class NestedFilter : public RewriteFilter {
 public:
  explicit NestedFilter(RewriteDriver* driver)
      : RewriteFilter(driver, kNestedFilterId), chain_(false) {
    ClearStats();
  }

  // Stats
  int num_top_rewrites() const { return num_top_rewrites_; }
  int num_sub_rewrites() const { return num_sub_rewrites_; }

  void ClearStats() {
    num_top_rewrites_ = 0;
    num_sub_rewrites_ = 0;
  }

  // Set this to true to create a chain of nested rewrites on the same slot.
  void set_chain(bool x) { chain_ = x; }

 protected:
  virtual ~NestedFilter() {}

  class NestedSlot : public ResourceSlot {
   public:
    explicit NestedSlot(const ResourcePtr& resource) : ResourceSlot(resource) {}
    virtual void Render() {}
  };

  class NestedContext : public SingleRewriteContext {
   public:
    NestedContext(RewriteContext* parent, NestedFilter* filter)
        : SingleRewriteContext(NULL, parent, NULL),
          filter_(filter) {}
    virtual ~NestedContext() {}

   protected:
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      ++filter_->num_sub_rewrites_;
      GoogleString text = input->contents().as_string();
      UpperString(&text);
      RewriteSingleResourceFilter::RewriteResult result =
          RewriteSingleResourceFilter::kRewriteFailed;
      if (input->contents() != text) {
        int64 origin_expire_time_ms = input->CacheExpirationTimeMs();
        ResourceManager* resource_manager = Manager();
        MessageHandler* message_handler = resource_manager->message_handler();
        if (resource_manager->Write(HttpStatus::kOK, text, output.get(),
                                    origin_expire_time_ms, message_handler)) {
          result = RewriteSingleResourceFilter::kRewriteOk;
        }
      }
      RewriteDone(result, 0);
    }

    virtual const char* id() const { return kNestedFilterId; }
    virtual OutputResourceKind kind() const { return kOnTheFlyResource; }

    NestedFilter* filter_;
  };

  class Context : public SingleRewriteContext {
   public:
    Context(RewriteDriver* driver, NestedFilter* filter, bool chain)
        : SingleRewriteContext(driver, NULL, NULL),
          filter_(filter),
          chain_(chain) {
    }
    virtual ~Context() {
      STLDeleteElements(&strings_);
    }
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      ++filter_->num_top_rewrites_;
      output_ = output;
      // Assume that this file just has nested CSS URLs one per line,
      // which we will rewrite.
      StringPieceVector pieces;
      SplitStringPieceToVector(input->contents(), "\n", &pieces, true);

      GoogleUrl base(input->url());
      if (base.is_valid()) {
        // Add a new nested multi-slot context.
        for (int i = 0, n = pieces.size(); i < n; ++i) {
          GoogleUrl url(base, pieces[i]);
          if (url.is_valid()) {
            ResourcePtr resource(Driver()->CreateInputResource(url));
            if (resource.get() != NULL) {
              NestedContext* nested_context = new NestedContext(this, filter_);
              AddNestedContext(nested_context);
              ResourceSlotPtr slot(new NestedSlot(resource));
              nested_context->AddSlot(slot);

              // Test chaining of a 2nd rewrite on the same slot, if asked.
              if (chain_) {
                NestedContext* nested_context2 =
                    new NestedContext(this, filter_);
                AddNestedContext(nested_context2);
                nested_context2->AddSlot(slot);
              }
            }
          }
        }
        // TODO(jmarantz): start this automatically.  This will be easier
        // to do once the states are kept more explicitly via a refactor.
        StartNestedTasks();
      }
    }

    virtual void Harvest() {
      RewriteSingleResourceFilter::RewriteResult result =
          RewriteSingleResourceFilter::kRewriteFailed;
      GoogleString new_content;

      // TODO(jmarantz): Make RewriteContext handle the aggregation of
      // of expiration times.
      int64 min_expire_ms = 0;
      CHECK_EQ(1, num_slots());
      for (int i = 0, n = num_nested(); i < n; ++i) {
        CHECK_EQ(1, nested(i)->num_slots());
        ResourceSlotPtr slot(nested(i)->slot(0));
        ResourcePtr resource(slot->resource());
        int64 expire_ms = resource->CacheExpirationTimeMs();
        if ((i == 0) || (expire_ms < min_expire_ms)) {
          min_expire_ms = expire_ms;
        }
        StrAppend(&new_content, resource->url(), "\n");
      }
      ResourceManager* resource_manager = Manager();
      MessageHandler* message_handler = resource_manager->message_handler();
      if (resource_manager->Write(HttpStatus::kOK, new_content, output(0).get(),
                                  min_expire_ms, message_handler)) {
        result = RewriteSingleResourceFilter::kRewriteOk;
      }
      RewriteDone(result, 0);
    }

   protected:
    virtual const char* id() const { return kNestedFilterId; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    OutputResourcePtr output_;
    std::vector<GoogleString*> strings_;
    NestedFilter* filter_;
    bool chain_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  bool Fetch(const OutputResourcePtr& output_resource,
             Writer* response_writer,
             const RequestHeaders& request_header,
             ResponseHeaders* response_headers,
             MessageHandler* message_handler,
             UrlAsyncFetcher::Callback* callback) {
    CHECK(false);
  }

  RewriteContext* MakeRewriteContext() {
    return new Context(driver_, this, chain_);
  }

  void StartElementImpl(HtmlElement* element) {
    HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kHref);
    if (attr != NULL) {
      ResourcePtr resource = CreateInputResource(attr->value());
      if (resource.get() != NULL) {
        ResourceSlotPtr slot(driver_->GetSlot(resource, element, attr));

        // This 'new' is paired with a delete in RewriteContext::FinishFetch()
        Context* context = new Context(driver_, this, chain_);
        context->AddSlot(slot);
        driver_->InitiateRewrite(context);
      }
    }
  }

  virtual OutputResourceKind kind() const { return kind_; }
  virtual const char* id() const { return kNestedFilterId; }
  virtual const char* Name() const { return "NestedFilter"; }
  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual bool HasAsyncFlow() const { return true; }

 private:
  OutputResourceKind kind_;
  bool chain_;

  // Stats
  int num_top_rewrites_;
  int num_sub_rewrites_;

  DISALLOW_COPY_AND_ASSIGN(NestedFilter);
};

// Simple version of CombineCssFilter.
//
// Concatenates all CSS files loaded from <link> tags into a single output.
// Does not consider barriers, @import statements, absolutification, etc.
class CombiningFilter : public RewriteFilter {
 public:
  explicit CombiningFilter(RewriteDriver* driver)
      : RewriteFilter(driver, kCombiningFilterId) {
    ClearStats();
  }
  virtual ~CombiningFilter() {}

  class Combiner : public ResourceCombiner {
   public:
    Combiner(RewriteDriver* driver, RewriteFilter* filter)
        : ResourceCombiner(
            driver, kContentTypeCss.file_extension() + 1, filter) {
    }
    OutputResourcePtr MakeOutput() {
      return Combine(kContentTypeCss, rewrite_driver_->message_handler());
    }
    bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
      return WriteCombination(in, out, rewrite_driver_->message_handler());
    }
    virtual bool UseAsyncFlow() const { return true; }
  };


  class Context : public RewriteContext {
   public:
    Context(RewriteDriver* driver, CombiningFilter* filter)
        : RewriteContext(driver, NULL, NULL),
          combiner_(driver, filter),
          filter_(filter) {
    }

    void AddElement(HtmlElement* element, HtmlElement::Attribute* href,
                    const ResourcePtr& resource) {
      ResourceSlotPtr slot(Driver()->GetSlot(resource, element, href));
      AddSlot(slot);
    }

   protected:
    virtual bool Partition(OutputPartitions* partitions,
                           OutputResourceVector* outputs) {
      MessageHandler* handler = Driver()->message_handler();
      OutputPartition* partition = partitions->add_partition();
      for (int i = 0, n = num_slots(); i < n; ++i) {
        slot(i)->resource()->AddInputInfoToPartition(i, partition);
        if (!combiner_.AddResourceNoFetch(slot(i)->resource(), handler).value) {
          return false;
        }
      }
      OutputResourcePtr combination(combiner_.MakeOutput());

      // ResourceCombiner provides us with a pre-populated CachedResult,
      // so we need to copy it over to our OutputPartition.  This is
      // less efficient than having ResourceCombiner work with our
      // cached_result directly but this allows code-sharing as we
      // transition to the async flow.
      CachedResult* partition_result = partition->mutable_result();
      const CachedResult* combination_result = combination->cached_result();
      *partition_result = *combination_result;
      outputs->push_back(combination);
      return true;
    }

    virtual void Rewrite(int partition_index,
                         OutputPartition* partition,
                         const OutputResourcePtr& output) {
      ++filter_->num_rewrites_;
      // resource_combiner.cc takes calls WriteCombination as part
      // of Combine.  But if we are being called on behalf of a
      // fetch then the resource still needs to be written.
      RewriteSingleResourceFilter::RewriteResult result =
          RewriteSingleResourceFilter::kRewriteOk;
      if (!output->IsWritten()) {
        ResourceVector resources;
        for (int i = 0, n = num_slots(); i < n; ++i) {
          ResourcePtr resource(slot(i)->resource());
          resources.push_back(resource);
        }
        if (!combiner_.Write(resources, output)) {
          result = RewriteSingleResourceFilter::kRewriteFailed;
        }
      }
      RewriteDone(result, partition_index);
    }

    virtual void Render() {
      // Slot 0 will be replaced by the combined resource as part of
      // rewrite_context.cc.  But we still need to delete slots 1-N.
      for (int p = 0, np = num_output_partitions(); p < np; ++p) {
        OutputPartition* partition = output_partition(p);
        for (int i = 1; i < partition->input_size(); ++i) {
          int slot_index = partition->input(i).index();
          slot(slot_index)->set_should_delete_element(true);
        }
      }
    }

    virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }
    virtual const char* id() const { return kCombiningFilterId; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    Combiner combiner_;
    UrlMultipartEncoder encoder_;
    CombiningFilter* filter_;
  };

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
      if (href != NULL) {
        ResourcePtr resource(CreateInputResource(href->value()));
        if (resource.get() != NULL) {
          if (context_.get() == NULL) {
            context_.reset(new Context(driver_, this));
          }
          context_->AddElement(element, href, resource);
        }
      }
    }
  }

  virtual void Flush() {
    if (context_.get() != NULL) {
      driver_->InitiateRewrite(context_.release());
    }
  }

  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "Combining"; }
  RewriteContext* MakeRewriteContext() { return new Context(driver_, this); }
  virtual bool Fetch(const OutputResourcePtr& resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response,
                     MessageHandler* handler,
                     UrlAsyncFetcher::Callback* callback) {
    CHECK(false);
    return false;
  }
  virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }
  virtual bool HasAsyncFlow() const { return true; }

  // Stats
  bool num_rewrites() const { return num_rewrites_; }

  void ClearStats() { num_rewrites_ = 0; }

 private:
  friend class Context;

  scoped_ptr<Context> context_;
  UrlMultipartEncoder encoder_;
  int num_rewrites_;

  DISALLOW_COPY_AND_ASSIGN(CombiningFilter);
};

class RewriteContextTest : public ResourceManagerTestBase {
 protected:
  RewriteContextTest() : combining_filter_(NULL), nested_filter_(NULL) {}
  virtual ~RewriteContextTest() {}

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(true);
  }
  virtual void TearDown() {
    rewrite_driver()->WaitForCompletion();
    ResourceManagerTestBase::TearDown();
  }

  virtual bool AddBody() const { return false; }

  void InitResources() {
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    // trimmable
    SetFetchResponse("http://test.com/a.css", default_css_header, " a ");
    // not trimmable
    SetFetchResponse("http://test.com/b.css", default_css_header, "b");
    SetFetchResponse("http://test.com/c.css", default_css_header,
                     "a.css\nb.css\n");
  }

  void InitTrimFilters(OutputResourceKind kind) {
    InitTrimFilter(kind, rewrite_driver());
    InitTrimFilter(kind, other_rewrite_driver());
  }

  void InitTwoFilters(OutputResourceKind kind) {
    InitTwoFilters(kind, rewrite_driver());
    InitTwoFilters(kind, other_rewrite_driver());
  }

  void InitTrimFilter(OutputResourceKind kind, RewriteDriver* rewrite_driver) {
    rewrite_driver->AddRewriteFilter(
        TrimWhitespaceRewriter::MakeFilter(kind, rewrite_driver));
    rewrite_driver->AddFilters();
  }

  void InitTwoFilters(OutputResourceKind kind, RewriteDriver* rewrite_driver) {
    rewrite_driver->AddRewriteFilter(
        UpperCaseRewriter::MakeFilter(kind, rewrite_driver));
    InitTrimFilter(kind, rewrite_driver);
  }

  void InitCombiningFilter() {
    RewriteDriver* driver = rewrite_driver();
    combining_filter_ = new CombiningFilter(driver);
    driver->AddRewriteFilter(combining_filter_);
    driver->AddFilters();
  }

  void InitNestedFilter() {
    RewriteDriver* driver = rewrite_driver();
    nested_filter_ = new NestedFilter(driver);
    driver->AddRewriteFilter(nested_filter_);
    driver->AddFilters();
  }

  GoogleString CssLink(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }

  void ClearStats() {
    lru_cache()->ClearStats();
    counting_url_async_fetcher()->Clear();
    file_system()->ClearStats();
    if (combining_filter_ != NULL) {
      combining_filter_->ClearStats();
    }
    if (nested_filter_ != NULL) {
      nested_filter_->ClearStats();
    }
  }

  CombiningFilter* combining_filter_;
  NestedFilter* nested_filter_;
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
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a
  // single cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizable) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
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
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());  // 3 cause it's kRewrittenResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimRewrittenNonOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.  We'll cache
  // the successfully fetched resource, and the OutputPartitions which
  // indicates the unsuccessful optimization.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
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
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Now we'll let the fetcher call its callbacks -- we'll see the
  // cache-inserts now, and the next rewrite will succeed.
  //
  // TODO(jmarantz): Implement and test a threaded Rewrite.
  CallFetcherCallbacks();
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
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
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());   // 1 because output is not saved
  EXPECT_EQ(1, lru_cache()->num_inserts());  // ditto
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again.  This time the input URL is cached.
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimFetchRewritten) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  // We did the output_resource lookup twice: once before acquiring the lock,
  // and the second time after acquiring the lock, because presumably whoever
  // released the lock has now written the resource.
  //
  // TODO(jmarantz): have the lock-code return whether it had to wait to
  // get the lock or was able to acquire it immediately to avoid the
  // second cache lookup.
  EXPECT_EQ(3, lru_cache()->num_misses());   // output resource(twice), input
  EXPECT_EQ(2, lru_cache()->num_inserts());  // output resource(once), input
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again: the output URL is cached.
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
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

  // note: no InitResources so we'll get a file-not-found.
  SetFetchFailOnUnexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, RewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLink("a.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// In this testcase we'll attempt to serve a rewritten resource, but having
// failed to call InitResources we will not be able to do the on-the-fly
// rewrite.
TEST_F(RewriteContextTest, FetchColdCacheOnTheFlyNotFound) {
  InitTrimFilters(kOnTheFlyResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  GoogleString content;
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());  // We "remember" the fetch failure
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a 'hit' which will inform us
  // that this resource is not fetchable.
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());  // We "remember" the fetch failure
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Same testcase, but with a non-on-the-fly resource.
TEST_F(RewriteContextTest, FetchColdCacheRewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  GoogleString content;
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(0, lru_cache()->num_hits());

  // We lookup the output resource twice plus the inputs.
  EXPECT_EQ(3, lru_cache()->num_misses());

  // We currently "remember" the fetch failure, but *not* the failed
  // rewrite.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a 'hit' which will inform us
  // that this resource is not fetchable.
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(1, lru_cache()->num_hits());

  // Because we don't (currently) remember the failed output cache lookup we
  // will get two new cache misses here as well: once before we try to acquire
  // the lock, and the second after having acquired the lock.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TwoFilters) {
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateExpected(
      "two_filters", CssLink("a.css"),
      CssLink("http://test.com/a.css,Muc.0.css.pagespeed.tw.0.css"));
}

TEST_F(RewriteContextTest, TwoFiltersDelayedFetches) {
  SetupWaitFetcher();
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable1", CssLink("a.css"));
  CallFetcherCallbacks();
  ValidateExpected(
      "delayed_fetches", CssLink("a.css"),
      CssLink("http://test.com/a.css,Muc.0.css.pagespeed.tw.0.css"));
}

TEST_F(RewriteContextTest, Nested) {
  const char kRewrittenUrl[] = "http://test.com/c.css.pagespeed.nf.0.css";
  InitNestedFilter();
  InitResources();
  ValidateExpected("async3", CssLink("c.css"), CssLink(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(ServeResourceUrl(kRewrittenUrl, &rewritten_contents));
  EXPECT_EQ("http://test.com/a.css.pagespeed.nf.0.css\n"
            "http://test.com/b.css.pagespeed.nf.0.css\n", rewritten_contents);
}

TEST_F(RewriteContextTest, NestedChained) {
  const char kRewrittenUrl[] = "http://test.com/c.css.pagespeed.nf.0.css";

  RewriteDriver* driver = rewrite_driver();
  NestedFilter* nf = new NestedFilter(driver);
  nf->set_chain(true);
  driver->AddRewriteFilter(nf);
  driver->AddFilters();
  InitResources();
  ValidateExpected(
      "async_nest_chain", CssLink("c.css"), CssLink(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(ServeResourceUrl(kRewrittenUrl, &rewritten_contents));
  // We expect each URL twice since we have two nested jobs for it, and the
  // Harvest() just dumps each nested rewrites' slots.
  EXPECT_EQ("http://test.com/a.css.pagespeed.nf.0.css\n"
            "http://test.com/a.css.pagespeed.nf.0.css\n"
            "http://test.com/b.css.pagespeed.nf.0.css\n"
            "http://test.com/b.css.pagespeed.nf.0.css\n", rewritten_contents);
}

TEST_F(RewriteContextTest, CombinationRewrite) {
  InitCombiningFilter();
  InitResources();
  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     "a.css+b.css", "css");
  ValidateExpected(
      "combination_rewrite", StrCat(CssLink("a.css"), CssLink("b.css")),
      CssLink(combined_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected(
      "combination_rewrite2", StrCat(CssLink("a.css"), CssLink("b.css")),
      CssLink(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());     // the output is all we need
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, CombinationFetch) {
  InitCombiningFilter();
  InitResources();

  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     "a.css+b.css", "css");

  // The input URLs are not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses())
      << "2 misses for the output.  1 before we acquire the lock, "
      << "and one after we acquire the lock.  Then we miss on the two inputs.";

  // TODO(jmarantz): add another Insert to write partition-table for filters
  // that always make exactly one partition.
  EXPECT_EQ(3, lru_cache()->num_inserts()) << "2 inputs, 1 output.";
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again.  This time the output resource is cached.
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}


// Test that rewriting works correctly when input resource is loaded from disk.

TEST_F(RewriteContextTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kOnTheFlyResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 2 cache misses: one for the OutputPartitions, one for the input resource.
  EXPECT_EQ(2, lru_cache()->num_misses());
  // 1 cache insertion: resource mapping (OutputPartition).
  // Output resource not stored in cache (because it's an on-the-fly resource).
  EXPECT_EQ(1, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}

TEST_F(RewriteContextTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kRewrittenResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 2 cache misses: one for the OutputPartitions, one for the input resource.
  EXPECT_EQ(2, lru_cache()->num_misses());
  // 2 cache insertion: resource mapping (OutputPartition) and output resource.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}



// Test resource update behavior.
class ResourceUpdateTest : public RewriteContextTest {
 protected:
  static const char kOriginalUrl[];
  static const char kRewrittenUrlFormat[];

  ResourceUpdateTest() {
    FetcherUpdateDateHeaders();
  }

  // Rewrite supplied HTML, search find rewritten resource URL (EXPECT only 1),
  // and return the fetched contents of that resource.
  //
  // Helper function to specific functions below.
  GoogleString RewriteResource(const StringPiece& id,
                               const GoogleString& html_input) {
    // We use MD5 hasher instead of mock hasher so that different resources
    // are assigned different URLs.
    UseMd5Hasher();

    // Rewrite HTML.
    Parse(id, html_input);

    // Find rewritten resource URL.
    StringVector css_urls;
    CollectCssLinks(StrCat(id, "-collect"), output_buffer_, &css_urls);
    EXPECT_EQ(1UL, css_urls.size());
    const GoogleString& rewritten_url = css_urls[0];

    // Fetch rewritten resource
    return FetchUrlAndCheckHash(rewritten_url);
  }

  GoogleString FetchUrlAndCheckHash(const StringPiece& url) {
    // Fetch resource.
    GoogleString contents;
    EXPECT_TRUE(ServeResourceUrl(url, &contents));

    // Check that hash code is correct.
    ResourceNamer namer;
    namer.Decode(url);
    EXPECT_EQ(hasher()->Hash(contents), namer.hash());

    return contents;
  }

  // Simulates requesting HTML doc and then loading resource.
  GoogleString RewriteSingleResource(const StringPiece& id) {
    return RewriteResource(id, CssLink(kOriginalUrl));
  }
};

const char ResourceUpdateTest::kOriginalUrl[] = "a.css";
const char ResourceUpdateTest::kRewrittenUrlFormat[] =
    "http://test.com/a.css.pagespeed.tw.%s.css";

TEST_F(ResourceUpdateTest, OnTheFly) {
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  EXPECT_EQ("init", RewriteSingleResource("first_load"));

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
}

TEST_F(ResourceUpdateTest, Rewritten) {
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  EXPECT_EQ("init", RewriteSingleResource("first_load"));

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
}

TEST_F(ResourceUpdateTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  EXPECT_EQ("init", RewriteSingleResource("first_load"));

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  // Rewrite should imediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
}

TEST_F(ResourceUpdateTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  EXPECT_EQ("init", RewriteSingleResource("first_load"));

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  // Rewrite should imediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
}

class CombineResourceUpdateTest : public ResourceUpdateTest {
 protected:
  GoogleString CombineResources(const StringPiece& id) {
    return RewriteResource(
        id, StrCat(CssLink("web/a.css"), CssLink("file/b.css"),
                   CssLink("web/c.css"), CssLink("file/d.css")));
  }
};

TEST_F(CombineResourceUpdateTest, CombineDifferentTTLs) {
  // Initialize system.
  InitCombiningFilter();
  options()->file_load_policy()->Associate("http://test.com/file/", "/test/");

  // Initialize resources.
  int64 kLongTtlMs = 1 * Timer::kMonthMs;
  int64 kShortTtlMs = 1 * Timer::kMinuteMs;
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a1 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b1 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c1 ", kShortTtlMs / 1000);
  WriteFile("/test/d.css", " d1 ");

  // 1) Initial combined resource.
  EXPECT_EQ(" a1  b1  c1  d1 ", CombineResources("first_load"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(0, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  mock_timer()->AdvanceMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  EXPECT_EQ(" a1  b1  c1  d1 ", CombineResources("advance_time"));
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c2 ", kShortTtlMs / 1000);
  WriteFile("/test/d.css", " d2 ");
  // File-based resources should be updated, but web-based ones still cached.
  // TODO(sligocki): Fix
  EXPECT_EQ(" a1  b2  c1  d2 ", CombineResources("stale_content"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Read both files.
  // TODO(sligocki): EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  mock_timer()->AdvanceMs(kShortTtlMs);
  // All but long TTL UrlInputResource should be updated.
  EXPECT_EQ(" a1  b2  c2  d2 ", CombineResources("short_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // One expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // TODO(sligocki): EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  mock_timer()->AdvanceMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  EXPECT_EQ(" a2  b2  c2  d2 ", CombineResources("all_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // Both expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // TODO(sligocki): EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();
}

class NestedResourceUpdateTest : public ResourceUpdateTest {
 protected:
  StringVector RewriteNestedResources(const StringPiece& id) {
    // Rewrite everything and fetch the rewritten main resource.
    GoogleString rewritten_list = RewriteResource(id, CssLink("main.txt"));

    // Parse URLs for subresources.
    StringPieceVector urls;
    SplitStringPieceToVector(rewritten_list, "\n", &urls, true);

    // Load text of subresources.
    StringVector subresources;
    for (StringPieceVector::const_iterator iter = urls.begin();
         iter != urls.end(); ++iter) {
      subresources.push_back(FetchUrlAndCheckHash(*iter));
    }
    return subresources;
  }
};

TEST_F(NestedResourceUpdateTest, NestedDifferentTTLs) {
  // Initialize system.
  InitNestedFilter();
  options()->file_load_policy()->Associate("http://test.com/file/", "/test/");

  // Initialize resources.
  int64 kLongTtlMs = 1 * Timer::kMonthMs;
  int64 kShortTtlMs = 1 * Timer::kMinuteMs;
  InitResponseHeaders("http://test.com/main.txt", kContentTypeCss,
                      "web/a.css\n"
                      "file/b.css\n"
                      "web/c.css\n", kLongTtlMs / 1000);
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a1 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b1 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c1 ", kShortTtlMs / 1000);

  ClearStats();
  // 1) Initial combined resource.
  StringVector result_vector;
  result_vector = RewriteNestedResources("first_load");
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());
  EXPECT_EQ(3, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());  // b.css
  // {a,b,c}.css.pagespeed.nf.HASH.css and b.css
  EXPECT_EQ(4, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(0, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  mock_timer()->AdvanceMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  result_vector = RewriteNestedResources("advance_time");
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(0, nested_filter_->num_top_rewrites());
  EXPECT_EQ(0, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c2 ", kShortTtlMs / 1000);
  // File-based resources should be updated, but web-based ones still cached.
  result_vector = RewriteNestedResources("stale_content");
  EXPECT_EQ(" A1 ", result_vector[0]);
  // TODO(sligocki): Fix
  //EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  //EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  EXPECT_EQ(0, nested_filter_->num_top_rewrites());
  //EXPECT_EQ(1, nested_filter_->num_sub_rewrites());  // b.css
  EXPECT_EQ(0, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());  // b.css
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  mock_timer()->AdvanceMs(kShortTtlMs);
  // All but long TTL UrlInputResrouce should be updated.
  result_vector = RewriteNestedResources("short_updated");
  EXPECT_EQ(" A1 ", result_vector[0]);
  // TODO(sligocki): Fix
  //EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  //EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  //EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  EXPECT_EQ(0, nested_filter_->num_top_rewrites());
  //EXPECT_EQ(1, nested_filter_->num_sub_rewrites());  // c.css
  EXPECT_EQ(0, nested_filter_->num_sub_rewrites());
  //EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // c.css
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  mock_timer()->AdvanceMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  result_vector = RewriteNestedResources("short_updated");
  EXPECT_EQ(" A2 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  // TODO(sligocki): Fix
  //EXPECT_EQ(2, nested_filter_->num_sub_rewrites());  // a.css, c.css
  EXPECT_EQ(3, nested_filter_->num_sub_rewrites());  // a.css, b.css, c.css
  //EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // a.css, c.css
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(0, file_system()->num_input_file_opens());
  EXPECT_EQ(4, file_system()->num_input_file_opens());
  // TODO(sligocki): EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();
}

}  // namespace net_instaweb
