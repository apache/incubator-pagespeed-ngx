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
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// TODO(ksimbili): Fix window.onload = addAllStyles call site as it will
// override the existing onload function.
const char CriticalCssFilter::kAddStylesScript[] =
    "var stylesAdded = false;"
    "var addAllStyles = function() {"
    "  if (stylesAdded) return;"
    "  stylesAdded = true;"
    "  var div = document.createElement(\"div\");"
    "  var styleText = \"\";"
    "  var styleElements = document.getElementsByClassName(\"psa_add_styles\");"
    "  for (var i = 0; i < styleElements.length; ++i) {"
    "    styleText += styleElements[i].textContent ||"
    "                 styleElements[i].innerHTML || "
    "                 styleElements[i].data || \"\";"
    "  }"
    "  div.innerHTML = styleText;"
    "  document.body.appendChild(div);"
    "};"
    "if (window.addEventListener) {"
    "  document.addEventListener(\"DOMContentLoaded\", addAllStyles, false);"
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
    : CommonFilter(driver),
      css_tag_scanner_(driver),
      finder_(finder),
      critical_css_result_(NULL),
      current_style_element_(NULL) {
  CHECK(finder_);  // a valid finder is expected
}

CriticalCssFilter::~CriticalCssFilter() {
}

void CriticalCssFilter::DetermineEnabled(GoogleString* disabled_reason) {
  bool is_ie = driver()->user_agent_matcher()->IsIe(driver()->user_agent());
  if (is_ie) {
    // Disable critical CSS for IE because conditional-comments are not handled
    // by the filter.
    // TODO(slamm): Add conditional-comment support, or enable on IE10
    // or higher. By default, IE10 does not support conditional comments.
    // However, pages can opt into the IE9 behavior with a meta tag:
    //     <meta http-equiv="X-UA-Compatible" content="IE=EmulateIE9">
    // IE10 could be enabled if the meta tag is not present.
    // Short of full conditional-comment support, the filter could also detect
    // whether conditional-comments are present (while computing critical CSS)
    // and only disable the filter for IE if they are.
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kPrioritizeCriticalCss),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);

    *disabled_reason = StrCat("User agent '", driver()->user_agent(),
                              "' appears to be Internet Explorer");
  }
  set_is_enabled(!is_ie);
}

void CriticalCssFilter::StartDocumentImpl() {
  // If there is no critical CSS data, the filter is a no-op.
  // However, the property cache is unavailable in DetermineEnabled
  // where disabling is possible.
  CHECK(finder_);
  critical_css_result_ = finder_->GetCriticalCss(driver());

  const bool is_property_cache_miss = critical_css_result_ == NULL;

  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kPrioritizeCriticalCss),
      (is_property_cache_miss ?
       RewriterHtmlApplication::PROPERTY_CACHE_MISS :
       RewriterHtmlApplication::ACTIVE));

  url_indexes_.clear();
  if (!is_property_cache_miss) {
    for (int i = 0, n = critical_css_result_->link_rules_size(); i < n; ++i) {
      const GoogleString& url = critical_css_result_->link_rules(i).link_url();
      url_indexes_.insert(make_pair(url, i));
    }
  }

  has_critical_css_ = !url_indexes_.empty();
  is_move_link_script_added_ = false;

  DCHECK(css_elements_.empty());  // emptied in EndDocument()
  DCHECK(current_style_element_ == NULL);  // cleared in EndElement()

  // Reset the stats since a filter instance may be reused.
  total_critical_size_ = 0;
  total_original_size_ = 0;
  repeated_style_blocks_size_ = 0;
  num_repeated_style_blocks_ = 0;
  num_links_ = 0;
  num_replaced_links_ = 0;
}

void CriticalCssFilter::EndDocument() {
  // Don't add link/style tags here, if we are in flushing early driver. We'll
  // get chance to collect and add them again through flushed early driver.
  if (num_replaced_links_ > 0 && !driver()->flushing_early()) {
    HtmlElement* noscript_element =
        driver()->NewElement(NULL, HtmlName::kNoscript);
    driver()->AddAttribute(noscript_element, HtmlName::kClass,
                           CriticalSelectorFilter::kNoscriptStylesClass);
    InsertNodeAtBodyEnd(noscript_element);
    // Write the full set of CSS elements (critical and non-critical rules).
    for (CssElementVector::iterator it = css_elements_.begin(),
         end = css_elements_.end(); it != end; ++it) {
      (*it)->AppendTo(noscript_element);
    }

    HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
    InsertNodeAtBodyEnd(script);

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
    driver()->server_context()->static_asset_manager()->AddJsToElement(
        critical_css_script, script, driver());

    driver()->log_record()->SetCriticalCssInfo(
        total_critical_size_, total_original_size_, total_overhead_size);
  }
  if (has_critical_css_ && driver()->DebugMode()) {
    driver()->InsertComment(StringPrintf(
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
}

void CriticalCssFilter::StartElementImpl(HtmlElement* element) {
  if (has_critical_css_ && element->keyword() == HtmlName::kStyle) {
    // Capture the style block because full CSS will be copied to end
    // of document if critical CSS rules are used.
    current_style_element_ = new CssStyleElement(driver(), element);
    num_repeated_style_blocks_ += 1;
  }
}

void CriticalCssFilter::Characters(HtmlCharactersNode* characters_node) {
  CommonFilter::Characters(characters_node);
  if (current_style_element_ != NULL) {
    current_style_element_->AppendCharactersNode(characters_node);
    repeated_style_blocks_size_ += characters_node->contents().size();
  }
}

void CriticalCssFilter::EndElementImpl(HtmlElement* element) {
  if (current_style_element_ != NULL) {
    // Capture the current style element.
    CHECK(element->keyword() == HtmlName::kStyle);
    css_elements_.push_back(current_style_element_);
    current_style_element_ = NULL;
    return;
  }

  if (noscript_element() != NULL) {
    // We are inside a no script element. No point moving further.
    return;
  }

  if (!has_critical_css_) {
    // No critical CSS, so don't bother going further.  Also don't bother
    // logging a rewrite failure since we've logged it already in StartDocument.
    return;
  }

  HtmlElement::Attribute* href;
  const char* media;
  if (!css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // Not a css link element.
    return;
  }

  num_links_++;
  css_elements_.push_back(new CssElement(driver(), element));

  const GoogleString url = DecodeUrl(href->DecodedValueOrNull());
  if (url.empty()) {
    // Unable to decode the link into a valid url.
    LogRewrite(RewriterApplication::INPUT_URL_INVALID);
    return;
  }

  const CriticalCssResult_LinkRules* link_rules = GetLinkRules(url);
  if (link_rules == NULL) {
    // The property wasn't found so we have no rules to apply.
    LogRewrite(RewriterApplication::PROPERTY_NOT_FOUND);
    return;
  }

  const GoogleString& style_id =
      driver()->server_context()->hasher()->Hash(url);

  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  // If the resource has already been flushed early, just apply it here. This
  // can be checked by looking up the url in the DOM cohort. If the url is
  // present in the DOM cohort, it is guaranteed to have been flushed early.
  if (driver()->flushed_early() &&
      driver()->options()->enable_flush_early_critical_css() &&
      driver()->flush_early_info() != NULL &&
      driver()->flush_early_info()->resource_html().find(escaped_url) !=
          GoogleString::npos) {
    // In this case we have already added the CSS rules to the head as
    // part of flushing early. Now, find the rule, remove the disabled tag
    // and move it here.

    // Add the JS function definition that moves and applies the flushed early
    // CSS rules, if it has not already been added.
    if (!is_move_link_script_added_) {
      is_move_link_script_added_ = true;
      HtmlElement* script =
          driver()->NewElement(element->parent(), HtmlName::kScript);
      // TODO(slamm): Remove this attribute and update webdriver test as needed.
      driver()->AddAttribute(script, HtmlName::kId,
                             CriticalSelectorFilter::kMoveScriptId);
      driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
      driver()->InsertNodeBeforeNode(element, script);
      driver()->server_context()->static_asset_manager()->AddJsToElement(
          CriticalSelectorFilter::kApplyFlushEarlyCss, script, driver());
    }

    HtmlElement* script_element =
        driver()->NewElement(element->parent(), HtmlName::kScript);
    driver()->AddAttribute(script_element, HtmlName::kPagespeedNoDefer, "");
    if (!driver()->ReplaceNode(element, script_element)) {
      LogRewrite(RewriterApplication::REPLACE_FAILED);
      return;
    }
    GoogleString js_data = StringPrintf(
        CriticalSelectorFilter::kInvokeFlushEarlyCssTemplate,
        style_id.c_str(), media);

    driver()->server_context()->static_asset_manager()->AddJsToElement(
        js_data, script_element, driver());
  } else {
    // Replace link with critical CSS rules.
    HtmlElement* style_element =
        driver()->NewElement(element->parent(), HtmlName::kStyle);
    if (!driver()->ReplaceNode(element, style_element)) {
      LogRewrite(RewriterApplication::REPLACE_FAILED);
      return;
    }

    driver()->AppendChild(style_element, driver()->NewCharactersNode(
        element, link_rules->critical_rules()));
    // If the link tag has a media attribute, copy it over to the style.
    if (media != NULL && strcmp(media, "") != 0) {
      driver()->AddEscapedAttribute(
          style_element, HtmlName::kMedia, media);
    }

    // Add a special attribute to style element so the flush early filter
    // can identify the element and flush these elements early as link tags.
    // By flushing the inlined link style tags early, the content can be
    // downloaded early before the HTML arrives.
    if (driver()->flushing_early()) {
      driver()->AddAttribute(style_element, HtmlName::kDataPagespeedFlushStyle,
                             style_id);
    }
  }

  // TODO(mpalem): Stats need to be updated for total critical css size when
  // the css rules are flushed early.
  int critical_size = link_rules->critical_rules().length();
  int original_size = link_rules->original_size();
  total_critical_size_ += critical_size;
  total_original_size_ += original_size;
  if (driver()->DebugMode()) {
    driver()->InsertComment(StringPrintf(
        "Critical CSS applied:\n"
        "critical_size=%d\n"
        "original_size=%d\n"
        "original_src=%s\n",
        critical_size, original_size, link_rules->link_url().c_str()));
  }

  num_replaced_links_++;
  LogRewrite(RewriterApplication::APPLIED_OK);
}

GoogleString CriticalCssFilter::DecodeUrl(const GoogleString& url) {
  GoogleUrl gurl(driver()->base_url(), url);
  if (!gurl.IsWebValid()) {
    return "";
  }
  StringVector decoded_urls;
  // Decode the url if it is pagespeed encoded.
  if (driver()->DecodeUrl(gurl, &decoded_urls)) {
    if (decoded_urls.size() == 1) {
      return decoded_urls.at(0);
    } else {
      driver()->InfoHere("Critical CSS: Unable to process combined URL: %s",
                         url.c_str());
      return "";
    }
  }
  return gurl.Spec().as_string();
}

const CriticalCssResult_LinkRules* CriticalCssFilter::GetLinkRules(
    const GoogleString& decoded_url) {
  UrlIndexes::const_iterator it = url_indexes_.find(decoded_url);
  if (it == url_indexes_.end()) {
    driver()->InfoHere("Critical CSS rules not found for URL: %s",
                       decoded_url.c_str());
    return NULL;
  }

  // Use "mutable" to get a pointer. A reference does not make sense here.
  return critical_css_result_->mutable_link_rules(it->second);
}

void CriticalCssFilter::LogRewrite(int status) {
  driver()->log_record()->SetRewriterLoggingStatus(
      RewriteOptions::FilterId(RewriteOptions::kPrioritizeCriticalCss),
      static_cast<RewriterApplication::Status>(status));
}

}  // namespace net_instaweb
