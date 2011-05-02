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
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"

namespace net_instaweb {

// Simple test filter just trims whitespace from the input resource.
//
// TODO(jmarantz): take as many lines of code out of this simplistic
// example as possible.  The actual rewrite code in this case is
// just one call to TrimWhitespace -- why all the other junk?
class TrimWhitespaceFilter : public CommonFilter {
 public:
  explicit TrimWhitespaceFilter(RewriteDriver* driver) : CommonFilter(driver) {}
  virtual ~TrimWhitespaceFilter() {}
  virtual const char* Name() const { return "TrimWhitespace"; }

  class TrimRewriteContext : public SingleRewriteContext {
   public:
    TrimRewriteContext(RewriteDriver* driver,
                       const ResourceSlotPtr& slot)
        : SingleRewriteContext(driver, slot, NULL) {
    }

    // TODO(jmarantz): consider changing this interface to take a string in/out
    // and provide interfaces to override origin_expire via a method or an
    // extra arg to be modified.
    virtual RewriteSingleResourceFilter::RewriteResult Rewrite(
        const Resource* input_resource, OutputResource* output_resource) {
      RewriteSingleResourceFilter::RewriteResult result =
          RewriteSingleResourceFilter::kRewriteFailed;
      GoogleString rewritten;
      TrimWhitespace(input_resource->contents(), &rewritten);
      if (rewritten != input_resource->contents()) {
        ResourceManager* rm = resource_manager();
        MessageHandler* message_handler = rm->message_handler();
        int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
        if (rm->Write(HttpStatus::kOK, rewritten, output_resource,
                      origin_expire_time_ms, message_handler)) {
          result = RewriteSingleResourceFilter::kRewriteOk;
        }
      }
      return result;
    }

   protected:
    virtual const char* id() const { return "tw"; }
  };

  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}

  // TODO(jmarantz): Consider a new interface which provides pre-populated
  // slots, perhaps based on a virtual method provided by the filter to
  // indicate element/attr patterns of interest.
  virtual void StartElementImpl(HtmlElement* element) {
    if (element->keyword() == HtmlName::kLink) {
      HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kHref);
      if (attr != NULL) {
        ResourcePtr resource = CreateInputResource(attr->value());
        if (resource.get() != NULL) {
          ResourceSlotPtr slot(driver_->GetSlot(resource, element, attr));
          driver_->InitiateRewrite(new TrimRewriteContext(driver_, slot));
        }
      }
    }
  }
};

class RewriteContextTest : public ResourceManagerTestBase {
 protected:
  virtual bool AddBody() const { return false; }

  GoogleString CssLink(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    rewrite_driver_.SetAsynchronousRewrites(true);
  }
};

TEST_F(RewriteContextTest, Trim) {
  rewrite_driver_.AddOwnedFilter(new TrimWhitespaceFilter(&rewrite_driver_));
  rewrite_driver_.AddFilters();
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

}  // namespace net_instaweb
