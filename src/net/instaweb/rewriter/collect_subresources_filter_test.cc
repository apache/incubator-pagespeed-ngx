/*
 * Copyright 2012 Google Inc.
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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/collect_subresources_filter.h"

#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {
namespace {
const int64 kOriginTtlMs = 12 * Timer::kMinuteMs;
const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
}

// DummyFilter sets a resource as optmimized even when it was not optimized.
class DummyFilter : public CommonFilter {
 public:
  class Context : public SingleRewriteContext {
   public:
    explicit Context(RewriteDriver* driver)
        : SingleRewriteContext(driver, NULL /* parent */,
                               NULL /* resource context*/) {}

    virtual ~Context() {}

   protected:
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      ResourceSlot* resource_slot = slot(0).get();
      resource_slot->set_was_optimized(true);
      RewriteDone(kRewriteFailed, 0);
    }

    virtual void Render() {
      ResourceSlot* resource_slot = slot(0).get();
      resource_slot->set_was_optimized(true);
    }

    virtual const char* id() const { return "dummy_filter"; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  explicit DummyFilter(RewriteDriver* driver)
      : CommonFilter(driver) {}
  virtual ~DummyFilter() {}

  virtual const char* Name() const {
    return "DummyFilter";
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    if (href != NULL) {
      ResourcePtr input_resource(CreateInputResource(
          href->DecodedValueOrNull()));
      ResourceSlotPtr slot(driver_->GetSlot(input_resource, element, href));
      Context* context = new Context(driver());
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  }

  virtual void EndElementImpl(HtmlElement* element) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyFilter);
};


class CollectSubresourcesFilterTest : public RewriteTestBase {
 public:
  CollectSubresourcesFilterTest() : collect_subresources_filter_(NULL) {}

 protected:
  void InitFilters(bool enable_dummy_filter) {
    options()->EnableExtendCacheFilters();
    if (enable_dummy_filter) {
      rewrite_driver()->AppendOwnedPreRenderFilter(
              new DummyFilter(rewrite_driver()));
    }
    collect_subresources_filter_ = new CollectSubresourcesFilter(
        rewrite_driver());
    rewrite_driver()->AddFilters();
    rewrite_driver()->AppendOwnedPreRenderFilter(
        collect_subresources_filter_);
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    rewrite_driver()->set_user_agent("prefetch_link_rel_subresource");
    SetResponseWithDefaultHeaders("http://test.com/a.css", kContentTypeCss,
                                  ".yellow {background-color: yellow;}",
                                  kOriginTtlMs);
    SetResponseWithDefaultHeaders("http://test.com/b.js",
                                  kContentTypeJavascript, kJsData,
                                  kOriginTtlMs);
    SetResponseWithDefaultHeaders("http://test.com/c.css", kContentTypeCss,
                                  ".yellow {background-color: yellow;}",
                                  kOriginTtlMs);
    SetResponseWithDefaultHeaders("http://test.com/d.js",
                                  kContentTypeJavascript, kJsData,
                                  kOriginTtlMs);
  }

  CollectSubresourcesFilter* collect_subresources_filter() {
    return collect_subresources_filter_;
  }

 private:
  CollectSubresourcesFilter* collect_subresources_filter_;

  DISALLOW_COPY_AND_ASSIGN(CollectSubresourcesFilterTest);
};

TEST_F(CollectSubresourcesFilterTest, CollectSubresourcesFilter) {
  InitFilters(false);
  GoogleString html_ip =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"c.css\"/>"
        "<script src=\"d.js\"></script>"
      "</body>";

  Parse("not_flushed_early", html_ip);
  FlushEarlyInfo* flush_early_info = rewrite_driver()->flush_early_info();
  collect_subresources_filter()->AddSubresourcesToFlushEarlyInfo(
      flush_early_info);
  // CollectSubresourcesFilter should have populated the flush_early_info
  // proto with the appropriate subresources.
  EXPECT_EQ(3, flush_early_info->subresource_size());
  EXPECT_EQ("http://test.com/a.css.pagespeed.ce.0.css",
            flush_early_info->subresource(0).rewritten_url());
  EXPECT_EQ("http://test.com/b.js.pagespeed.ce.0.js",
            flush_early_info->subresource(1).rewritten_url());
  EXPECT_EQ("http://test.com/c.css.pagespeed.ce.0.css",
            flush_early_info->subresource(2).rewritten_url());
  EXPECT_EQ(CSS, flush_early_info->subresource(0).content_type());
  EXPECT_EQ(JAVASCRIPT, flush_early_info->subresource(1).content_type());
  EXPECT_EQ(CSS, flush_early_info->subresource(2).content_type());
  // Calling Parse once more so that we hit only Render without hitting
  // RewriteSingle.
  rewrite_driver()->Clear();
  Parse("flushed_early", html_ip);
  flush_early_info = rewrite_driver()->flush_early_info();
  collect_subresources_filter()->AddSubresourcesToFlushEarlyInfo(
      flush_early_info);
  EXPECT_EQ(3, flush_early_info->subresource_size());
}

TEST_F(CollectSubresourcesFilterTest, HtmlHasRewrittenUrl) {
  InitFilters(true);
  GoogleString html_ip =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" "
        "href=\"http://test.com/a.css.pagespeed.ce.0.css\"/>"
      "</head>"
      "<body></body>";

  Parse("not_flushed_early", html_ip);
  FlushEarlyInfo* flush_early_info = rewrite_driver()->flush_early_info();
  flush_early_info = rewrite_driver()->flush_early_info();
  collect_subresources_filter()->AddSubresourcesToFlushEarlyInfo(
      flush_early_info);
  EXPECT_EQ(1, flush_early_info->subresource_size());
  rewrite_driver()->Clear();
  Parse("flushed_early", html_ip);
  flush_early_info = rewrite_driver()->flush_early_info();
  collect_subresources_filter()->AddSubresourcesToFlushEarlyInfo(
      flush_early_info);
  EXPECT_EQ(1, flush_early_info->subresource_size());
}

}  // namespace net_instaweb
