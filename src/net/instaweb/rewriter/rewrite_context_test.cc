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
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/common_filter.h"
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
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;

namespace {

const char kTrimWhitespaceFilterId[] = "tw";
const char kTrimWhitespaceSyncFilterId[] = "ts";
const char kUpperCaseFilterId[] = "uc";
const char kNestedFilterId[] = "nf";
const char kCombiningFilterId[] = "cr";
const int64 kRewriteDeadlineMs = 20;

// This value needs to be bigger than rewrite driver timeout;
// and it's useful while debugging for it to not be the driver
// timeout's multiple (so one can easily tell its occurrences
// from repetitions of the driver's timeout).
const int64 kRewriteDelayMs = 47;

// For use with NestedFilter constructor
const bool kExpectNestedRewritesSucceed = true;
const bool kExpectNestedRewritesFail = false;

const int64 kOriginTtlMs = 5 * Timer::kMinuteMs;

}  // namespace

// Simple test filter just trims whitespace from the input resource.
class TrimWhitespaceRewriter : public SimpleTextFilter::Rewriter {
 public:
  explicit TrimWhitespaceRewriter(OutputResourceKind kind) : kind_(kind) {
    ClearStats();
  }

  // Stats
  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(TrimWhitespaceRewriter);
  virtual ~TrimWhitespaceRewriter() {}

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ResourceManager* resource_manager) {
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
  virtual const char* id() const { return kTrimWhitespaceFilterId; }
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
  explicit TrimWhitespaceSyncFilter(OutputResourceKind kind,
                                    RewriteDriver* driver)
      : SimpleTextFilter(new TrimWhitespaceRewriter(kind), driver) {
  }

  virtual void StartElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
      if (href != NULL) {
        GoogleUrl gurl(driver()->google_url(), href->value());
        href->SetValue(StrCat(gurl.Spec(), ".pagespeed.ts.0.css").c_str());
      }
    }
  }

  virtual const char* id() const { return kTrimWhitespaceSyncFilterId; }
  virtual const char* name() const { return "TrimWhitespaceSync"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrimWhitespaceSyncFilter);
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
  explicit NestedFilter(RewriteDriver* driver, bool expected_nested_result)
      : RewriteFilter(driver), chain_(false),
        expected_nested_rewrite_result_(expected_nested_result) {
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

  bool expected_nested_rewrite_result() const {
    return expected_nested_rewrite_result_;
  }

  void set_expected_nested_rewrite_result(bool x) {
    expected_nested_rewrite_result_ = x;
  }

 protected:
  virtual ~NestedFilter() {}

  class NestedSlot : public ResourceSlot {
   public:
    explicit NestedSlot(const ResourcePtr& resource) : ResourceSlot(resource) {}
    virtual void Render() {}
    virtual GoogleString LocationString() { return "nested:"; }
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
              nested_slots_.push_back(slot);

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

      for (int i = 0, n = nested_slots_.size(); i < n; ++i) {
        EXPECT_EQ(filter_->expected_nested_rewrite_result(),
                  nested_slots_[i]->was_optimized());
      }

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
    ResourceSlotVector nested_slots_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  bool Fetch(const OutputResourcePtr& output_resource,
             Writer* response_writer,
             const RequestHeaders& request_header,
             ResponseHeaders* response_headers,
             MessageHandler* message_handler,
             UrlAsyncFetcher::Callback* callback) {
    LOG(FATAL);
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

 private:
  OutputResourceKind kind_;
  bool chain_;

  // Whether we expect nested rewrites to be successful.
  bool expected_nested_rewrite_result_;

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
  CombiningFilter(RewriteDriver* driver,
                  MockScheduler* scheduler,
                  int64 rewrite_delay_ms)
    : RewriteFilter(driver),
      scheduler_(scheduler),
      rewrite_delay_ms_(rewrite_delay_ms),
      on_the_fly_(false) {
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

    virtual bool WritePiece(int index, const Resource* input,
                            OutputResource* combination,
                            Writer* writer, MessageHandler* handler) {
      writer->Write(prefix_, handler);
      return ResourceCombiner::WritePiece(
          index, input, combination, writer, handler);
    }

    void set_prefix(const GoogleString& prefix) { prefix_ = prefix; }

   private:
    GoogleString prefix_;
  };

  virtual const char* id() const { return kCombiningFilterId; }

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
      outputs->push_back(combination);
      return true;
    }

    virtual void Rewrite(int partition_index,
                         CachedResult* partition,
                         const OutputResourcePtr& output) {
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

    void DoRewrite(int partition_index,
                   CachedResult* partition,
                   OutputResourcePtr output) {
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
        CachedResult* partition = output_partition(p);
        for (int i = 1; i < partition->input_size(); ++i) {
          int slot_index = partition->input(i).index();
          slot(slot_index)->set_should_delete_element(true);
        }
      }
    }

    virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }
    virtual const char* id() const { return kCombiningFilterId; }
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
        ResourcePtr resource(CreateInputResource(href->value()));
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
  virtual bool Fetch(const OutputResourcePtr& resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response,
                     MessageHandler* handler,
                     UrlAsyncFetcher::Callback* callback) {
    LOG(FATAL);
    return false;
  }
  virtual const UrlSegmentEncoder* encoder() const { return &encoder_; }

  virtual bool ComputeOnTheFly() const { return on_the_fly_; }

  bool num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }
  int64 rewrite_delay_ms() const { return rewrite_delay_ms_; }

  // Each entry in combination will be prefixed with this.
  void set_prefix(const GoogleString& prefix) { prefix_ = prefix; }

  void set_on_the_fly(bool v) { on_the_fly_ = true; }

 private:
  friend class Context;

  scoped_ptr<Context> context_;
  UrlMultipartEncoder encoder_;
  MockScheduler* scheduler_;
  int num_rewrites_;
  int64 rewrite_delay_ms_;
  GoogleString prefix_;
  bool on_the_fly_;  // If true, will act as an on-the-fly filter.

  DISALLOW_COPY_AND_ASSIGN(CombiningFilter);
};

class RewriteContextTest : public ResourceManagerTestBase {
 protected:
  RewriteContextTest() : trim_filter_(NULL), other_trim_filter_(NULL),
                         combining_filter_(NULL), nested_filter_(NULL) {}
  virtual ~RewriteContextTest() {}

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    // The default deadline set in RewriteDriver is dependent on whether
    // the system was compiled for debug, or is being run under valgrind.
    // However, the unit-tests here use mock-time so we want to set the
    // deadline explicitly.
    rewrite_driver()->set_rewrite_deadline_ms(kRewriteDeadlineMs);
    other_rewrite_driver()->set_rewrite_deadline_ms(kRewriteDeadlineMs);
  }

  virtual void TearDown() {
    rewrite_driver()->WaitForShutDown();
    ResourceManagerTestBase::TearDown();
  }

  virtual bool AddBody() const { return false; }

  void InitResources() {
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    int64 now_ms = http_cache()->timer()->NowMs();
    default_css_header.SetDateAndCaching(now_ms, kOriginTtlMs);
    default_css_header.ComputeCaching();

    // trimmable
    SetFetchResponse("http://test.com/a.css", default_css_header, " a ");
    // not trimmable
    SetFetchResponse("http://test.com/b.css", default_css_header, "b");
    SetFetchResponse("http://test.com/c.css", default_css_header,
                     "a.css\nb.css\n");

    // trimmable, private
    ResponseHeaders private_css_header;
    now_ms = http_cache()->timer()->NowMs();
    private_css_header.set_major_version(1);
    private_css_header.set_minor_version(1);
    private_css_header.SetStatusAndReason(HttpStatus::kOK);
    private_css_header.SetDateAndCaching(now_ms, kOriginTtlMs, ",private");
    private_css_header.ComputeCaching();

    SetFetchResponse("http://test.com/a_private.css",
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

    SetFetchResponse("http://test.com/a_no_cache.css",
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

    SetFetchResponse("http://test.com/a_no_store.css",
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
    rewrite_driver->AppendRewriteFilter(
        UpperCaseRewriter::MakeFilter(kind, rewrite_driver));
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
    nested_filter_ = new NestedFilter(driver, expected_nested_rewrite_result);
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

  virtual void ClearStats() {
    ResourceManagerTestBase::ClearStats();
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
  }

  TrimWhitespaceRewriter* trim_filter_;
  TrimWhitespaceRewriter* other_trim_filter_;
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
  GoogleString input_html(CssLinkHref("a.css"));
  GoogleString output_html(CssLinkHref(
      Encode(kTestDomain, "tw", "0", "a.css", "css")));
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  ClearStats();

  // The second time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a
  // single cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  ClearStats();

  // The third time we request this URL, we've advanced time so that the origin
  // resource TTL has expired.  The data will be re-fetched, and the Date
  // corrected.   See url_input_resource.cc, AddToCache().  The http cache will
  // miss, but we'll re-insert.  We won't need to do any more rewrites because
  // the data did not actually change.
  mock_timer()->AdvanceMs(2 * kOriginTtlMs);
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(2, lru_cache()->num_hits());     // 1 expired hit, 1 valid hit.
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // re-inserts after expiration.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_expirations()->Get());
  ClearStats();

  // The fourth time we request this URL, the cache is in good shape despite
  // the expired date header from the origin.
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());     // 1 expired hit, 1 valid hit.
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());  // re-inserts after expiration.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  ClearStats();
}

TEST_F(RewriteContextTest, TrimOnTheFlyOptimizableCacheInvalidation) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // The first rewrite was successful because we got an 'instant' url
  // fetch, not because we did any cache lookups. We'll have 2 cache
  // misses: one for the OutputPartitions, one for the fetch.  We
  // should need two items in the cache: the element and the resource
  // mapping (OutputPartitions).  The output resource should not be
  // stored.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a
  // single cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we invalidate the cache and then request the URL.
  SetCacheInvalidationTimestamp();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  // Setting the cache invalidation timestamp causes the partition key to change
  // and hence we get a cache miss (and insert) on the metadata. The resource is
  // then obtained from http cache.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizable) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizableCacheInvalidation) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we invalidate the cache and then request the URL.
  SetCacheInvalidationTimestamp();
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  // Setting the cache invalidation timestamp causes the partition key to change
  // and hence we get a cache miss (and insert) on the metadata. The resource is
  // then obtained from http cache.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
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
  // should need three items in the cache: the element, the resource
  // mapping (OutputPartitions) and the output resource.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());  // 3 cause it's kRewrittenResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata (or output?).  No cache misses will occur.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimRewrittenNonOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was on-the-fly and optimizable.
  // We'll cache the successfully fetched resource, and the OutputPartitions
  // which indicates the unsuccessful optimization.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimRepeatedOptimizable) {
  // Make sure two instances of the same link are handled properly,
  // when optimization succeeds.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateExpected(
        "trimmable2", StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
        StrCat(CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")),
               CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css"))));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, TrimRepeatedOptimizableDelayed) {
  // Make sure two instances of the same link are handled properly,
  // when optimization succeeds --- but fetches are slow.
  SetupWaitFetcher();
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First time nothing happens by deadline.
  ValidateNoChanges("trimable2_notyet",
                    StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")));

  CallFetcherCallbacks();

  // Second time we get both rewritten right.
  ValidateExpected(
      "trimmable2_now",
      StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
      StrCat(CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")),
             CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css"))));

  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, TrimRepeatedNonOptimizable) {
  // Make sure two instances of the same link are handled properly --
  // when optimization fails.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateNoChanges("notrimmable2",
                    StrCat(CssLinkHref("b.css"), CssLinkHref("b.css")));
}

TEST_F(RewriteContextTest, TrimRepeated404) {
  // Make sure two instances of the same link are handled properly --
  // when fetch fails.
  InitTrimFilters(kRewrittenResource);
  SetFetchResponse404("404.css");
  ValidateNoChanges("repeat404",
                    StrCat(CssLinkHref("404.css"), CssLinkHref("404.css")));
}

TEST_F(RewriteContextTest, FetchNonOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // Fetching a resource that's not optimizable under the rewritten URL
  // should still work in a single-input case. This is important to be more
  // robust against JS URL manipulation.
  GoogleString output;
  EXPECT_TRUE(ServeResourceUrl(Encode(kTestDomain, "tw", "0", "b.css", "css"),
                               &output));
  EXPECT_EQ("b", output);
}

TEST_F(RewriteContextTest, FetchNoSource) {
  InitTrimFilters(kRewrittenResource);
  SetFetchFailOnUnexpected(false);
  EXPECT_FALSE(
      TryFetchResource(Encode(kTestDomain, "tw", "0", "b.css", "css")));
}

// In the above tests, our URL fetcher called its callback directly, allowing
// the Rewrite to occur while the RewriteDriver was still attached.  In this
// run, we will delay the URL fetcher's callback so that the initial Rewrite
// will not take place until after the HTML has been flushed.
TEST_F(RewriteContextTest, TrimDelayed) {
  SetupWaitFetcher();
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable", CssLinkHref("a.css"));
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
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
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
  EXPECT_EQ(2, lru_cache()->num_inserts());  // input, metadata
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
  EXPECT_EQ(4, lru_cache()->num_misses());  // 2x output, metadata, input
  EXPECT_EQ(3, lru_cache()->num_inserts());  // output resource, input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again: the output URL is cached.
  ResponseHeaders headers;
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content, &headers));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Make sure headers are nice and long.
  EXPECT_EQ(Timer::kYearMs, headers.cache_ttl_ms());
  EXPECT_TRUE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsCacheable());
}

TEST_F(RewriteContextTest, TrimFetchSeedsCache) {
  // Make sure that rewriting on resource request also caches it for
  // future use for HTML.
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
  EXPECT_EQ(4, lru_cache()->num_misses());   // 2x output, metadata, input
  EXPECT_EQ(3, lru_cache()->num_inserts());  // output resource, input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  ClearStats();

  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());  // Just metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, trim_filter_->num_rewrites());  // cached.
}

TEST_F(RewriteContextTest, TrimFetchRewriteFailureSeedsCache) {
  // Make sure that rewriting on resource request also caches it for
  // future use for HTML, in the case where the rewrite fails.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "b.css",
                            "css", &content));
  EXPECT_EQ("b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());   // 2x output, metadata, input
  EXPECT_EQ(2, lru_cache()->num_inserts());  // input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  ClearStats();

  ValidateNoChanges("nontrimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // Just metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, trim_filter_->num_rewrites());  // cached.
}

TEST_F(RewriteContextTest, TrimFetch404SeedsCache) {
  // Check that we cache a 404, and cache it for a reasonable amount of time.
  InitTrimFilters(kRewrittenResource);
  SetFetchResponse404("404.css");

  GoogleString content;
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "404.css",
                             "css", &content));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Should cache immediately...
  ValidateNoChanges("404", CssLinkHref("404.css"));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // ... but not for too long.
  mock_timer()->AdvanceMs(Timer::kDayMs);
  ValidateNoChanges("404", CssLinkHref("404.css"));
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

// Verifies that rewriters can replace resource URLs without kicking off any
// fetching or caching.
TEST_F(RewriteContextTest, ClobberResourceUrlSync) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();
  GoogleString input_html(CssLinkHref("a_private.css"));
  GoogleString output_html(CssLinkHref(
      Encode(kTestDomain, "ts", "0", "a_private.css", "css")));
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
}

// Verifies that when an HTML document references an uncacheable resource, that
// reference does not get modified.
TEST_F(RewriteContextTest, DoNotModifyReferencesToUncacheableResources) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  GoogleString input_html(CssLinkHref("a_private.css"));

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());  // partition, resource
  EXPECT_EQ(2, lru_cache()->num_inserts());  // partition, not-cacheable memo
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // the resource
  ClearStats();

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(1, lru_cache()->num_hits());  // not-cacheable memo
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(1, lru_cache()->num_hits());  // not-cacheable memo
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
};

// Verifies that we can rewrite uncacheable resources without caching them.
TEST_F(RewriteContextTest, FetchUncacheableWithRewritesInLineOfServing) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;

  // The first time we serve the resource, we insert a memo that it is
  // uncacheable, and a name mapping.
  EXPECT_TRUE(ServeResource(kTestDomain,
                            kTrimWhitespaceSyncFilterId,
                            "a_private.css",
                            "css",
                            &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // name mapping & uncacheable memo
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Each subsequent time we serve the resource, we should experience a cache
  // hit for the notation that the resource is uncacheable, and then we should
  // perform an origin fetch anyway.
  for (int i = 0; i < 3; ++i) {
    ClearStats();
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_private.css",
                              "css",
                              &content));
    EXPECT_EQ("a", content);
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  }

  // Now, we change the resource.
  ResponseHeaders private_css_header;
  private_css_header.set_major_version(1);
  private_css_header.set_minor_version(1);
  private_css_header.SetStatusAndReason(HttpStatus::kOK);
  private_css_header.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                       kOriginTtlMs,
                                       ", private");
  private_css_header.ComputeCaching();

  SetFetchResponse("http://test.com/a_private.css",
                   private_css_header,
                   " b ");

  // We should continue to experience cache hits, and continue to fetch from
  // the origin.
  for (int i = 0; i < 3; ++i) {
    ClearStats();
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_private.css",
                              "css",
                              &content));
    EXPECT_EQ("b", content);
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  }
  ClearStats();

  // After advancing the time, we should see new cache inserts.  Note that we
  // also get a cache hit because the out-of-date entries are still there.
  mock_timer()->AdvanceMs(Timer::kMinuteMs * 50);
  EXPECT_TRUE(ServeResource(kTestDomain,
                            kTrimWhitespaceSyncFilterId,
                            "a_private.css",
                            "css",
                            &content));
  EXPECT_EQ("b", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

// Verifies that we preserve cache-control when rewriting a no-cache resource.
TEST_F(RewriteContextTest, PreserveNoCacheWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_no_cache.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ("max-age=0", *values[0]);
    EXPECT_STREQ("no-cache", *values[1]);
  }
}

TEST_F(RewriteContextTest, PreserveNoCacheWithFailedRewrites) {
  // Make sure propagation of non-cacheability works in case when
  // rewrite failed. (This relies on cache extender explicitly
  // rejecting to rewrite non-cacheable things).
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();

  InitResources();

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(ServeResource(kTestDomain,
                              "ce",
                              "a_no_cache.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ(" a ", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ("max-age=0", *values[0]);
    EXPECT_STREQ("no-cache", *values[1]);
  }
}

// Verifies that we preserve cache-control when rewriting a no-cache resource
// with a non-on-the-fly filter
TEST_F(RewriteContextTest, PrivateNotCached) {
  InitTrimFiltersSync(kRewrittenResource);
  InitResources();
  resource_manager()->set_store_outputs_in_file_system(false);

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    // There are two possible secure outcomes here: either the fetch fails
    // entirely here, or we serve it as cache-control: private. For now
    // we just fail it.
    EXPECT_FALSE(ServeResource(kTestDomain,
                               kTrimWhitespaceSyncFilterId,
                               "a_private.css",
                               "css",
                               &content,
                               &headers));
    EXPECT_TRUE(content.empty());
  }

  // Now make sure that fetching with an invalid hash doesn't work when
  // the original is not available. This is significant since if it this fails
  // an attacker may get access to resources without access to an actual hash.
  GoogleString output;
  mock_url_fetcher()->Disable();
  EXPECT_FALSE(ServeResourceUrl(
      Encode(kTestDomain, kTrimWhitespaceSyncFilterId, "1", "a_private.css",
             "css"),
      &output));
}

TEST_F(RewriteContextTest, PrivateNotCachedOnTheFly) {
  // Variant of the above for on-the-fly, as that relies on completely
  // different code paths to be safe. (It is also covered by earlier tests,
  // but this is included here to be thorough).
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();
  resource_manager()->set_store_outputs_in_file_system(false);

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_private.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
        << " Not private on fetch #" << i << " " << headers.ToString();
  }

  // Now make sure that fetching with an invalid hash doesn't work when
  // the original is not available. This is significant since if it this fails
  // an attacker may get access to resources without access to an actual hash.
  GoogleString output;
  mock_url_fetcher()->Disable();
  EXPECT_FALSE(ServeResourceUrl(
      Encode(kTestDomain, kTrimWhitespaceSyncFilterId, "1", "a_private.css",
             "css"),
      &output));
}

// Verifies that we preserve cache-control when rewriting a no-store resource.
TEST_F(RewriteContextTest, PreserveNoStoreWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_no_store.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-cache"));
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-store"));
  }
}

// Verifies that we preserve cache-control when rewriting a private resource.
TEST_F(RewriteContextTest, PreservePrivateWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a_private.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ("max-age=300", *values[0]);
    EXPECT_STREQ("private", *values[1]);
  }
}

// Verifies that we intersect cache-control when there are multiple input
// resources.
TEST_F(RewriteContextTest, CacheControlWithMultipleInputResources) {
  InitCombiningFilter(0);
  combining_filter_->set_on_the_fly(true);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, kCombiningFilterId, "0",
             MultiUrl("a.css",
                      "b.css",
                      "a_private.css"), "css");

  ServeResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b a ", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // 3 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts()) <<
      "partition, 2 inputs, 1 non-cacheability note";
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());

  ConstStringStarVector values;
  headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("max-age=300", *values[0]);
  EXPECT_STREQ("private", *values[1]);
}

// Verifies that we intersect cache-control when there are multiple input
// resources.
TEST_F(RewriteContextTest, CacheControlWithMultipleInputResourcesAndNoStore) {
  InitCombiningFilter(0);
  combining_filter_->set_on_the_fly(true);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, kCombiningFilterId, "0",
             MultiUrl("a.css",
                      "b.css",
                      "a_private.css",
                      "a_no_store.css"), "css");

  ServeResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b a  a ", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());   // 4 inputs.
  EXPECT_EQ(5, lru_cache()->num_inserts())
      << "partition, 2 inputs, 2 non-cacheability notes";
  EXPECT_EQ(4, counting_url_async_fetcher()->fetch_count());

  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-cache"));
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-store"));
}

// Verifies that we cache-extend when rewriting a cacheable resource.
TEST_F(RewriteContextTest, CacheExtendCacheableResource) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(ServeResource(kTestDomain,
                              kTrimWhitespaceSyncFilterId,
                              "a.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_STREQ(StringPrintf("max-age=%lld",
                              ResourceManager::kGeneratedMaxAgeMs/1000),
                 headers.Lookup1(HttpAttributes::kCacheControl));
  }
}

TEST_F(RewriteContextTest, FetchColdCacheOnTheFly) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  ClearStats();
  TestServeFiles(&kContentTypeCss, kTrimWhitespaceFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");
}

TEST_F(RewriteContextTest, TrimFetchWrongHash) {
  // Test to see that fetches from wrong hash can fallback to the
  // correct one mentioned in metadata correctly.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First rewrite a page to get the right hash remembered
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  ClearStats();

  // Now try fetching it with the wrong hash)
  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(ServeResourceUrl(Encode(kTestDomain, "tw", "1", "a.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("a", contents);
  // Should not need any rewrites or fetches.
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // Should have 2 hits: metadata and .0., and 2 misses on wrong-hash version
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Make sure the TTL is correct, and the result is private.
  EXPECT_EQ(ResponseHeaders::kImplicitCacheTtlMs, headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsCacheable());
}

TEST_F(RewriteContextTest, TrimFetchWrongHashColdCache) {
  // Tests fetch with wrong hash when we did not originally create
  // the version with the right hash.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(ServeResourceUrl(Encode(kTestDomain, "tw", "1", "a.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("a", contents);

  // Make sure the TTL is correct (short), and the result is private.
  EXPECT_EQ(ResponseHeaders::kImplicitCacheTtlMs, headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsCacheable());
}

TEST_F(RewriteContextTest, TrimFetchHashFailed) {
  // Test to see that if we try to fetch a rewritten version (with a pagespeed
  // resource URL) when metadata cache indicates rewrite of original failed
  // that we will quickly fallback to original without attempting rewrite.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  ClearStats();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(ServeResourceUrl(Encode(kTestDomain, "tw", "1", "b.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("b", contents);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // Should have 2 hits: metadata and .0., and 2 misses on wrong-hash version
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Make sure the TTL is correct, and the result is private.
  EXPECT_EQ(ResponseHeaders::kImplicitCacheTtlMs, headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsCacheable());
}

TEST_F(RewriteContextTest, TrimFetchHashFailedShortTtl) {
  // Variation of TrimFetchHashFailed, where the input's TTL is very short.
  const int kTestTtlSec = 5;
  InitTrimFilters(kRewrittenResource);
  InitResponseHeaders("d.css", kContentTypeCss, "d", kTestTtlSec);
  ValidateNoChanges("no_trimmable", CssLinkHref("d.css"));
  ClearStats();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(ServeResourceUrl(Encode(kTestDomain, "tw", "1", "d.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("d", contents);
  EXPECT_EQ(kTestTtlSec * Timer::kSecondMs, headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsCacheable());
}

TEST_F(RewriteContextTest, FetchColdCacheRewritten) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
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
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
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
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
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
  EXPECT_EQ(2, lru_cache()->num_inserts());  // fetch failure, metadata.
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

  // We lookup the output resource twice plus the inputs and metadata.
  EXPECT_EQ(4, lru_cache()->num_misses());

  // We remember the fetch failure, and the failed rewrite.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a number of hits which will
  // inform us that this resource is not fetchable:
  // - a metadata entry stating there is no successful rewrite.
  // - HTTP cache entry for resource fetch of original failing
  // - 2nd access of it when we give up on fast path.
  // TODO(morlovich): Should we propagate the 404 directly?
  EXPECT_FALSE(ServeResource(kTestDomain, kTrimWhitespaceFilterId, "a.css",
                            "css", &content));
  EXPECT_EQ(3, lru_cache()->num_hits());

  // Because we don't write out under failed output resource name,
  // will get two new cache misses here as well: once before we try to acquire
  // the lock, and the second after having acquired the lock.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TwoFilters) {
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateExpected("two_filters",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      Encode("", "uc", "0", "a.css", "css"),
                                      "css")));
}

TEST_F(RewriteContextTest, TwoFiltersDelayedFetches) {
  SetupWaitFetcher();
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable1", CssLinkHref("a.css"));
  CallFetcherCallbacks();
  ValidateExpected("delayed_fetches",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      Encode("", "uc", "0", "a.css", "css"),
                                      "css")));
}

TEST_F(RewriteContextTest, RepeatedTwoFilters) {
  // Make sure if we have repeated URLs and chaining, it still works right.
  InitTwoFilters(kRewrittenResource);
  InitResources();

  ValidateExpected(
    "two_filters2", StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
    StrCat(CssLinkHref(Encode(kTestDomain, "tw", "0",
                              Encode("", "uc", "0", "a.css", "css"), "css")),
           CssLinkHref(Encode(kTestDomain, "tw", "0",
                              Encode("", "uc", "0", "a.css", "css"), "css"))));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, ReconstructChainedWrongHash) {
  // Make sure that we don't have problems with repeated reconstruction
  // of chained rewrites where the hash is incorrect. (We used to screw
  // up the response code if two different wrong inner hashes were used,
  // leading to failure at outer level). Also make sure we always propagate
  // short TTL as well, since that could also be screwed up.
  resource_manager()->set_store_outputs_in_file_system(false);

  // Need normal filters since cloned RewriteDriver instances wouldn't
  // know about test-only stuff.
  options()->EnableFilter(RewriteOptions::kCombineCss);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  rewrite_driver()->AddFilters();

  InitResponseHeaders(
      "a.css", kContentTypeCss, " div { display: block;  }", 100);

  GoogleString url = Encode(kTestDomain, "cc", "0",
                            Encode("", "cf", "1", "a.css", "css"), "css");
  GoogleString url2 = Encode(kTestDomain, "cc", "0",
                             Encode("", "cf", "2", "a.css", "css"), "css");

  for (int run = 0; run < 3; ++run) {
    GoogleString content;
    ResponseHeaders headers;

    ServeResourceUrl(url, &content, &headers);
    // Note that this works only because the combiner fails and passes
    // through its input, which is the private cache-controled output
    // of rewrite_css
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ("div{display:block}", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
        << headers.ToString();
  }

  // Now also try the second version.
  GoogleString content;
  ResponseHeaders headers;
  ServeResourceUrl(url2, &content, &headers);
  EXPECT_EQ(HttpStatus::kOK, headers.status_code());
  EXPECT_STREQ("div{display:block}", content);
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
      << headers.ToString();
}

TEST_F(RewriteContextTest, Nested) {
  const GoogleString kRewrittenUrl = Encode(kTestDomain, "nf", "0",
                                            "c.css", "css");
  InitNestedFilter(kExpectNestedRewritesSucceed);
  InitResources();
  ValidateExpected("async3", CssLinkHref("c.css"), CssLinkHref(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(ServeResourceUrl(kRewrittenUrl, &rewritten_contents));
  EXPECT_EQ(StrCat(Encode(kTestDomain, "nf", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "nf", "0", "b.css", "css"), "\n"),
            rewritten_contents);
}

TEST_F(RewriteContextTest, NestedFailed) {
  // Make sure that the was_optimized() bit is not set when the nested
  // rewrite fails (which it will since it's already all caps)
  const GoogleString kRewrittenUrl = Encode(kTestDomain, "nf", "0",
                                            "d.css", "css");
  InitNestedFilter(kExpectNestedRewritesFail);
  InitResources();
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse("http://test.com/u.css", default_css_header,
                     "UPPERCASE");
  SetFetchResponse("http://test.com/d.css", default_css_header,
                     "u.css");
  ValidateExpected("nested-noop", CssLinkHref("d.css"),
                   CssLinkHref(kRewrittenUrl));
}

TEST_F(RewriteContextTest, NestedChained) {
  const GoogleString kRewrittenUrl = Encode(kTestDomain, "nf", "0",
                                            "c.css", "css");

  RewriteDriver* driver = rewrite_driver();
  NestedFilter* nf =
      new NestedFilter(driver, kExpectNestedRewritesSucceed);
  nf->set_chain(true);
  driver->AppendRewriteFilter(nf);
  driver->AddFilters();
  InitResources();
  ValidateExpected(
      "async_nest_chain", CssLinkHref("c.css"), CssLinkHref(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(ServeResourceUrl(kRewrittenUrl, &rewritten_contents));
  // We expect each URL twice since we have two nested jobs for it, and the
  // Harvest() just dumps each nested rewrites' slots.
  EXPECT_EQ(StrCat(Encode(kTestDomain, "nf", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "nf", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "nf", "0", "b.css", "css"), "\n",
                   Encode(kTestDomain, "nf", "0", "b.css", "css"), "\n"),
            rewritten_contents);
}

TEST_F(RewriteContextTest, CombinationRewrite) {
  InitCombiningFilter(0);
  InitResources();
  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     MultiUrl("a.css", "b.css"), "css");
  ValidateExpected(
      "combination_rewrite", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected(
      "combination_rewrite2",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());     // the output is all we need
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Proof-of-concept simulation of a Rewriter where delay is injected into
// the Rewrite flow.
TEST_F(RewriteContextTest, CombinationRewriteWithDelay) {
  InitCombiningFilter(kRewriteDelayMs);
  InitResources();
  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     MultiUrl("a.css", "b.css"), "css");
  ValidateNoChanges("xx", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(3, lru_cache()->num_inserts());  // partition+2 in, output not ready
  ClearStats();

  // The delay was too large so we were not able to complete the
  // Rewrite.  Now give it more time so it will complete.

  rewrite_driver()->BoundedWaitFor(
      RewriteDriver::kWaitForCompletion, kRewriteDelayMs);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());  // finally we cache the output.
  ClearStats();

  ValidateExpected(
      "combination_rewrite", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(0, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected(
      "combination_rewrite2",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());     // the output is all we need
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, CombinationFetch) {
  InitCombiningFilter(0);
  InitResources();

  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     MultiUrl("a.css", "b.css"), "css");

  // The input URLs are not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(5, lru_cache()->num_misses())
      << "2 misses for the output.  1 before we acquire the lock, "
      << "and one after we acquire the lock.  Then we miss on the metadata "
      << "and the two inputs.";

  EXPECT_EQ(4, lru_cache()->num_inserts()) << "2 inputs, 1 output, 1 metadata.";
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

// FYI: Takes ~70000 ms to run under Valgrind.
TEST_F(RewriteContextTest, FetchDeadlineTest) {
  // This tests that deadlines on fetches are functional.
  // This uses a combining filter with one input, as it has the needed delay
  // functionality.
  InitCombiningFilter(Timer::kMonthMs);
  InitResources();
  combining_filter_->set_prefix("|");

  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     "a.css", "css");

  GoogleString content;
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  // Should not get a |, as 1 month is way bigger than the rendering deadline.
  EXPECT_EQ(" a ", content);
  EXPECT_EQ(3, lru_cache()->num_inserts()); // input, output, metadata

  // However, due to mock scheduler auto-advance, it should finish
  // everything now, and be able to do it from cache.
  content.clear();
  ClearStats();
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ("| a ", content);

  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, FetchDeadlineTestBeforeDeadline) {
  // As above, but rewrite finishes quickly. This time we should see the |
  // immediately
  InitCombiningFilter(1 /*ms*/);
  InitResources();
  combining_filter_->set_prefix("|");

  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     "a.css", "css");

  GoogleString content;
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  // Should get a |, as 1 ms is smaller than the rendering deadline.
  EXPECT_EQ("| a ", content);

  // And of course it's nicely cached.
  content.clear();
  ClearStats();
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ("| a ", content);

  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, CombinationFetchMissing) {
  InitCombiningFilter(0);
  SetFetchFailOnUnexpected(false);
  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     MultiUrl("a.css", "b.css"), "css");
  EXPECT_FALSE(TryFetchResource(combined_url));
}

TEST_F(RewriteContextTest, CombinationFetchNestedMalformed) {
  // Fetch of a combination where nested URLs look like they were pagespeed-
  // produced, but actually have invalid filter ids.
  InitCombiningFilter(0);
  SetFetchFailOnUnexpected(false);
  GoogleString combined_url = Encode(
      kTestDomain, kCombiningFilterId, "0",
      MultiUrl("a.pagespeed.nosuchfilter.0.css",
               "b.pagespeed.nosuchfilter.0.css"), "css");
  EXPECT_FALSE(TryFetchResource(combined_url));
}

TEST_F(RewriteContextTest, CombinationFetchSeedsCache) {
  // Make sure that fetching a combination seeds cache for future rewrites
  // properly.
  InitCombiningFilter(0 /* no rewrite delay*/);
  InitResources();

  // First fetch it..
  GoogleString combined_url = Encode(kTestDomain, kCombiningFilterId, "0",
                                     MultiUrl("a.css", "b.css"), "css");
  GoogleString content;
  EXPECT_TRUE(ServeResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  ClearStats();

  // Then use from HTML.
  ValidateExpected(
      "hopefully_hashed",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Test that rewriting works correctly when input resource is loaded from disk.

TEST_F(RewriteContextTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kOnTheFlyResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 2 cache misses: one for the OutputPartitions, one for the input resource.
  EXPECT_EQ(2, lru_cache()->num_misses());
  // 1 cache insertion: resource mapping (CachedResult).
  // Output resource not stored in cache (because it's an on-the-fly resource).
  EXPECT_EQ(1, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}

TEST_F(RewriteContextTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kRewrittenResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 2 cache misses: one for the OutputPartitions, one for the input resource.
  EXPECT_EQ(2, lru_cache()->num_misses());
  // 2 cache insertion: resource mapping (CachedResult) and output resource.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}

namespace {

// Filter that blocks on Flush() in order to let an actual rewrite succeed
// while we are still 'parsing'.
class TestWaitFilter : public CommonFilter {
 public:
  TestWaitFilter(RewriteDriver* driver,
                 WorkerTestBase::SyncPoint* sync)
      : CommonFilter(driver), sync_(sync) {}
  virtual ~TestWaitFilter() {}

  virtual const char* Name() const { return "TestWait"; }
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(net_instaweb::HtmlElement*) {}
  virtual void EndElementImpl(net_instaweb::HtmlElement*) {}

  virtual void Flush() {
    sync_->Wait();
    driver()->set_externally_managed(true);
    CommonFilter::Flush();
  }

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(TestWaitFilter);
};

// Filter that wakes up a given sync point once its rewrite context is
// getting destroyed.
class TestNotifyFilter : public CommonFilter {
 public:
  class Context : public SingleRewriteContext {
   public:
    Context(RewriteDriver* driver, WorkerTestBase::SyncPoint* sync)
        : SingleRewriteContext(driver, NULL /* parent */,
                               NULL /* resource context*/),
          sync_(sync) {}

    virtual ~Context() {
      sync_->Notify();
    }

   protected:
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
    }

    virtual const char* id() const { return "testnotify"; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    WorkerTestBase::SyncPoint* sync_;
    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  TestNotifyFilter(RewriteDriver* driver, WorkerTestBase::SyncPoint* sync)
      : CommonFilter(driver), sync_(sync)  {}
  virtual ~TestNotifyFilter() {}

  virtual const char* Name() const {
    return "Notify";
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(net_instaweb::HtmlElement* element) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    if (href != NULL) {
      ResourcePtr input_resource(CreateInputResource(href->value()));
      ResourceSlotPtr slot(driver_->GetSlot(input_resource, element, href));
      Context* context = new Context(driver(), sync_);
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  }

  virtual void EndElementImpl(net_instaweb::HtmlElement*) {}

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(TestNotifyFilter);
};

}  // namespace

// Test to make sure we don't crash/delete a RewriteContext when it's completed
// while we're still writing. Not 100% guaranteed to crash, however, as
// we notice in ~TestNotifyFilter::Context and not when context is fully
// destroyed.
TEST_F(RewriteContextTest, UltraQuickRewrite) {
  // Turn on automatic memory management for now, to see if it tries to
  // auto-delete while still parsing. We turn it off inside
  // TestWaitFilter::Flush.
  rewrite_driver()->set_externally_managed(false);
  InitResources();

  WorkerTestBase::SyncPoint sync(resource_manager()->thread_system());
  rewrite_driver()->AppendOwnedPreRenderFilter(
      new TestNotifyFilter(rewrite_driver(), &sync));
  rewrite_driver()->AddOwnedPostRenderFilter(
      new TestWaitFilter(rewrite_driver(), &sync));
  resource_manager()->ComputeSignature(options());

  ValidateExpected("trimmable.quick", CssLinkHref("a.css"),
                   CssLinkHref("a.css"));
}

TEST_F(RewriteContextTest, RenderCompletesCacheAsync) {
  // Make sure we finish rendering fully even when cache is ultra-slow.
  SetCacheDelayUs(50 * kRewriteDeadlineMs * 1000);
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First time we're fetching, so we don't know.
  Parse("trimmable_async", CssLinkHref("a.css"));
  rewrite_driver()->WaitForCompletion();

  ValidateExpected("trimmable_async",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode(kTestDomain, "tw", "0", "a.css", "css")));
}

TEST_F(RewriteContextTest, TestFreshen) {
  FetcherUpdateDateHeaders();

  // Note that this must be >= kImplicitCacheTtlMs for freshening.
  const int kTtlMs = ResponseHeaders::kImplicitCacheTtlMs * 10;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  mock_timer()->AdvanceMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  InitResponseHeaders(kPath, kContentTypeCss, kDataIn,
                      kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance halfway from TTL. This should be an entire cache hit.
  mock_timer()->AdvanceMs(kTtlMs * 5 / 10);
  ValidateExpected("fully_hit",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance close to TTL and rewrite. We should see an extra fetch.
  // Also upload a version with a newer timestamp.
  InitResponseHeaders(kPath, kContentTypeCss, kDataIn,
                      kTtlMs / Timer::kSecondMs);
  mock_timer()->AdvanceMs(kTtlMs * 4 / 10);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Now advance past original expiration. While we cannot avoid trying to
  // rewrite as of now, we at least don't do an extra fetch at this point,
  // which hopefully shaves off tons of latency. In this test, we also
  // ensure this by using a wait fetcher, expecting that it will not be
  // asked to serve a fetch.
  // As we are also reusing rewrite results when contents did not change,
  // there is no second rewrite, either.
  SetupWaitFetcher();
  mock_timer()->AdvanceMs(kTtlMs * 4 / 10);
  ValidateExpected("freshen2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Make sure we do this or it will leak.
  CallFetcherCallbacks();
}

TEST_F(RewriteContextTest, TestReuse) {
  FetcherUpdateDateHeaders();

  // Test to make sure we are able to avoid rewrites when inputs don't
  // change even when they expire.

  const int kTtlMs = ResponseHeaders::kImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  mock_timer()->AdvanceMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  InitResponseHeaders(kPath, kContentTypeCss, kDataIn,
                      kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time way past when it was expired, or even when it'd live with
  // freshening.
  mock_timer()->AdvanceMs(kTtlMs * 10);

  // This should fetch, but can avoid calling the filter's Rewrite function.
  ValidateExpected("forward",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Advance some more --- make sure we fully hit from cache now (which
  // requires the previous operation to have updated it).
  mock_timer()->AdvanceMs(kTtlMs / 2);
  ValidateExpected("forward2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestFallbackOnFetchFails) {
  FetcherUpdateDateHeaders();
  InitTrimFilters(kRewrittenResource);
  resource_manager()->set_store_outputs_in_file_system(false);

  // Test to make sure we are able to serve stale resources if available when
  // the fetch fails.
  const int kTtlMs = ResponseHeaders::kImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const char kDataOut[] = "data";
  GoogleString rewritten_url = Encode(kTestDomain, "tw", "0",
                                      "test.css", "css");
  GoogleString response_content;
  ResponseHeaders response_headers;

  // Serve a 500 for the CSS file.
  ResponseHeaders bad_headers;
  bad_headers.set_first_line(1, 1, 500, "Internal Server Error");
  mock_url_fetcher()->SetResponse(AbsolutifyUrl(kPath), bad_headers, "");

  // First fetch. No rewriting happens since the fetch fails. We cache that the
  // fetch failed for kImplicitCacheTtlMs.
  ValidateNoChanges("initial_500", CssLinkHref(kPath));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      resource_manager()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();
  // Advance the timer by less than kImplicitCacheTtlMs. Since we remembered
  // that the fetch failed, we don't trigger a fetch for the CSS and don't
  // rewrite it either.
  mock_timer()->AdvanceMs(kTtlMs / 2);
  ValidateNoChanges("forward_500", CssLinkHref(kPath));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      resource_manager()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();

  // Advance the timer again so that the fetch failed is stale and update the
  // css response to a valid 200.
  mock_timer()->AdvanceMs(kTtlMs);
  InitResponseHeaders(kPath, kContentTypeCss, kDataIn,
                      kTtlMs / Timer::kSecondMs);

  // The resource is rewritten successfully.
  ValidateExpected("forward_200",
                   CssLinkHref(kPath),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Two cache inserts for the original and rewritten resource.
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_TRUE(ServeResourceUrl(rewritten_url, &response_content,
                               &response_headers));
  EXPECT_STREQ(kDataOut, response_content);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ(
      0,
      resource_manager()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();

  // Advance time way past when it was expired. Set the css response to a 500
  // again and delete the rewritten url from cache. We don't rewrite the html.
  // Note that we don't overwrite the stale response for the css and serve a
  // valid 200 response to the rewrriten resource.
  mock_timer()->AdvanceMs(kTtlMs * 10);
  lru_cache()->Delete(rewritten_url);
  mock_url_fetcher()->SetResponse(AbsolutifyUrl(kPath), bad_headers, "");

  ValidateNoChanges("forward_500_fallback_served", CssLinkHref(kPath));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      1,
      resource_manager()->rewrite_stats()->fallback_responses_served()->Get());

  response_headers.Clear();
  response_content.clear();
  EXPECT_TRUE(ServeResourceUrl(rewritten_url, &response_content,
                               &response_headers));
  EXPECT_STREQ(kDataOut, response_content);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());

  // Disable serving of stale resources and delete the rewritten resource from
  // cache. We don't rewrite the html. We insert the fetch failure into cache
  // and are unable to serve the rewritten resource.
  options()->ClearSignatureForTesting();
  options()->set_serve_stale_if_fetch_error(false);
  options()->ComputeSignature(hasher());

  ClearStats();
  lru_cache()->Delete(rewritten_url);
  ValidateNoChanges("forward_500_no_fallback", CssLinkHref(kPath));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      resource_manager()->rewrite_stats()->fallback_responses_served()->Get());

  response_headers.Clear();
  response_content.clear();
  EXPECT_FALSE(ServeResourceUrl(rewritten_url, &response_content,
                                &response_headers));
}

TEST_F(RewriteContextTest, TestReuseNotFastEnough) {
  // Make sure we handle deadline passing when trying to reuse properly.
  FetcherUpdateDateHeaders();

  const int kTtlMs = ResponseHeaders::kImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  mock_timer()->AdvanceMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  InitResponseHeaders(kPath, kContentTypeCss, kDataIn,
                      kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time way past when it was expired, or even when it'd live with
  // freshening.
  mock_timer()->AdvanceMs(kTtlMs * 10);

  // Make sure we can't check for freshening fast enough...
  SetupWaitFetcher();
  ValidateNoChanges("forward2.slow_fetch", CssLinkHref(kPath));

  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CallFetcherCallbacks();
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Next time should be fine again, though.
  mock_timer()->AdvanceMs(kTtlMs / 2);
  ValidateExpected("forward2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode(kTestDomain, "tw", "0",
                                      "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

// Test resource update behavior.
class ResourceUpdateTest : public RewriteContextTest {
 protected:
  static const char kOriginalUrl[];

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
    const GoogleString& rewritten_url = AbsolutifyUrl(css_urls[0]);

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
    return RewriteResource(id, CssLinkHref(kOriginalUrl));
  }
};

const char ResourceUpdateTest::kOriginalUrl[] = "a.css";

// Test to make sure that 404's expire.
TEST_F(ResourceUpdateTest, TestExpire404) {
  InitTrimFilters(kRewrittenResource);

  // First, set a 404.
  SetFetchResponse404(kOriginalUrl);

  // Trying to rewrite it should not do anything..
  ValidateNoChanges("404", CssLinkHref(kOriginalUrl));

  // Now move forward 20 years and upload a new version. We should
  // be ready to optimize at that point.
  // "And thus Moses wandered the desert for only 20 years, because of a
  // limitation in the implementation of time_t."
  mock_timer()->AdvanceMs(20 * Timer::kYearMs);
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", 100);
  EXPECT_EQ("init", RewriteSingleResource("200"));
}

TEST_F(ResourceUpdateTest, OnTheFly) {
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  // TODO(sligocki): Why are we rewriting twice here?
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, Rewritten) {
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite should immediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite should immediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

class CombineResourceUpdateTest : public ResourceUpdateTest {
 protected:
  GoogleString CombineResources(const StringPiece& id) {
    return RewriteResource(
        id, StrCat(CssLinkHref("web/a.css"), CssLinkHref("file/b.css"),
                   CssLinkHref("web/c.css"), CssLinkHref("file/d.css")));
  }
};

TEST_F(CombineResourceUpdateTest, CombineDifferentTTLs) {
  // Initialize system.
  InitCombiningFilter(0);
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
  // Note that we stat each file as we load it in.
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  mock_timer()->AdvanceMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  EXPECT_EQ(" a1  b1  c1  d1 ", CombineResources("advance_time"));
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c2 ", kShortTtlMs / 1000);
  WriteFile("/test/d.css", " d2 ");
  // File-based resources should be updated, but web-based ones still cached.
  EXPECT_EQ(" a1  b2  c1  d2 ", CombineResources("stale_content"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Read both files.
  // 2 reads + stat of b
  EXPECT_EQ(3, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  mock_timer()->AdvanceMs(kShortTtlMs);
  // All but long TTL UrlInputResource should be updated.
  EXPECT_EQ(" a1  b2  c2  d2 ", CombineResources("short_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // One expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // 2 file reads + stat of b, which we get to as a has long TTL,
  // as well as as of d (for figuring out revalidation strategy).
  EXPECT_EQ(4, file_system()->num_input_file_stats());
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  mock_timer()->AdvanceMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  EXPECT_EQ(" a2  b2  c2  d2 ", CombineResources("all_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // Both expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // 2 read-induced stats, 2 stats to figure out how to deal with
  // c + d for invalidation.
  EXPECT_EQ(4, file_system()->num_input_file_stats());
  ClearStats();
}

class NestedResourceUpdateTest : public ResourceUpdateTest {
 protected:
  StringVector RewriteNestedResources(const StringPiece& id) {
    // Rewrite everything and fetch the rewritten main resource.
    GoogleString rewritten_list = RewriteResource(id, CssLinkHref("main.txt"));

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

TEST_F(NestedResourceUpdateTest, TestExpireNested404) {
  UseMd5Hasher();
  InitNestedFilter(kExpectNestedRewritesFail);

  const int64 kDecadeMs = 10 * Timer::kYearMs;

  // Have the nested one have a 404...
  const GoogleString kOutUrl = Encode(kTestDomain, "nf", "sdUklQf3sx",
                                      "main.txt", "css");
  InitResponseHeaders("http://test.com/main.txt", kContentTypeCss,
                      "a.css\n", 4 * kDecadeMs / 1000);
  SetFetchResponse404("a.css");

  ValidateExpected("nested_404", CssLinkHref("main.txt"), CssLinkHref(kOutUrl));
  GoogleString contents;
  EXPECT_TRUE(ServeResourceUrl(kOutUrl, &contents));
  EXPECT_EQ("http://test.com/a.css\n", contents);

  // Determine if we're using the TestUrlNamer, for the hash later.
  bool test_url_namer = factory_->use_test_url_namer();

  // Now move forward two decades, and upload a new version. We should
  // be ready to optimize at that point, but input should not be expired.
  mock_timer()->AdvanceMs(2 * kDecadeMs);
  InitResponseHeaders("a.css", kContentTypeCss, " lowercase ", 100);
  ReconfigureNestedFilter(kExpectNestedRewritesSucceed);
  const GoogleString kFullOutUrl =
      Encode(kTestDomain, "nf",
             test_url_namer ? "ddCLaqHYwR" : "wtz1oZ56O0",
             "main.txt", "css");
  const GoogleString kInnerUrl = StrCat(Encode(kTestDomain, "nf", "N4LKMOq9ms",
                                               "a.css", "css"), "\n");
  ValidateExpected("nested_404", CssLinkHref("main.txt"),
                   CssLinkHref(kFullOutUrl));
  EXPECT_TRUE(ServeResourceUrl(kFullOutUrl, &contents));
  EXPECT_EQ(kInnerUrl, contents);
  EXPECT_TRUE(ServeResourceUrl(kInnerUrl, &contents));
  EXPECT_EQ(" LOWERCASE ", contents);
}

TEST_F(NestedResourceUpdateTest, NestedDifferentTTLs) {
  // Initialize system.
  InitNestedFilter(kExpectNestedRewritesSucceed);
  options()->file_load_policy()->Associate("http://test.com/file/", "/test/");

  // Initialize resources.
  const int64 kExtraLongTtlMs = 10 * Timer::kMonthMs;
  const int64 kLongTtlMs = 1 * Timer::kMonthMs;
  const int64 kShortTtlMs = 1 * Timer::kMinuteMs;
  InitResponseHeaders("http://test.com/main.txt", kContentTypeCss,
                      "web/a.css\n"
                      "file/b.css\n"
                      "web/c.css\n", kExtraLongTtlMs / 1000);
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a1 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b1 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c1 ", kShortTtlMs / 1000);

  ClearStats();
  // 1) Initial combined resource.
  StringVector result_vector;
  result_vector = RewriteNestedResources("first_load");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());
  EXPECT_EQ(3, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  // {a,b,c}.css.pagespeed.nf.HASH.css and b.css
  EXPECT_EQ(4, file_system()->num_input_file_opens());
  // Loading b.css the first time.
  EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  mock_timer()->AdvanceMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  result_vector = RewriteNestedResources("advance_time");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(0, nested_filter_->num_top_rewrites());
  EXPECT_EQ(0, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // re-checked b.
  EXPECT_EQ(1, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  InitResponseHeaders("http://test.com/web/a.css", kContentTypeCss,
                      " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  InitResponseHeaders("http://test.com/web/c.css", kContentTypeCss,
                      " c2 ", kShortTtlMs / 1000);
  // File-based resources should be updated, but web-based ones still cached.
  result_vector = RewriteNestedResources("stale_content");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  EXPECT_EQ(1, nested_filter_->num_sub_rewrites());  // b.css
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // b.css, b.css.pagespeed.nf.HASH.css
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // The stats here are:
  // 1) Stat b.css to figure out if top-level rewrite is valid.
  // 2) Stat b.css to figure out if nested rewrite is valid.
  // 3) Stat b.css to figure out its time on loading it.
  EXPECT_EQ(3, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  mock_timer()->AdvanceMs(kShortTtlMs);
  // All but long TTL UrlInputResource should be updated.
  result_vector = RewriteNestedResources("short_updated");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  EXPECT_EQ(1, nested_filter_->num_sub_rewrites());  // c.css
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // c.css
  EXPECT_EQ(1, file_system()->num_input_file_opens());  // rewritten c.css
  EXPECT_EQ(1, file_system()->num_input_file_stats());  // verify b.css
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  mock_timer()->AdvanceMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  result_vector = RewriteNestedResources("short_updated");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A2 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated

  // a.css; and c.css gets rewritten while unchanged as it is an on-the-fly
  // resource.
  EXPECT_EQ(2, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // a.css, c.css
  EXPECT_EQ(1, file_system()->num_input_file_opens());  // rewritten a.css
  EXPECT_EQ(1, file_system()->num_input_file_stats());  // check b.css (nested)
  ClearStats();
}

}  // namespace net_instaweb
