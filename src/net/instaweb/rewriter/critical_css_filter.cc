/*
 * Copyright 2013 Google Inc.
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

// Author: slamm@google.com (Stephen Lamm)
//
// Contains implementation of CriticalCssFilter, replaces link tags with
// style blocks of critical rules. The full CSS, links and style blocks,
// is inserted at the end. That means some CSS will be duplicated.
//
// TODO(slamm): Group all the inline blocks together (or make sure this filter
//     works with css_move_to_head_filter).

#include "net/instaweb/rewriter/public/critical_css_filter.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// TODO(ksimbili): Move this to appropriate event instead of 'onload'.
const char CriticalCssFilter::kAddStylesScript[] =
    "var addAllStyles = function() {"
    "  var div = document.createElement(\"div\");"
    "  div.innerHTML = document.getElementById(\"psa_add_styles\").textContent;"
    "  document.body.appendChild(div);"
    "};"
    "if (window.addEventListener) {"
    "  window.addEventListener(\"load\", addAllStyles, false);"
    "} else if (window.attachEvent) {"
    "  window.attachEvent(\"onload\", addAllStyles);"
    "} else {"
    "  window.onload = addAllStyles;"
    "}";

// TODO(slamm): Remove this once we complete logging for this filter.
const char CriticalCssFilter::kStatsScriptTemplate[] =
    "window['pagespeed'] = window['pagespeed'] || {};"
    "window['pagespeed']['criticalCss'] = {"
    "  'total_critical_inlined_size': %d,"
    "  'total_original_external_size': %d,"
    "  'total_overhead_size': %d,"
    "  'num_replaced_links': %d,"
    "  'num_unreplaced_links': %d"
    "};";

// TODO(slamm): Check charset like CssInlineFilter::ShouldInline().

// Wrap CSS elements to move them later in the document.
// A simple list of elements is insufficient because link tags and style tags
// are inserted different.
class CriticalCssFilter::CssElement {
 public:
  CssElement(HtmlParse* p, HtmlElement* e)
      : html_parse_(p), element_(p->CloneElement(e)) {}

  // HtmlParse deletes the element (regardless of whether it is inserted).
  virtual ~CssElement() {}

  virtual void AppendTo(HtmlElement* parent) const {
    html_parse_->AppendChild(parent, element_);
  }

 protected:
  HtmlParse* html_parse_;
  HtmlElement* element_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssElement);
};

// Wrap CSS style blocks to move them later in the document.
class CriticalCssFilter::CssStyleElement
    : public CriticalCssFilter::CssElement {
 public:
  CssStyleElement(HtmlParse* p, HtmlElement* e) : CssElement(p, e) {}
  virtual ~CssStyleElement() {}

  // Call before InsertBeforeCurrent.
  void AppendCharactersNode(HtmlCharactersNode* characters_node) {
    characters_nodes_.push_back(
        html_parse_->NewCharactersNode(NULL, characters_node->contents()));
  }

  virtual void AppendTo(HtmlElement* parent) const {
    HtmlElement* element = element_;
    CssElement::AppendTo(parent);
    for (CharactersNodeVector::const_iterator it = characters_nodes_.begin(),
         end = characters_nodes_.end(); it != end; ++it) {
      html_parse_->AppendChild(element, *it);
    }
  }

 protected:
  typedef std::vector<HtmlCharactersNode*> CharactersNodeVector;
  CharactersNodeVector characters_nodes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssStyleElement);
};

// Wrap CSS related elements so they can be moved later in the document.
CriticalCssFilter::CriticalCssFilter(RewriteDriver* driver,
                                     CriticalCssFinder* finder)
    : driver_(driver),
      css_tag_scanner_(driver),
      finder_(finder) {
}

CriticalCssFilter::~CriticalCssFilter() {
}

void CriticalCssFilter::StartDocument() {
  DCHECK(css_elements_.empty());

  // StartDocument may be called multiple times, reset internal state.
  current_style_element_ = NULL;
  total_critical_size_ = 0;
  total_original_size_ = 0;
  repeated_style_blocks_size_ = 0;
  num_repeated_style_blocks_ = 0;
  num_links_ = 0;
  num_replaced_links_ = 0;

  has_critical_css_ = false;

  if (finder_ != NULL) {
    // This cannot go in DetermineEnabled() because the cache is not ready.
    critical_css_result_.reset(finder_->GetCriticalCssFromCache(driver_));
    if (critical_css_result_.get() != NULL &&
        critical_css_result_->link_rules_size() > 0) {
      has_critical_css_ = true;
    }
  }

  const char* pcc_id = RewriteOptions::FilterId(
      RewriteOptions::kPrioritizeCriticalCss);
  LogRecord* log_record = driver_->log_record();
  url_indexes_.clear();
  if (has_critical_css_) {
    for (int i = 0, n = critical_css_result_->link_rules_size(); i < n; ++i) {
      const GoogleString& url = critical_css_result_->link_rules(i).link_url();
      url_indexes_.insert(make_pair(url, i));
    }
    log_record->LogRewriterHtmlStatus(pcc_id, RewriterStats::ACTIVE);
  } else {
    // TODO(gee): In the future it may be necessary for the finder to
    // communicate the exact reason for not returning the property (parse
    // failure, expiration, etc.), but for the time being lump all reasons
    // into the single category.
    log_record->LogRewriterHtmlStatus(pcc_id,
                                      RewriterStats::PROPERTY_CACHE_MISS);
  }
}

void CriticalCssFilter::EndDocument() {
  if (num_replaced_links_ > 0) {
    // Comment all the style, link tags so that look-ahead parser cannot find
    // them.
    HtmlElement* noscript_element =
        driver_->NewElement(NULL, HtmlName::kNoscript);
    driver_->AddAttribute(noscript_element, HtmlName::kId, "psa_add_styles");
    driver_->InsertElementBeforeCurrent(noscript_element);
    // Write the full set of CSS elements (critical and non-critical rules).
    for (CssElementVector::iterator it = css_elements_.begin(),
         end = css_elements_.end(); it != end; ++it) {
      (*it)->AppendTo(noscript_element);
    }

    HtmlElement* script = driver_->NewElement(NULL, HtmlName::kScript);
    driver_->InsertElementBeforeCurrent(script);
    int num_unreplaced_links_ = num_links_ - num_replaced_links_;
    int total_overhead_size =
        total_critical_size_ + repeated_style_blocks_size_;
    GoogleString critical_css_script = StrCat(
        kAddStylesScript,
        StringPrintf(kStatsScriptTemplate,
                     total_critical_size_,
                     total_original_size_,
                     total_overhead_size,
                     num_replaced_links_,
                     num_unreplaced_links_));
    driver_->server_context()->static_asset_manager()->AddJsToElement(
        critical_css_script, script, driver_);

    driver_->log_record()->SetCriticalCssInfo(
        total_critical_size_, total_original_size_, total_overhead_size);
  }
  if (has_critical_css_ && driver_->DebugMode()) {
    driver_->InsertComment(StringPrintf(
        "Additional Critical CSS stats:\n"
        "  num_repeated_style_blocks=%d\n"
        "  repeated_style_blocks_size=%d\n"
        "\n"
        "From computing the critical CSS:\n"
        "  unhandled_import_count=%d\n"
        "  unhandled_link_count=%d\n"
        "  exception_count=%d\n",
        num_repeated_style_blocks_,
        repeated_style_blocks_size_,
        critical_css_result_->import_count(),
        critical_css_result_->link_count(),
        critical_css_result_->exception_count()));
  }
  if (!css_elements_.empty()) {
    STLDeleteElements(&css_elements_);
  }
  critical_css_result_.reset();
}

void CriticalCssFilter::StartElement(HtmlElement* element) {
  if (has_critical_css_ && element->keyword() == HtmlName::kStyle) {
    // Capture the style block because full CSS will be copied to end
    // of document if critical CSS rules are used.
    current_style_element_ = new CssStyleElement(driver_, element);
    num_repeated_style_blocks_ += 1;
  }
}

void CriticalCssFilter::Characters(HtmlCharactersNode* characters_node) {
  if (current_style_element_ != NULL) {
    current_style_element_->AppendCharactersNode(characters_node);
    repeated_style_blocks_size_ += characters_node->contents().size();
  }
}

void CriticalCssFilter::EndElement(HtmlElement* element) {
  if (current_style_element_ != NULL) {
    // Capture the current style element.
    // TODO(slamm): Prioritize critical rules for style blocks too?
    CHECK(element->keyword() == HtmlName::kStyle);
    css_elements_.push_back(current_style_element_);
    current_style_element_ = NULL;
    return;
  }

  if (!has_critical_css_) {
    // No critical CSS, so don't bother going further.  Also don't bother
    // logging a rewrite failure since we've logged it already in StartDocument.
    return;
  }

  if (element->keyword() != HtmlName::kLink) {
    // We only rewrite link tags.
    return;
  }

  HtmlElement::Attribute* href;
  const char* media;
  if (!css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // Not a css element.
    return;
  }

  num_links_++;
  css_elements_.push_back(new CssElement(driver_, element));

  const GoogleString url = DecodeUrl(href->DecodedValueOrNull());
  if (url.empty()) {
    // Unable to decode the link into a valid url.
    LogRewrite(RewriterInfo::INPUT_URL_INVALID);
    return;
  }

  const CriticalCssResult_LinkRules* link_rules = GetLinkRules(url);
  if (link_rules == NULL) {
    // The property wasn't found so we have no rules to apply.
    LogRewrite(RewriterInfo::PROPERTY_NOT_FOUND);
    return;
  }

  // Replace link with critical CSS rules.
  HtmlElement* style_element =
      driver_->NewElement(element->parent(), HtmlName::kStyle);
  if (!driver_->ReplaceNode(element, style_element)) {
    LogRewrite(RewriterInfo::REPLACE_FAILED);
    return;
  }

  driver_->AppendChild(style_element, driver_->NewCharactersNode(
      element, link_rules->critical_rules()));
  // If the link tag has a media attribute, copy it over to the style.
  if (media != NULL && strcmp(media, "") != 0) {
    driver_->AddEscapedAttribute(
        style_element, HtmlName::kMedia, media);
  }

  int critical_size = link_rules->critical_rules().length();
  int original_size = link_rules->original_size();
  total_critical_size_ += critical_size;
  total_original_size_ += original_size;
  if (driver_->DebugMode()) {
    driver_->InsertComment(StringPrintf(
        "Critical CSS applied:\n"
          "critical_size=%d\n"
        "original_size=%d\n"
        "original_src=%s\n",
        critical_size, original_size, link_rules->link_url().c_str()));
  }

  num_replaced_links_++;
  LogRewrite(RewriterInfo::APPLIED_OK);
}

GoogleString CriticalCssFilter::DecodeUrl(const GoogleString& url) {
  StringVector decoded_urls;
  GoogleUrl gurl(url);
  StringPiece decoded_url = url;
  // Decode the url if it is pagespeed encoded.
  if (driver_->DecodeUrl(gurl, &decoded_urls)) {
    // PrioritizeCriticalCss is ahead of combine_css.
    // So ideally, we should never have combined urls here.
    DCHECK_EQ(decoded_urls.size(), 1U)
        << "Found combined css url " << url
        << " (rewriting " << driver_->url() << ")";
    decoded_url.set(decoded_urls.at(0).c_str(), decoded_urls.at(0).size());
  } else {
    driver_->InfoHere("Critical CSS: Unable to decode URL: %s", url.data());
  }

  GoogleUrl link_url(driver_->base_url(), decoded_url);
  if (!link_url.is_valid()) {
    return "";
  }

  return link_url.Spec().as_string();
}

const CriticalCssResult_LinkRules* CriticalCssFilter::GetLinkRules(
    const GoogleString& decoded_url) {
  UrlIndexes::const_iterator it = url_indexes_.find(decoded_url);
  if (it == url_indexes_.end()) {
    driver_->InfoHere("Critical CSS rules not found for URL: %s",
                      decoded_url.data());
    return NULL;
  }

  // Use "mutable" to get a pointer. A reference does not make sense here.
  return critical_css_result_->mutable_link_rules(it->second);
}

void CriticalCssFilter::LogRewrite(int status) {
  driver_->log_record()->SetRewriterLoggingStatus(
      RewriteOptions::FilterId(RewriteOptions::kPrioritizeCriticalCss),
      static_cast<RewriterInfo::RewriterApplicationStatus>(status));
}

}  // namespace net_instaweb
