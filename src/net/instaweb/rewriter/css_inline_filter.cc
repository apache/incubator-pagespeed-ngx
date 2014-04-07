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

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/inline_rewrite_context.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const char CssInlineFilter::kNumCssInlined[] = "num_css_inlined";

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
      LocalStorageCacheFilter::RemoveLscAttributes(get_element(),
                                                   filter_->driver());
    }
    InlineRewriteContext::Render();
  }

  virtual void RenderInline(
      const ResourcePtr& resource, const StringPiece& text,
      HtmlElement* element) {
    filter_->RenderInline(resource, *(output_partition(0)),
                          base_url_, text, element);
  }

  virtual ResourcePtr CreateResource(const char* url) {
    return filter_->CreateResource(url);
  }

  virtual const char* id() const { return filter_->id_; }

 private:
  CssInlineFilter* filter_;
  GoogleUrl base_url_;
  GoogleString attrs_charset_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

CssInlineFilter::CssInlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      id_(RewriteOptions::kCssInlineId),
      size_threshold_bytes_(driver->options()->css_inline_max_bytes()),
      css_tag_scanner_(driver) {
  Statistics* stats = server_context()->statistics();
  num_css_inlined_ = stats->GetVariable(kNumCssInlined);
}

void CssInlineFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumCssInlined);
}

void CssInlineFilter::StartDocumentImpl() {
}

CssInlineFilter::~CssInlineFilter() {}

void CssInlineFilter::EndElementImpl(HtmlElement* element) {
  // Don't inline if the CSS element is under <noscript>.
  if (noscript_element() != NULL) {
    return;
  }
  HtmlElement::Attribute* href = NULL;
  const char* media = NULL;
  if (css_tag_scanner_.ParseCssElement(element, &href, &media) &&
      !driver()->HasChildrenInFlushWindow(element)) {
    // Only inline if the media type affects "screen".  We don't inline other
    // types since they're very unlikely to change the initial page view, and
    // inlining them would actually slow down the 99% case of "screen".
    if (!css_util::CanMediaAffectScreen(media)) {
      driver()->message_handler()->Message(
          kInfo, "Stylesheet media=%s is not for screen href=%s",
          media, href->DecodedValueOrNull());
      return;
    }
    // Ask the LSC filter to work out how to handle this element. A return
    // value of true means we don't have to rewrite it so can skip that.
    // The state is carried forward to after we initiate rewriting since
    // we might still have to modify the element.
    LocalStorageCacheFilter::InlineState state;
    if (!LocalStorageCacheFilter::AddStorableResource(
            href->DecodedValueOrNull(), driver(), false /* check cookie */,
            element, &state)) {
      // StartInlining() transfers possession of ctx to RewriteDriver or
      // deletes it on failure.
      Context* ctx = new Context(this, base_url(), element, href);
      bool initiated = ctx->StartInlining();

      // If we're rewriting we need the LSC filter to add the URL as an
      // attribute so that it knows to insert the LSC specific javascript.
      if (initiated) {
        LocalStorageCacheFilter::AddStorableResource(href->DecodedValueOrNull(),
                                                     driver(),
                                                     true /* ignore cookie */,
                                                     element, &state);
      }
    }
  }
}

ResourcePtr CssInlineFilter::CreateResource(const char* url) {
  return CreateInputResource(url);
}

bool CssInlineFilter::HasClosingStyleTag(StringPiece contents) {
  return FindIgnoreCase(contents, "</style") != StringPiece::npos;
}

bool CssInlineFilter::ShouldInline(const ResourcePtr& resource,
                                   const StringPiece& attrs_charset) const {
  // If the contents are bigger than our threshold or the contents contain
  // "</style>" anywhere, don't inline. If we inline an external stylesheet
  // containing a "</style>", the <style> tag will be ended early.
  if (resource->contents().size() > size_threshold_bytes_) {
    return false;
  }
  if (HasClosingStyleTag(resource->contents())) {
    return false;
  }

  // If the charset is incompatible with the HTML's, don't inline.
  StringPiece htmls_charset(driver()->containing_charset());
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
  MessageHandler* message_handler = driver()->message_handler();

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
  switch (driver()->ResolveCssUrls(
      resource_url, base_url.Spec(), clean_contents,
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

  if (!resolved_ok) {
    // Remove any LSC attributes as they're now pointless.
    LocalStorageCacheFilter::RemoveLscAttributes(element, driver());
    return;
  }

  // Inline the CSS.
  HtmlElement* style_element =
      driver()->NewElement(element->parent(), HtmlName::kStyle);
  if (!driver()->ReplaceNode(element, style_element)) {
    DCHECK(false) << "!driver()->ReplaceNode(element, style_element)";
    return;
  }
  driver()->AppendChild(style_element,
                        driver()->NewCharactersNode(element,
                                                    rewritten_contents));

  // Copy over most attributes from the original link, discarding those that
  // we convert (href, rel), and dropping those that are irrelevant (type).
  bool has_pagespeed_lsc_url = false;
  bool has_pagespeed_lsc_hash = false;
  const HtmlElement::AttributeList& attrs = element->attributes();
  for (HtmlElement::AttributeConstIterator i(attrs.begin()), e(attrs.end());
       i != e; ++i) {
    const HtmlElement::Attribute& attr = *i;
    switch (attr.keyword()) {
      case HtmlName::kHref:
      case HtmlName::kRel:
      case HtmlName::kType:
        break;
      case HtmlName::kPagespeedLscHash:
        // If we have a hash, we /must/ have an url as well, so the fallthrough
        // will be a no-op (so, the hash case must come before the url case).
        has_pagespeed_lsc_hash = true;
        FALLTHROUGH_INTENDED;
      case HtmlName::kPagespeedLscUrl:
        has_pagespeed_lsc_url = true;
        FALLTHROUGH_INTENDED;
      default:
        style_element->AddAttribute(attr);
        break;
    }
  }
  if (driver()->options()->Enabled(RewriteOptions::kComputeCriticalCss)) {
    // If compute_critical_css is enabled, add 'href' attribute to the style
    // node.
    // Computing critical css needs this url to store the critical
    // css in the map.
    driver()->AddAttribute(style_element, HtmlName::kDataPagespeedHref,
                           resource_url.Spec());
  }
  // If we don't already have a pagespeed_lsc_url then EndElementImpl must not
  // have called AddStorableResource or LSC is disabled; in either case there
  // is no point in trying to add the LSC attributes. OTOH, if have an url and
  // a hash then we've already got all the attributes we need.
  if (has_pagespeed_lsc_url && !has_pagespeed_lsc_hash) {
    LocalStorageCacheFilter::AddLscAttributes(resource_url.Spec(), cached,
                                              driver(), style_element);
  }
  num_css_inlined_->Add(1);
}

}  // namespace net_instaweb
