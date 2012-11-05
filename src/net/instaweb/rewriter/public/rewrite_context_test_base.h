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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_TEST_BASE_H_

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"  // for HTTPCache
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr, etc
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

class MessageHandler;
class TestRewriteDriverFactory;
class UrlSegmentEncoder;

// Simple test filter just trims whitespace from the input resource.
class TrimWhitespaceRewriter : public SimpleTextFilter::Rewriter {
 public:
  static const char kFilterId[];

  explicit TrimWhitespaceRewriter(OutputResourceKind kind) : kind_(kind) {
    ClearStats();
  }

  // Stats
  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(TrimWhitespaceRewriter);
  virtual ~TrimWhitespaceRewriter();

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ServerContext* resource_manager) {
    LOG(INFO) << "Trimming whitespace.";
    ++num_rewrites_;
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
  virtual const char* id() const { return kFilterId; }
  virtual const char* name() const { return "TrimWhitespace"; }

 private:
  OutputResourceKind kind_;

  int num_rewrites_;

  DISALLOW_COPY_AND_ASSIGN(TrimWhitespaceRewriter);
};

// Test filter that replaces a CSS resource URL with a corresponding Pagespeed
// resource URL.  When that URL is requested, it will invoke a rewriter that
// trims whitespace in the line of serving.  Does not require or expect the
// resource to be fetched or loaded from cache at rewrite time.
class TrimWhitespaceSyncFilter : public SimpleTextFilter {
 public:
  static const char kFilterId[];

  explicit TrimWhitespaceSyncFilter(OutputResourceKind kind,
                                    RewriteDriver* driver)
      : SimpleTextFilter(new TrimWhitespaceRewriter(kind), driver) {
  }

  virtual void StartElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
      if (href != NULL) {
        GoogleUrl gurl(driver()->google_url(), href->DecodedValueOrNull());
        href->SetValue(StrCat(gurl.Spec(), ".pagespeed.ts.0.css"));
      }
    }
  }

  virtual const char* id() const { return kFilterId; }
  virtual const char* name() const { return "TrimWhitespaceSync"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrimWhitespaceSyncFilter);
};

// A similarly structured test-filter: this one just upper-cases its text.
class UpperCaseRewriter : public SimpleTextFilter::Rewriter {
 public:
  static const char kFilterId[];

  explicit UpperCaseRewriter(OutputResourceKind kind)
      : kind_(kind), num_rewrites_(0) {}
  static SimpleTextFilter* MakeFilter(OutputResourceKind kind,
                                      RewriteDriver* driver,
                                      UpperCaseRewriter** rewriter_out) {
    *rewriter_out = new UpperCaseRewriter(kind);
    return new SimpleTextFilter(*rewriter_out, driver);
  }

  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(UpperCaseRewriter);
  virtual ~UpperCaseRewriter();

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ServerContext* resource_manager) {
    ++num_rewrites_;
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
  virtual const char* id() const { return kFilterId; }
  virtual const char* name() const { return "UpperCase"; }

 private:
  OutputResourceKind kind_;
  int num_rewrites_;

  DISALLOW_COPY_AND_ASSIGN(UpperCaseRewriter);
};

// Filter that contains nested resources that must themselves
// be rewritten.
class NestedFilter : public RewriteFilter {
 public:
  static const char kFilterId[];

  NestedFilter(RewriteDriver* driver, SimpleTextFilter* upper_filter,
               UpperCaseRewriter* upper_rewriter, bool expected_nested_result)
      : RewriteFilter(driver), upper_filter_(upper_filter),
        upper_rewriter_(upper_rewriter), chain_(false),
        expected_nested_rewrite_result_(expected_nested_result) {
    ClearStats();
  }

  // Stats
  int num_top_rewrites() const { return num_top_rewrites_; }
  int num_sub_rewrites() const { return upper_rewriter_->num_rewrites(); }

  void ClearStats() {
    num_top_rewrites_ = 0;
    upper_rewriter_->ClearStats();
  }

  // Set this to true to create a chain of nested rewrites on the same slot.
  void set_chain(bool x) { chain_ = x; }

  bool expected_nested_rewrite_result() const {
    return expected_nested_rewrite_result_;
  }

  void set_expected_nested_rewrite_result(bool x) {
    expected_nested_rewrite_result_ = x;
  }

 protected:
  virtual ~NestedFilter();

  class NestedSlot : public ResourceSlot {
   public:
    explicit NestedSlot(const ResourcePtr& resource) : ResourceSlot(resource) {}
    virtual void Render() {}
    virtual GoogleString LocationString() { return "nested:"; }
  };

  class Context : public SingleRewriteContext {
   public:
    Context(RewriteDriver* driver, NestedFilter* filter, bool chain)
        : SingleRewriteContext(driver, NULL, NULL),
          filter_(filter),
          chain_(chain) {
    }
    virtual ~Context();
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      ++filter_->num_top_rewrites_;
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
              ResourceSlotPtr slot(new NestedSlot(resource));
              RewriteContext* nested_context =
                  filter_->upper_filter()->MakeNestedRewriteContext(this, slot);
              AddNestedContext(nested_context);
              nested_slots_.push_back(slot);

              // Test chaining of a 2nd rewrite on the same slot, if asked.
              if (chain_) {
                RewriteContext* nested_context2 =
                    filter_->upper_filter()->MakeNestedRewriteContext(this,
                                                                      slot);
                AddNestedContext(nested_context2);
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
      RewriteResult result = kRewriteFailed;
      GoogleString new_content;

      for (int i = 0, n = nested_slots_.size(); i < n; ++i) {
        EXPECT_EQ(filter_->expected_nested_rewrite_result(),
                  nested_slots_[i]->was_optimized());
      }

      CHECK_EQ(1, num_slots());
      for (int i = 0, n = num_nested(); i < n; ++i) {
        CHECK_EQ(1, nested(i)->num_slots());
        ResourceSlotPtr slot(nested(i)->slot(0));
        ResourcePtr resource(slot->resource());
        StrAppend(&new_content, resource->url(), "\n");
      }
      ServerContext* resource_manager = FindServerContext();
      MessageHandler* message_handler = resource_manager->message_handler();
      // Warning: this uses input's content-type for simplicity, but real
      // filters should not do that --- see comments in
      // CacheExtender::RewriteLoadedResource as to why.
      if (resource_manager->Write(ResourceVector(1, slot(0)->resource()),
                                  new_content,
                                  slot(0)->resource()->type(),
                                  slot(0)->resource()->charset(),
                                  output(0).get(),
                                  message_handler)) {
        result = kRewriteOk;
      }
      RewriteDone(result, 0);
    }

   protected:
    virtual const char* id() const { return kFilterId; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    std::vector<GoogleString*> strings_;
    NestedFilter* filter_;
    bool chain_;
    ResourceSlotVector nested_slots_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  RewriteContext* MakeRewriteContext() {
    return new Context(driver_, this, chain_);
  }

  void StartElementImpl(HtmlElement* element) {
    HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kHref);
    if (attr != NULL) {
      ResourcePtr resource = CreateInputResource(attr->DecodedValueOrNull());
      if (resource.get() != NULL) {
        ResourceSlotPtr slot(driver_->GetSlot(resource, element, attr));

        // This 'new' is paired with a delete in RewriteContext::FinishFetch()
        Context* context = new Context(driver_, this, chain_);
        context->AddSlot(slot);
        driver_->InitiateRewrite(context);
      }
    }
  }

  SimpleTextFilter* upper_filter() { return upper_filter_; }

  virtual const char* id() const { return kFilterId; }
  virtual const char* Name() const { return "NestedFilter"; }
  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}

 private:
  // Upper-casing filter we also invoke.
  SimpleTextFilter* upper_filter_;
  UpperCaseRewriter* upper_rewriter_;
  bool chain_;

  // Whether we expect nested rewrites to be successful.
  bool expected_nested_rewrite_result_;

  // Stats
  int num_top_rewrites_;

  DISALLOW_COPY_AND_ASSIGN(NestedFilter);
};

// Simple version of CombineCssFilter.
//
// Concatenates all CSS files loaded from <link> tags into a single output.
// Does not consider barriers, @import statements, absolutification, etc.
class CombiningFilter : public RewriteFilter {
 public:
  static const char kFilterId[];

  CombiningFilter(RewriteDriver* driver,
                  MockScheduler* scheduler,
                  int64 rewrite_delay_ms)
    : RewriteFilter(driver),
      scheduler_(scheduler),
      rewrite_delay_ms_(rewrite_delay_ms),
      rewrite_block_on_(NULL),
      rewrite_signal_on_(NULL),
      on_the_fly_(false),
      optimization_only_(true),
      disable_successors_(false) {
    ClearStats();
  }
  virtual ~CombiningFilter();

  class Combiner : public ResourceCombiner {
   public:
    Combiner(RewriteDriver* driver, RewriteFilter* filter)
        : ResourceCombiner(
            driver, kContentTypeCss.file_extension() + 1, filter) {
    }
    OutputResourcePtr MakeOutput() {
      return Combine(rewrite_driver_->message_handler());
    }
    bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
      return WriteCombination(in, out, rewrite_driver_->message_handler());
    }

    virtual bool WritePiece(int index, const Resource* input,
                            OutputResource* combination,
                            Writer* writer, MessageHandler* handler) {
      writer->Write(prefix_, handler);
      return ResourceCombiner::WritePiece(
          index, input, combination, writer, handler);
    }

    void set_prefix(const GoogleString& prefix) { prefix_ = prefix; }

   private:
    virtual const ContentType* CombinationContentType() {
      return &kContentTypeCss;
    }

    GoogleString prefix_;
  };

  virtual const char* id() const { return kFilterId; }

  class Context : public RewriteContext {
   public:
    Context(RewriteDriver* driver, CombiningFilter* filter,
            MockScheduler* scheduler)
        : RewriteContext(driver, NULL, NULL),
          combiner_(driver, filter),
          scheduler_(scheduler),
          time_at_start_of_rewrite_us_(scheduler_->timer()->NowUs()),
          filter_(filter) {
      combiner_.set_prefix(filter_->prefix_);
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
      CachedResult* partition = partitions->add_partition();
      for (int i = 0, n = num_slots(); i < n; ++i) {
        slot(i)->resource()->AddInputInfoToPartition(
            Resource::kIncludeInputHash, i, partition);
        if (!combiner_.AddResourceNoFetch(slot(i)->resource(), handler).value) {
          return false;
        }
      }
      OutputResourcePtr combination(combiner_.MakeOutput());

      // ResourceCombiner provides us with a pre-populated CachedResult,
      // so we need to copy it over to our CachedResult.  This is
      // less efficient than having ResourceCombiner work with our
      // cached_result directly but this allows code-sharing as we
      // transition to the async flow.
      combination->UpdateCachedResultPreservingInputInfo(partition);
      DisableRemovedSlots(partition);
      outputs->push_back(combination);
      return true;
    }

    virtual void Rewrite(int partition_index,
                         CachedResult* partition,
                         const OutputResourcePtr& output) {
      if (filter_->rewrite_signal_on_ != NULL) {
        filter_->rewrite_signal_on_->Notify();
      }
      if (filter_->rewrite_block_on_ != NULL) {
        filter_->rewrite_block_on_->Wait();
      }
      if (filter_->rewrite_delay_ms() == 0) {
        DoRewrite(partition_index, partition, output);
      } else {
        int64 wakeup_us = time_at_start_of_rewrite_us_ +
            1000 * filter_->rewrite_delay_ms();
        Function* closure = MakeFunction(
            this, &Context::DoRewrite, partition_index, partition, output);
        scheduler_->AddAlarm(wakeup_us, closure);
      }
    }

    virtual bool OptimizationOnly() const {
      return filter_->optimization_only();
    }

    void DoRewrite(int partition_index,
                   CachedResult* partition,
                   OutputResourcePtr output) {
      ++filter_->num_rewrites_;
      // resource_combiner.cc takes calls WriteCombination as part
      // of Combine.  But if we are being called on behalf of a
      // fetch then the resource still needs to be written.
      RewriteResult result = kRewriteOk;
      if (!output->IsWritten()) {
        ResourceVector resources;
        for (int i = 0, n = num_slots(); i < n; ++i) {
          ResourcePtr resource(slot(i)->resource());
          resources.push_back(resource);
        }
        if (!combiner_.Write(resources, output)) {
          result = kRewriteFailed;
        }
      }
      RewriteDone(result, partition_index);
    }

    virtual void Render() {
      // Slot 0 will be replaced by the combined resource as part of
      // rewrite_context.cc.  But we still need to delete slots 1-N.
      for (int p = 0, np = num_output_partitions(); p < np; ++p) {
        DisableRemovedSlots(output_partition(p));
      }
    }

    void DisableRemovedSlots(CachedResult* partition) {
      if (filter_->disable_successors_) {
        slot(0)->set_disable_further_processing(true);
      }
      for (int i = 1; i < partition->input_size(); ++i) {
        int slot_index = partition->input(i).index();
        slot(slot_index)->RequestDeleteElement();
      }
    }

    virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }
    virtual const char* id() const { return kFilterId; }
    virtual OutputResourceKind kind() const {
      return filter_->on_the_fly_ ? kOnTheFlyResource : kRewrittenResource;
    }

   private:
    Combiner combiner_;
    UrlMultipartEncoder encoder_;
    MockScheduler* scheduler_;
    int64 time_at_start_of_rewrite_us_;
    CombiningFilter* filter_;
  };

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
      if (href != NULL) {
        ResourcePtr resource(CreateInputResource(href->DecodedValueOrNull()));
        if (resource.get() != NULL) {
          if (context_.get() == NULL) {
            context_.reset(new Context(driver_, this, scheduler_));
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
  RewriteContext* MakeRewriteContext() {
    return new Context(driver_, this, scheduler_);
  }
  virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }

  virtual bool ComputeOnTheFly() const { return on_the_fly_; }

  bool num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }
  int64 rewrite_delay_ms() const { return rewrite_delay_ms_; }
  void set_rewrite_block_on(WorkerTestBase::SyncPoint* sync) {
    rewrite_block_on_ = sync;
  }

  void set_rewrite_signal_on(WorkerTestBase::SyncPoint* sync) {
    rewrite_signal_on_ = sync;
  }

  // Each entry in combination will be prefixed with this.
  void set_prefix(const GoogleString& prefix) { prefix_ = prefix; }

  void set_on_the_fly(bool v) { on_the_fly_ = v; }

  void set_disable_successors(bool v) { disable_successors_ = v; }

  bool optimization_only() const { return optimization_only_; }
  void set_optimization_only(bool o) { optimization_only_ = o; }

 private:
  friend class Context;

  scoped_ptr<Context> context_;
  UrlMultipartEncoder encoder_;
  MockScheduler* scheduler_;
  int num_rewrites_;
  int64 rewrite_delay_ms_;

  // If this is non-NULL, the actual rewriting will block until this is
  // signaled. Applied before rewrite_delay_ms_
  WorkerTestBase::SyncPoint* rewrite_block_on_;

  // If this is non-NULL, this will be signaled the moment rewrite is called
  // on the context, before rewrite_block_on_ and rewrite_delay_ms_ are
  // applied.
  WorkerTestBase::SyncPoint* rewrite_signal_on_;
  GoogleString prefix_;
  bool on_the_fly_;  // If true, will act as an on-the-fly filter.
  bool optimization_only_;  // If false, will disable load-shedding and fetch
                            // rewrite deadlines.
  bool disable_successors_;  // if true, will disable successors for all
                             // slots, not just mutated ones.

  DISALLOW_COPY_AND_ASSIGN(CombiningFilter);
};

class RewriteContextTest : public RewriteTestBase {
 protected:
  static const int64 kRewriteDeadlineMs = 20;

  // Use a TTL value other than the implicit value, so we are sure we are using
  // the original TTL value.
  static const int64 kOriginTtlMs = 12 * Timer::kMinuteMs;

  // Use a TTL value other than the implicit value, so we are sure we are using
  // the original TTL value.
  GoogleString OriginTtlMaxAge() {
    return StrCat("max-age=", Integer64ToString(
        kOriginTtlMs / Timer::kSecondMs));
  }

  RewriteContextTest(
      std::pair<TestRewriteDriverFactory*, TestRewriteDriverFactory*> factories)
      : RewriteTestBase(factories) {}
  RewriteContextTest() {}
  virtual ~RewriteContextTest();

  virtual void SetUp() {
    trim_filter_ = NULL;
    other_trim_filter_ = NULL;
    combining_filter_ = NULL;
    nested_filter_ = NULL;
    logging_info_ = log_record_.logging_info();

    RewriteTestBase::SetUp();

    // The default deadline set in RewriteDriver is dependent on whether
    // the system was compiled for debug, or is being run under valgrind.
    // However, the unit-tests here use mock-time so we want to set the
    // deadline explicitly.
    rewrite_driver()->set_rewrite_deadline_ms(kRewriteDeadlineMs);
    other_rewrite_driver()->set_rewrite_deadline_ms(kRewriteDeadlineMs);
  }

  virtual void TearDown() {
    rewrite_driver()->WaitForShutDown();
    RewriteTestBase::TearDown();
  }

  virtual bool AddBody() const { return false; }

  void InitResources() {
    InitResourcesToDomain(kTestDomain);
  }

  void InitResourcesToDomain(const char* domain) {
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    int64 now_ms = http_cache()->timer()->NowMs();
    default_css_header.SetDateAndCaching(now_ms, kOriginTtlMs);
    default_css_header.ComputeCaching();

    // trimmable
    SetFetchResponse(StrCat(domain, "a.css"), default_css_header, " a ");

    // not trimmable
    SetFetchResponse(StrCat(domain, "b.css"), default_css_header, "b");
    SetFetchResponse(StrCat(domain, "c.css"), default_css_header,
                     "a.css\nb.css\n");

    // trimmable, with charset.
    ResponseHeaders encoded_css_header;
    server_context()->SetDefaultLongCacheHeadersWithCharset(
        &kContentTypeCss, "koi8-r", &encoded_css_header);
    SetFetchResponse(StrCat(domain, "a_ru.css"), encoded_css_header,
                     " a = \xc1 ");

    // trimmable, private
    ResponseHeaders private_css_header;
    now_ms = http_cache()->timer()->NowMs();
    private_css_header.set_major_version(1);
    private_css_header.set_minor_version(1);
    private_css_header.SetStatusAndReason(HttpStatus::kOK);
    private_css_header.SetDateAndCaching(now_ms, kOriginTtlMs, ",private");
    private_css_header.ComputeCaching();

    SetFetchResponse(StrCat(domain, "a_private.css"),
                     private_css_header,
                     " a ");

    // trimmable, no-cache
    ResponseHeaders no_cache_css_header;
    now_ms = http_cache()->timer()->NowMs();
    no_cache_css_header.set_major_version(1);
    no_cache_css_header.set_minor_version(1);
    no_cache_css_header.SetStatusAndReason(HttpStatus::kOK);
    no_cache_css_header.SetDateAndCaching(now_ms, 0, ",no-cache");
    no_cache_css_header.ComputeCaching();

    SetFetchResponse(StrCat(domain, "a_no_cache.css"),
                     no_cache_css_header,
                     " a ");

    // trimmable, no-cache, no-store
    ResponseHeaders no_store_css_header;
    now_ms = http_cache()->timer()->NowMs();
    no_store_css_header.set_major_version(1);
    no_store_css_header.set_minor_version(1);
    no_store_css_header.SetStatusAndReason(HttpStatus::kOK);
    no_store_css_header.SetDateAndCaching(now_ms, 0, ",no-cache,no-store");
    no_store_css_header.ComputeCaching();

    SetFetchResponse(StrCat(domain, "a_no_store.css"),
                     no_store_css_header,
                     " a ");
  }

  void InitTrimFilters(OutputResourceKind kind) {
    trim_filter_ = new TrimWhitespaceRewriter(kind);
    rewrite_driver()->AppendRewriteFilter(
        new SimpleTextFilter(trim_filter_, rewrite_driver()));
    rewrite_driver()->AddFilters();

    other_trim_filter_ = new TrimWhitespaceRewriter(kind);
    other_rewrite_driver()->AppendRewriteFilter(
        new SimpleTextFilter(other_trim_filter_, other_rewrite_driver()));
    other_rewrite_driver()->AddFilters();
  }

  void InitTrimFiltersSync(OutputResourceKind kind) {
    rewrite_driver()->AppendRewriteFilter(
        new TrimWhitespaceSyncFilter(kind, rewrite_driver()));
    rewrite_driver()->AddFilters();

    other_rewrite_driver()->AppendRewriteFilter(
        new TrimWhitespaceSyncFilter(kind, rewrite_driver()));
    other_rewrite_driver()->AddFilters();
  }

  void InitTwoFilters(OutputResourceKind kind) {
    InitUpperFilter(kind, rewrite_driver());
    InitUpperFilter(kind, other_rewrite_driver());
    InitTrimFilters(kind);
  }

  void InitUpperFilter(OutputResourceKind kind, RewriteDriver* rewrite_driver) {
    UpperCaseRewriter* rewriter;
    rewrite_driver->AppendRewriteFilter(
        UpperCaseRewriter::MakeFilter(kind, rewrite_driver, &rewriter));
  }

  void InitCombiningFilter(int64 rewrite_delay_ms) {
    RewriteDriver* driver = rewrite_driver();
    combining_filter_ = new CombiningFilter(driver, mock_scheduler(),
                                            rewrite_delay_ms);
    driver->AppendRewriteFilter(combining_filter_);
    driver->AddFilters();
  }

  void InitNestedFilter(bool expected_nested_rewrite_result) {
    RewriteDriver* driver = rewrite_driver();

    // Note that we only register this instance for rewrites, not HTML
    // handling, so that uppercasing doesn't end up messing things up before
    // NestedFilter gets to them.
    UpperCaseRewriter* upper_rewriter;
    SimpleTextFilter* upper_filter =
        UpperCaseRewriter::MakeFilter(kOnTheFlyResource, driver,
                                      &upper_rewriter);
    AddFetchOnlyRewriteFilter(upper_filter);
    nested_filter_ = new NestedFilter(driver, upper_filter, upper_rewriter,
                                      expected_nested_rewrite_result);
    driver->AppendRewriteFilter(nested_filter_);
    driver->AddFilters();
  }

  void ReconfigureNestedFilter(bool expected_nested_rewrite_result) {
    nested_filter_->set_expected_nested_rewrite_result(
        expected_nested_rewrite_result);
  }

  void SetCacheInvalidationTimestamp() {
    options()->ClearSignatureForTesting();
    options()->set_cache_invalidation_timestamp(mock_timer()->NowMs());
    options()->ComputeSignature(hasher());
  }

  void SetCacheInvalidationUrlTimestamp(StringPiece url, bool is_strict) {
    options()->ClearSignatureForTesting();
    options()->AddUrlCacheInvalidationEntry(
        url, mock_timer()->NowMs(), is_strict);
    options()->ComputeSignature(hasher());
  }

  virtual void ClearStats() {
    RewriteTestBase::ClearStats();
    if (trim_filter_ != NULL) {
      trim_filter_->ClearStats();
    }
    if (other_trim_filter_ != NULL) {
      other_trim_filter_->ClearStats();
    }
    if (combining_filter_ != NULL) {
      combining_filter_->ClearStats();
    }
    if (nested_filter_ != NULL) {
      nested_filter_->ClearStats();
    }
    log_record_.logging_info()->Clear();
  }

  TrimWhitespaceRewriter* trim_filter_;
  TrimWhitespaceRewriter* other_trim_filter_;
  CombiningFilter* combining_filter_;
  NestedFilter* nested_filter_;
  LogRecord log_record_;
  const LoggingInfo* logging_info_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_TEST_BASE_H_
