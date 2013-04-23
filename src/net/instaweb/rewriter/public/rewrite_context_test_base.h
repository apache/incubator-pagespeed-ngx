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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/worker_test_base.h"


namespace net_instaweb {

class CachedResult;
class MessageHandler;
class MockScheduler;
class OutputPartitions;
class OutputResource;
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
                           ServerContext* server_context);
  virtual HtmlElement::Attribute* FindResourceAttribute(HtmlElement* element);
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
  virtual ~TrimWhitespaceSyncFilter();

  virtual void StartElementImpl(HtmlElement* element);
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
                           ServerContext* server_context) {
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
  // For use with NestedFilter constructor
  static const bool kExpectNestedRewritesSucceed = true;
  static const bool kExpectNestedRewritesFail = false;

  static const char kFilterId[];

  NestedFilter(RewriteDriver* driver, SimpleTextFilter* upper_filter,
               UpperCaseRewriter* upper_rewriter, bool expected_nested_result)
      : RewriteFilter(driver), upper_filter_(upper_filter),
        upper_rewriter_(upper_rewriter), chain_(false),
        check_nested_rewrite_result_(true),
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

  void set_check_nested_rewrite_result(bool x) {
    check_nested_rewrite_result_ = x;
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
        const ResourcePtr& input, const OutputResourcePtr& output);
    virtual void Harvest();

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

  void StartElementImpl(HtmlElement* element);
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

  // Whether to check the result of the nested rewrites.
  bool check_nested_rewrite_result_;
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
                  int64 rewrite_delay_ms);
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
            MockScheduler* scheduler);

    void AddElement(HtmlElement* element, HtmlElement::Attribute* href,
                    const ResourcePtr& resource) {
      ResourceSlotPtr slot(Driver()->GetSlot(resource, element, href));
      AddSlot(slot);
    }

   protected:
    virtual bool Partition(OutputPartitions* partitions,
                           OutputResourceVector* outputs);

    virtual void Rewrite(int partition_index,
                         CachedResult* partition,
                         const OutputResourcePtr& output);
    virtual bool OptimizationOnly() const {
      return filter_->optimization_only();
    }

    void DoRewrite(int partition_index,
                   CachedResult* partition,
                   OutputResourcePtr output);
    virtual void Render();
    virtual void WillNotRender();
    virtual void Cancel();
    void DisableRemovedSlots(CachedResult* partition);
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
  virtual void StartElementImpl(HtmlElement* element);
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

  int num_rewrites() const { return num_rewrites_; }
  int num_render() const { return num_render_; }
  int num_will_not_render() const { return num_will_not_render_; }
  int num_cancel() const { return num_cancel_; }

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
  int num_render_;
  int num_will_not_render_;
  int num_cancel_;
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

class RewriteContextTestBase : public RewriteTestBase {
 protected:
  static const int64 kRewriteDeadlineMs = 20;

  // Use a TTL value other than the implicit value, so we are sure we are using
  // the original TTL value.
  static const int64 kOriginTtlMs = 12 * Timer::kMinuteMs;
  // An TTL value that is lower than the default implicit TTL value (300
  // seconds).
  static const int64 kLowOriginTtlMs = 5 * Timer::kSecondMs;

  // Use a TTL value other than the implicit value, so we are sure we are using
  // the original TTL value.
  GoogleString OriginTtlMaxAge() {
    return StrCat("max-age=", Integer64ToString(
        kOriginTtlMs / Timer::kSecondMs));
  }

  RewriteContextTestBase(
      std::pair<TestRewriteDriverFactory*, TestRewriteDriverFactory*> factories)
      : RewriteTestBase(factories) {}
  RewriteContextTestBase() {}
  virtual ~RewriteContextTestBase();

  virtual void SetUp();
  virtual void TearDown();
  virtual bool AddBody() const { return false; }
  virtual void ClearStats();

  void InitResources() { InitResourcesToDomain(kTestDomain); }
  void InitResourcesToDomain(const char* domain);
  void InitTrimFilters(OutputResourceKind kind);
  void InitUpperFilter(OutputResourceKind kind, RewriteDriver* rewrite_driver);
  void InitCombiningFilter(int64 rewrite_delay_ms);
  void InitNestedFilter(bool expected_nested_rewrite_result);

  TrimWhitespaceRewriter* trim_filter_;
  TrimWhitespaceRewriter* other_trim_filter_;
  CombiningFilter* combining_filter_;
  NestedFilter* nested_filter_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_TEST_BASE_H_
