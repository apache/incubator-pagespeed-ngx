// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/css_inline_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/inline_rewrite_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class MessageHandler;

class CssInlineFilter::Context : public InlineRewriteContext {
 public:
  Context(CssInlineFilter* filter, const GoogleUrl& base_url,
          HtmlElement* element, HtmlElement::Attribute* src)
      : InlineRewriteContext(filter, element, src),
        filter_(filter) {
    base_url_.Reset(base_url);
  }

  virtual bool ShouldInline(const StringPiece& input) const {
    return filter_->ShouldInline(input);
  }

  virtual void RenderInline(
      const ResourcePtr& resource, const StringPiece& text,
      HtmlElement* element) {
    filter_->RenderInline(resource, base_url_, text, element);
  }

  virtual const char* id() const {
    // Unlike filters with output resources, which use their ID as part of URLs
    // they make, we are not constrained to 2 characters, so we make our
    // name (used for our cache key) nice and long so at not to worry about
    // someone else using it.
    return "css_inline";
  }

 private:
  CssInlineFilter* filter_;
  GoogleUrl base_url_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

CssInlineFilter::CssInlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      size_threshold_bytes_(driver->options()->css_inline_max_bytes()) {}

void CssInlineFilter::StartDocumentImpl() {
}

CssInlineFilter::~CssInlineFilter() {}

void CssInlineFilter::EndElementImpl(HtmlElement* element) {
  if ((element->keyword() == HtmlName::kLink) &&
      !driver_->HasChildrenInFlushWindow(element)) {
    const char* rel = element->AttributeValue(HtmlName::kRel);
    if (rel == NULL || strcmp(rel, "stylesheet") != 0) {
      return;
    }

    // If the link tag has a media attribute whose value isn't "all", don't
    // inline.  (Note that "all" is equivalent to having no media attribute;
    // see http://www.w3.org/TR/html5/semantics.html#the-style-element)
    const char* media = element->AttributeValue(HtmlName::kMedia);
    if (media != NULL && strcmp(media, "all") != 0) {
      return;
    }

    // Get the URL where the external script is stored
    HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kHref);
    if (attr == NULL || attr->value() == NULL) {
      return;  // We obviously can't inline if the URL isn't there.
    }

    if (HasAsyncFlow()) {
      (new Context(this, base_url(), element, attr))->Initiate();
    } else {
      // Make sure we're not moving across domains -- CSS can potentially
      // contain Javascript expressions.
      // TODO(jmaessen): Is the domain lawyer policy the appropriate one here?
      // Or do we still have to check for strict domain equivalence?
      // If so, add an inline-in-page policy to domainlawyer in some form,
      // as we make a similar policy decision in js_inline_filter.
      ResourcePtr resource(CreateInputResourceAndReadIfCached(attr->value()));
      if ((resource.get() == NULL) || !resource->ContentsValid()) {
        return;
      }

      StringPiece contents = resource->contents();
      if (ShouldInline(contents)) {
        RenderInline(resource, base_url(), contents, element);
      }
    }
  }
}

bool CssInlineFilter::ShouldInline(const StringPiece& contents) const {
  if (contents.size() > size_threshold_bytes_) {
    return false;
  }

  // Check that the file does not have imports, which we cannot yet
  // correct paths yet.
  //
  // Remove this once CssTagScanner::TransformUrls handles imports.
  if (CssTagScanner::HasImport(contents, driver_->message_handler())) {
    return false;
  }

  return true;
}

void CssInlineFilter::RenderInline(const ResourcePtr& resource,
                                   const GoogleUrl& base_url,
                                   const StringPiece& contents,
                                   HtmlElement* element) {
  MessageHandler* message_handler = driver_->message_handler();

  // Absolutify the URLs in the CSS -- relative URLs will break otherwise.
  // Note that we have to do this at rendering stage, since the same stylesheet
  // may be included from HTML in different directories.
  // TODO(jmarantz): fix bug 295:  domain-rewrite & shard here.
  GoogleString rewritten_contents;
  StringWriter writer(&rewritten_contents);
  GoogleUrl resource_url(resource->url());
  StringPiece input_dir = resource_url.AllExceptLeaf();
  StringPiece base_dir = base_url.AllExceptLeaf();
  bool written;
  const DomainLawyer* lawyer = driver_->options()->domain_lawyer();
  if (lawyer->WillDomainChange(resource_url.Origin()) ||
      input_dir != base_dir) {
    RewriteDomainTransformer transformer(&resource_url, &base_url, driver_);
    written = CssTagScanner::TransformUrls(contents, &writer, &transformer,
                                           message_handler);
  } else {
    // We don't need to absolutify URLs if input directory is same as base.
    written = writer.Write(contents, message_handler);
  }
  if (!written) {
    return;
  }

  // Inline the CSS.
  HtmlElement* style_element =
      driver_->NewElement(element->parent(), HtmlName::kStyle);
  if (driver_->ReplaceNode(element, style_element)) {
    driver_->AppendChild(
        style_element,
        driver_->NewCharactersNode(element, rewritten_contents));
  }
}

bool CssInlineFilter::HasAsyncFlow() const {
  return driver_->asynchronous_rewrites();
}

}  // namespace net_instaweb
