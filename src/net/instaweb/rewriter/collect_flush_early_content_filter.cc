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
// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/collect_flush_early_content_filter.h"

#include <memory>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/critical_selector_filter.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CollectFlushEarlyContentFilter::Context : public SingleRewriteContext {
 public:
  explicit Context(RewriteDriver* driver)
      : SingleRewriteContext(driver, NULL, NULL) {}

 protected:
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output) {
    // Do not add resources which are inlined or combined.
    if (num_slots() != 1 || slot(0)->should_delete_element()) {
      // Do nothing.
    } else {
      // Update the cache with resource size.
      ResourceSlot* resource_slot = slot(0).get();
      ResourcePtr resource = resource_slot->resource();
      CachedResult* partition = output_partition(0);
      partition->set_size(resource->contents().size());
    }
    RewriteDone(kRewriteFailed, 0);
  }

  virtual void Render() {
    if (num_output_partitions() > 0 && output_partition(0)->has_size()) {
      HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
          slot(0).get());
      HtmlElement* element = html_slot->element();
      if (Driver()->IsRewritable(element)) {
        // TODO(pulkitg): Can IsRewritable be false here (see comment to
        // Propagate in rewrite_context.h)?
        Driver()->AddAttribute(element, HtmlName::kPagespeedSize,
                               Integer64ToString(output_partition(0)->size()));
      }
    }
  }

  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }

  virtual const char* id() const {
    return "rscc";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Context);
};

CollectFlushEarlyContentFilter::CollectFlushEarlyContentFilter(
    RewriteDriver* driver)
    : RewriteFilter(driver) {
  Clear();
}

void CollectFlushEarlyContentFilter::StartDocumentImpl() {
  Clear();
  FlushEarlyInfoFinder* finder =
      driver()->server_context()->flush_early_info_finder();
  if (finder != NULL && finder->IsMeaningful(driver())) {
    finder->UpdateFlushEarlyInfoInDriver(driver());
  }
}

void CollectFlushEarlyContentFilter::EndDocument() {
  if (driver()->flushing_early()) {
    return;
  }
  // Empty the resource_html_ if no resource is found.
  if (!found_resource_) {
    resource_html_ = "";
  }
  if (!resource_html_.empty()) {
    driver()->flush_early_info()->set_resource_html(resource_html_);
  }
}

void CollectFlushEarlyContentFilter::StartElementImpl(HtmlElement* element) {
  // Collect the link stylesheet tags inside the noscript element only if
  // they are added by the Critical CSS filter. In this case, the link tags
  // thus collected will be parsed by a subsequent run of the Critical CSS
  // filter in flush early phase. In this phase, Critical CSS filter replaces
  // link tags with style elements with critical CSS rules inlined and a
  // special attribute added (kDataPagespeedFlushStyle). Flush early content
  // filter in turn looks for the special attribute in the style tag and flush
  // the content early as inlined CSS link tags.
  // Note that this may cause the order of CSS elements stored in resource html
  // to be different from the order in which elements are parsed in HTML. This
  // can cause downloads to be in a different order too.
  //
  // FlushEarlyContentWriterFilter depends on us not flushing multiple resources
  // for the same element for two reasons:
  //  - The pagespeed_size attribute doesn't specify which url-valued attribute
  //    it refers to.
  //  - If there are multiple such attributes at least one is unlikely to be
  //    used and so not worth flushing.
  if (element == noscript_element()) {
    if (driver()->options()->enable_flush_early_critical_css()) {
      const char* cls = noscript_element()->AttributeValue(HtmlName::kClass);
      if (cls != NULL &&
          StringCaseEqual(cls, CriticalSelectorFilter::kNoscriptStylesClass)) {
        should_collect_critical_css_ = true;
      }
    }
    return;
  }

  if (noscript_element() != NULL && !should_collect_critical_css_) {
    // Do nothing
    return;
  }

  if (element->keyword() == HtmlName::kBody) {
    StrAppend(&resource_html_, "<body>");
    return;
  }

  if (driver()->flushing_early() &&
      driver()->options()->flush_more_resources_early_if_time_permits()) {
    resource_tag_scanner::UrlCategoryVector attributes;
    resource_tag_scanner::ScanElement(
        element, driver()->options(), &attributes);
    // We only want to flush early if there is a single flushable resource.
    HtmlElement::Attribute* resource_url = NULL;
    for (int i = 0, n = attributes.size(); i < n; ++i) {
      if (attributes[i].category == semantic_type::kStylesheet ||
          attributes[i].category == semantic_type::kScript ||
          attributes[i].category == semantic_type::kImage) {
        if (resource_url != NULL) {
          // This should never happen.  When StartElementImpl is called with
          // driver()->flushing_early() being true we're parsing the content
          // which we want to flush early.  That content was already filtered to
          // contain only elements with single resources to be flushed early.
          DCHECK(false);
          return;
        }
        resource_url = attributes[i].url;
      }
    }
    if (resource_url != NULL) {
      // We found a single resource to flush early.
      StringPiece url(resource_url->DecodedValueOrNull());
      if (url.empty() || IsDataUrl(url)) {
        return;
      }
      ResourcePtr resource = CreateInputResource(url);
      if (resource.get() == NULL) {
        return;
      }
      ResourceSlotPtr slot(driver()->GetSlot(resource, element, resource_url));
      Context* context = new Context(driver());
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  } else {
    // Find javascript elements in the head, and css elements in the entire
    // page.  Only look at standard link-href/script-src tags because those are
    // the only ones we can handle with AppendToHtml() and because we're only
    // able to flush one resource early per element.
    HtmlName::Keyword attribute_name;
    if (element->keyword() == HtmlName::kScript) {
      attribute_name = HtmlName::kSrc;
    } else if (element->keyword() == HtmlName::kLink) {
      attribute_name = HtmlName::kHref;
    } else {
      return;
    }
    HtmlElement::Attribute* resource_url =
        element->FindAttribute(attribute_name);
    semantic_type::Category category =
        resource_tag_scanner::CategorizeAttribute(
            element, resource_url, driver()->options());
    if (element->keyword() == HtmlName::kScript &&
        category != semantic_type::kScript) {
      return;
    }
    if (element->keyword() == HtmlName::kLink &&
        category != semantic_type::kStylesheet) {
      return;
    }

    StringPiece url(resource_url->DecodedValueOrNull());
    if (url.empty() || IsDataUrl(url)) {
      return;
    }
    ResourcePtr resource = CreateInputResource(url);
    if (resource.get() == NULL) {
      return;
    }
    // We need to always use the absolutified urls while flushing, else we
    // might end up flushing wrong resources. Use the absolutified url that is
    // computed in CreateInputResource call.
    GoogleUrl gurl(resource->url());
    if (gurl.IsWebValid()) {
      StringVector decoded_url;
      // Decode the url if it is encoded.
      if (driver()->DecodeUrl(gurl, &decoded_url)) {
        // TODO(pulkitg): Detect cases where rewritten resources are already
        // present in the original html.
        if (decoded_url.size() == 1) {
          // There will be only 1 url as combiners are off and this should be
          // modified once they are enabled.
          AppendToHtml(decoded_url.at(0), category, element);
        }
      } else {
        AppendToHtml(gurl.Spec(), category, element);
      }
    }
  }
}

void CollectFlushEarlyContentFilter::AppendToHtml(
    StringPiece url, semantic_type::Category category, HtmlElement* element) {
  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  found_resource_ = true;
  if (category == semantic_type::kStylesheet) {
    StrAppend(&resource_html_, "<link ");
    AppendAttribute(HtmlName::kType, element);
    AppendAttribute(HtmlName::kRel, element);
    StrAppend(&resource_html_, "href=\"", escaped_url, "\"/>");
  } else if (category == semantic_type::kScript) {
    StrAppend(&resource_html_, "<script ");
    AppendAttribute(HtmlName::kType, element);
    StrAppend(&resource_html_, "src=\"", escaped_url, "\"></script>");
  }
}

void CollectFlushEarlyContentFilter::AppendAttribute(
    HtmlName::Keyword keyword, HtmlElement* element) {
  HtmlElement::Attribute* attr = element->FindAttribute(keyword);
  if (attr != NULL) {
    StringPiece attr_value(attr->DecodedValueOrNull());
    if (!attr_value.empty()) {
      GoogleString escaped_value;
      HtmlKeywords::Escape(attr_value, &escaped_value);
      StrAppend(
          &resource_html_, attr->name_str(), "=\"", escaped_value, "\" ");
    }
  }
}

void CollectFlushEarlyContentFilter::EndElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    if (element == noscript_element()) {
      should_collect_critical_css_ = false;
    }
  } else if (element->keyword() == HtmlName::kBody) {
    StrAppend(&resource_html_, "</body>");
  }
}

void CollectFlushEarlyContentFilter::Clear() {
  resource_html_.clear();
  found_resource_ = false;
  should_collect_critical_css_ = false;
}

}  // namespace net_instaweb
