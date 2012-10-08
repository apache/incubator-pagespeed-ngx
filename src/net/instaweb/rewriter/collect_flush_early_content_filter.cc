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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

CollectFlushEarlyContentFilter::CollectFlushEarlyContentFilter(
    RewriteDriver* driver)
    : RewriteFilter(driver) {
  Clear();
}

void CollectFlushEarlyContentFilter::StartDocumentImpl() {
  Clear();
  FlushEarlyInfoFinder* finder =
      driver()->server_context()->flush_early_info_finder();
  if (finder != NULL && finder->IsMeaningful()) {
    finder->UpdateFlushEarlyInfoInDriver(driver());
    finder->ComputeFlushEarlyInfo(driver());
  }
}

void CollectFlushEarlyContentFilter::EndDocument() {
  if (driver()->flushing_early()) {
    return;
  }
  if (!resource_html_.empty()) {
    driver()->flush_early_info()->set_resource_html(resource_html_);
  }
}

void CollectFlushEarlyContentFilter::StartElementImpl(HtmlElement* element) {
  if (driver()->flushing_early() || noscript_element() != NULL) {
    // Do nothing.
    return;
  }
  if (element->keyword() == HtmlName::kHead) {
    in_head_ = true;
    return;
  }
  semantic_type::Category category;
  HtmlElement::Attribute* attr =  resource_tag_scanner::ScanElement(
      element, driver(), &category);
  // Find javascript elements in the head, and css elements in the entire page.
  if ((attr != NULL) &&
      (category == semantic_type::kStylesheet ||
       (category == semantic_type::kScript && in_head_))) {
    // TODO(pulkitg): Collect images which can be flushed early.
    StringPiece url(attr->DecodedValueOrNull());
    if (!url.empty()) {
      // Absolutify the url before storing its value so that we handle
      // <base> tags correctly.
      GoogleUrl gurl(driver()->base_url(), url);
      if (gurl.is_valid()) {
        StringVector decoded_url;
        // Decode the url if it is encoded.
        if (driver()->DecodeUrl(gurl, &decoded_url)) {
          // TODO(pulkitg): Detect cases where rewritten resources are already
          // present in the original html.
          if (decoded_url.size() == 1) {
            // There will be only 1 url as combiners are off and this should be
            // modified once they are enabled.
            AppendToHtml(decoded_url.at(0).c_str(), category, element);
          }
        } else {
          AppendToHtml(gurl.Spec(), category, element);

        }
      }
    }
  }
}

void CollectFlushEarlyContentFilter::AppendToHtml(
    StringPiece url, semantic_type::Category category, HtmlElement* element) {
  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  if (category == semantic_type::kStylesheet) {
    StrAppend(&resource_html_, "<link ");
    AppendAttribute(HtmlName::kType, element);
    AppendAttribute(HtmlName::kRel, element);
    StrAppend(&resource_html_, "href=\"", escaped_url.c_str(), "\"/>");
  } else if (category == semantic_type::kScript) {
    StrAppend(&resource_html_, "<script ");
    AppendAttribute(HtmlName::kType, element);
    StrAppend(&resource_html_, "src=\"", escaped_url.c_str(), "\"></script>");
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
  if (driver()->flushing_early() && noscript_element() != NULL) {
    // Do nothing if we are flushing early.
  } else if (element->keyword() == HtmlName::kHead) {
    // Check if we are exiting a <head> node.
    if (in_head_) {
      in_head_ = false;
    }
  }
}

void CollectFlushEarlyContentFilter::Clear() {
  in_head_ = false;
  resource_html_.clear();
}

}  // namespace net_instaweb
