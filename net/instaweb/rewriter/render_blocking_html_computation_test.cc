/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/render_blocking_html_computation.h"

#include "base/logging.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"

namespace net_instaweb {

namespace {

// TODO(morlovich): Duped from CommonFilterTest
class CountingFilter : public CommonFilter {
 public:
  explicit CountingFilter(RewriteDriver* driver) : CommonFilter(driver),
                                                   start_doc_calls_(0),
                                                   start_element_calls_(0),
                                                   end_element_calls_(0) {}

  virtual void StartDocumentImpl() { ++start_doc_calls_; }
  virtual void StartElementImpl(HtmlElement* element) {
    ++start_element_calls_;
  }
  virtual void EndElementImpl(HtmlElement* element) { ++end_element_calls_; }

  virtual const char* Name() const { return "CountingFilter"; }

  int start_doc_calls_;
  int start_element_calls_;
  int end_element_calls_;
};

const char kPage[] = "page.html";
const char kContent[] = "<a><b><c></c></b></a>";

class RenderBlockingHtmlComputationTest : public RewriteTestBase {
 public:
  void OnlyCallFetcherCallbacks() {
    // Normal CallFetcherCallbacks() tries to WaitForCompletion and do all
    // sorts of other similar stuff that makes it unusable during parsing,
    // so we work with the wait fetcher directly.
    factory()->wait_url_async_fetcher()->CallCallbacks();
  }

 protected:
  friend class CountingClientFilter;
  friend class CountingComputation;

  RenderBlockingHtmlComputationTest()
      : done_(false), result_(false), start_doc_calls_(0),
        start_element_calls_(0), end_element_calls_(0) {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->ComputeSignature();
    mutex_.reset(server_context()->thread_system()->NewMutex());
    cond_.reset(mutex_->NewCondvar());
    SetResponseWithDefaultHeaders(kPage, kContentTypeHtml, kContent, 100);

    // Permit RenderBlockingHtmlComputation to make resources if we're not also
    // running a document through a driver.
    SetBaseUrlForFetch(kTestDomain);
  }

  void Done(bool result, int start_doc_calls,
            int start_element_calls, int end_element_calls) {
    ScopedMutex hold(mutex_.get());
    done_ = true;
    result_ = result;
    start_doc_calls_ = start_doc_calls;
    start_element_calls_ = start_element_calls;
    end_element_calls_ = end_element_calls;
    cond_->Signal();
  }

  // Returns success/failure.
  bool WaitForDone() {
    ScopedMutex hold(mutex_.get());
    while (!done_) {
      cond_->Wait();
    }
    done_ = false;  // Reset for further runs.
    return result_;
  }

  bool done() const {
    ScopedMutex hold(mutex_.get());
    return done_;
  }

  bool result() const {
    ScopedMutex hold(mutex_.get());
    return result_;
  }

  int start_doc_calls() const {
    ScopedMutex hold(mutex_.get());
    return start_doc_calls_;
  }

  int start_element_calls() const {
    ScopedMutex hold(mutex_.get());
    return start_element_calls_;
  }

  int end_element_calls() const {
    ScopedMutex hold(mutex_.get());
    return end_element_calls_;
  }

  bool done_ GUARDED_BY(mutex_);
  bool result_ GUARDED_BY(mutex_);
  int start_doc_calls_ GUARDED_BY(mutex_);
  int start_element_calls_ GUARDED_BY(mutex_);
  int end_element_calls_ GUARDED_BY(mutex_);
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> cond_;
};

class CountingComputation : public RenderBlockingHtmlComputation {
 public:
  CountingComputation(RenderBlockingHtmlComputationTest* fixture,
                      RewriteDriver* parent_driver)
      : RenderBlockingHtmlComputation(parent_driver),
        filter_(NULL) , fixture_(fixture) {}

 protected:
  virtual void SetupFilters(RewriteDriver* child_driver) {
    filter_ = new CountingFilter(child_driver);
    child_driver->AppendOwnedPreRenderFilter(filter_);
  }

  virtual void Done(bool success) {
    if (success) {
      CHECK(filter_ != NULL);
      fixture_->Done(true,
                     filter_->start_doc_calls_,
                     filter_->start_element_calls_,
                     filter_->end_element_calls_);
    } else {
      fixture_->Done(false, 0, 0, 0);
    }
  }

 private:
  // Owned indirectly by base class.
  CountingFilter* filter_;
  RenderBlockingHtmlComputationTest* fixture_;
};


TEST_F(RenderBlockingHtmlComputationTest, ErrorPaths) {
  SetFetchFailOnUnexpected(false);

  scoped_ptr<CountingComputation> background_computation(
    new CountingComputation(this, rewrite_driver()));
  background_computation.release()->Compute("fekrfkek://wkewkl");
  EXPECT_FALSE(WaitForDone());

  background_computation.reset(
    new CountingComputation(this, rewrite_driver()));
  background_computation.release()->Compute(StrCat(kPage, "404"));
  EXPECT_FALSE(WaitForDone());

  EXPECT_EQ(0, start_doc_calls_);
  EXPECT_EQ(0, start_element_calls_);
  EXPECT_EQ(0, end_element_calls_);
}

TEST_F(RenderBlockingHtmlComputationTest, BasicOperation) {
  // Makes sure we can run a basic computation through in a simplest case.
  scoped_ptr<CountingComputation> background_computation(
      new CountingComputation(this, rewrite_driver()));
  background_computation.release()->Compute(AbsolutifyUrl(kPage));
  EXPECT_TRUE(WaitForDone());
  EXPECT_EQ(1, start_doc_calls_);
  EXPECT_EQ(3, start_element_calls_);
  EXPECT_EQ(3, end_element_calls_);
}

class CountingClientFilter : public CommonFilter {
 public:
  CountingClientFilter(RewriteDriver* driver,
                       RenderBlockingHtmlComputationTest* fixture)
        : CommonFilter(driver), first_flush_window_(true), fixture_(fixture) {}

  virtual void StartDocumentImpl() {
    first_flush_window_ = true;

    // In real case this would be conditional on something like entry in
    // pcache missing.
    scoped_ptr<CountingComputation> background_computation(
        new CountingComputation(fixture_, driver()));
    background_computation.release()->Compute(driver()->url());
  }

  virtual void RenderDone() {
    // Only care about the first one.
    if (!first_flush_window_) {
      return;
    }

    EXPECT_TRUE(fixture_->done());
    EXPECT_TRUE(fixture_->result());

    // Test computer saved results to fixture --- real one would likely save
    // it to here.
    GoogleString stats = StringPrintf("docs=%d, open=%d, close=%d",
                                      fixture_->start_doc_calls(),
                                      fixture_->start_element_calls(),
                                      fixture_->end_element_calls());
    InsertNodeAtBodyEnd(driver()->NewCommentNode(NULL, stats));

    first_flush_window_ = false;
  }

  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "CountingClientFilter"; }

 private:
  bool first_flush_window_;
  RenderBlockingHtmlComputationTest* fixture_;
  DISALLOW_COPY_AND_ASSIGN(CountingClientFilter);
};

TEST_F(RenderBlockingHtmlComputationTest, WithFilter) {
  // A bit more like the expected usage scenario, with a filter invoking
  // RenderBlockingHtmlComputation as a background computation.
  scoped_ptr<CountingClientFilter> parent_filter(
      new CountingClientFilter(rewrite_driver(), this));
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(parent_filter.release());
  ValidateExpected("page",  // it appends .html itself.
                   "<html>will use fetched content.</html>",
                   "<html>will use fetched content."
                       "<!--docs=1, open=3, close=3--></html>");
}

TEST_F(RenderBlockingHtmlComputationTest, WithFilterAndWaiting) {
  // Actually run this asynchronously --- also checking that we actually
  // do wait for the fetch to happen.
  SetupWaitFetcher();
  scoped_ptr<CountingClientFilter> parent_filter(
      new CountingClientFilter(rewrite_driver(), this));
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(parent_filter.release());
  int64 start_us = timer()->NowUs();
  mock_scheduler()->AddAlarmAtUs(
      start_us + 50 * Timer::kMsUs,
      MakeFunction(
          static_cast<RenderBlockingHtmlComputationTest*>(this),
          &RenderBlockingHtmlComputationTest::OnlyCallFetcherCallbacks));
  ValidateExpected("page",  // it appends .html itself.
                   "<html>will use fetched content.</html>",
                   "<html>will use fetched content."
                       "<!--docs=1, open=3, close=3--></html>");
  EXPECT_EQ(50 * Timer::kMsUs, timer()->NowUs() - start_us);
}

}  // namespace

}  // namespace net_instaweb
