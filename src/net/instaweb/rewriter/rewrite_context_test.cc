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
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace {

const char kTrimWhitespaceFilterId[] = "tw";

}  // namespace

namespace net_instaweb {

// Simple test filter just trims whitespace from the input resource.
//
// TODO(jmarantz): take as many lines of code out of this simplistic
// example as possible.  The actual rewrite code in this case is
// just one call to TrimWhitespace -- why all the other junk?
class TrimWhitespaceRewriter : public SimpleTextFilter::Rewriter {
 public:
  static SimpleTextFilter* MakeFilter(RewriteDriver* driver) {
    return new SimpleTextFilter(new TrimWhitespaceRewriter, driver);
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
  virtual const char* id() const { return kTrimWhitespaceFilterId; }
  virtual const char* Name() const { return "TrimWhitespace"; }
};

class RewriteContextTest : public ResourceManagerTestBase {
 protected:
  virtual bool AddBody() const { return false; }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    InitTrimFilter(&rewrite_driver_);
    InitTrimFilter(&other_rewrite_driver_);
  }

  void InitTrimFilter(RewriteDriver* rewrite_driver) {
    rewrite_driver->SetAsynchronousRewrites(true);
    rewrite_driver->AddRewriteFilter(
        TrimWhitespaceRewriter::MakeFilter(&rewrite_driver_));
    rewrite_driver->AddFilters();
  }

  GoogleString CssLink(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }
};

TEST_F(RewriteContextTest, Trim) {
  ResponseHeaders default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse("http://test.com/a.css", default_css_header,
                                " a ");  // trimmable
  mock_url_fetcher_.SetResponse("http://test.com/b.css", default_css_header,
                                "b");    // not trimmable
  ValidateExpected("trimmable", CssLink("a.css"),
                   CssLink("http://test.com/a.css.pagespeed.tw.0.css"));
  ValidateNoChanges("no_trimmable", CssLink("b.css"));
}

TEST_F(RewriteContextTest, FetchColdCache) {
  TestServeFiles(&kContentTypeCss, kTrimWhitespaceFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");

  // TODO(jmarantz): test failure cases (failed to fetch, failed to rewrite).
}

}  // namespace net_instaweb
