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
#include "net/instaweb/rewriter/public/inline_rewrite_context.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/charset_util.h"
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
    const char* charset = element->AttributeValue(HtmlName::kCharset);
    if (charset != NULL) {
      attrs_charset_ = GoogleString(charset);
    }
  }

  virtual bool ShouldInline(const ResourcePtr& resource) const {
    return filter_->ShouldInline(resource, attrs_charset_);
  }

  virtual void Render() {
    if (num_output_partitions() < 1) {
      // Remove any LSC attributes as they're pointless if we don't inline.
      LocalStorageCacheFilter::RemoveLscAttributes(get_element());
    }
    InlineRewriteContext::Render();
  }

  virtual void RenderInline(
      const ResourcePtr& resource, const StringPiece& text,
      HtmlElement* element) {
    filter_->RenderInline(resource, *(output_partition(0)),
                          base_url_, text, element);
  }

  virtual const char* id() const { return RewriteOptions::kCssInlineId; }

 private:
  CssInlineFilter* filter_;
  GoogleUrl base_url_;
  GoogleString attrs_charset_;

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
    if (attr == NULL || attr->DecodedValueOrNull() == NULL) {
      return;  // We obviously can't inline if the URL isn't there.
    }

    // Ask the LSC filter to work out how to handle this element. A return
    // value of true means we don't have to rewrite it so can skip that.
    // The state is carried forward to after we initiate rewriting since
    // we might still have to modify the element.
    LocalStorageCacheFilter::InlineState state;
    if (!LocalStorageCacheFilter::AddStorableResource(
            attr->DecodedValueOrNull(), driver_, false /* check cookie */,
            element, &state)) {
      // StartInlining() transfers possession of ctx to RewriteDriver or
      // deletes it on failure.
      Context* ctx = new Context(this, base_url(), element, attr);
      bool initiated = ctx->StartInlining();

      // If we're rewriting we need the LSC filter to add the URL as an
      // attribute so that it knows to insert the LSC specific javascript.
      if (initiated) {
        LocalStorageCacheFilter::AddStorableResource(attr->DecodedValueOrNull(),
                                                     driver_,
                                                     true /* ignore cookie */,
                                                     element, &state);
      }
    }
  }
}

bool CssInlineFilter::ShouldInline(const ResourcePtr& resource,
                                   const StringPiece& attrs_charset) const {
  // If the contents are bigger than our threshold, don't inline.
  if (resource->contents().size() > size_threshold_bytes_) {
    return false;
  }

  // If the charset is incompatible with the HTML's, don't inline.
  StringPiece htmls_charset(driver_->containing_charset());
  GoogleString css_charset = RewriteFilter::GetCharsetForStylesheet(
      resource.get(), attrs_charset, htmls_charset);
  if (!StringCaseEqual(htmls_charset, css_charset)) {
    return false;
  }

  return true;
}

void CssInlineFilter::RenderInline(const ResourcePtr& resource,
                                   const CachedResult& cached,
                                   const GoogleUrl& base_url,
                                   const StringPiece& contents,
                                   HtmlElement* element) {
  MessageHandler* message_handler = driver_->message_handler();

  // Absolutify the URLs in the CSS -- relative URLs will break otherwise.
  // Note that we have to do this at rendering stage, since the same stylesheet
  // may be included from HTML in different directories.
  // TODO(jmarantz): fix bug 295:  domain-rewrite & shard here.
  StringPiece clean_contents(contents);
  StripUtf8Bom(&clean_contents);
  GoogleString rewritten_contents;
  StringWriter writer(&rewritten_contents);
  GoogleUrl resource_url(resource->url());
  bool resolved_ok = true;
  switch (driver_->ResolveCssUrls(resource_url, base_url.Spec(), clean_contents,
                                  &writer, message_handler)) {
    case RewriteDriver::kNoResolutionNeeded:
      // We don't need to absolutify URLs if input directory is same as base.
      if (!writer.Write(clean_contents, message_handler)) {
        resolved_ok = false;
      }
      break;
    case RewriteDriver::kWriteFailed:
      resolved_ok = false;
      break;
    case RewriteDriver::kSuccess:
      break;
  }

  if (resolved_ok) {
    // Inline the CSS.
    HtmlElement* style_element =
        driver_->NewElement(element->parent(), HtmlName::kStyle);
    if (driver_->ReplaceNode(element, style_element)) {
      driver_->AppendChild(style_element,
                           driver_->NewCharactersNode(element,
                                                      rewritten_contents));
    }

    // Add the local storage cache attributes if it is enabled.
    LocalStorageCacheFilter::AddLscAttributes(resource_url.Spec(), cached,
                                              false /* has_url */,
                                              driver_, style_element);
  } else {
    // Remove any LSC attributes as they're now pointless.
    LocalStorageCacheFilter::RemoveLscAttributes(element);
  }
}

}  // namespace net_instaweb
