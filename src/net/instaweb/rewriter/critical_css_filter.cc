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
#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
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
      finder_(finder),
      current_style_element_(NULL),
      has_critical_css_(false),
      has_critical_css_match_(false),
      total_critical_size_(0),
      total_original_size_(0),
      repeated_style_blocks_size_(0),
      num_repeated_style_blocks_(0),
      num_delayed_links_(0) {
}

CriticalCssFilter::~CriticalCssFilter() {
}

void CriticalCssFilter::StartDocument() {
  DCHECK(css_elements_.empty());
  current_style_element_ = NULL;
  has_critical_css_match_ = false;

  has_critical_css_ = false;
  if (finder_ != NULL) {
    // This cannot go in DetermineEnabled() because the cache is not ready.
    critical_css_result_.reset(finder_->GetCriticalCssFromCache(driver_));
    if (critical_css_result_.get() != NULL &&
        critical_css_result_->link_rules_size() > 0) {
      has_critical_css_ = true;
    }
  }
  url_indexes_.clear();
  if (has_critical_css_) {
    for (int i = 0, n = critical_css_result_->link_rules_size(); i < n; ++i) {
      const GoogleString& url = critical_css_result_->link_rules(i).link_url();
      url_indexes_.insert(make_pair(url, i));
    }
  }
}

void CriticalCssFilter::EndDocument() {
  if (has_critical_css_ && driver_->DebugMode()) {
    driver_->InsertComment(StringPrintf(
        "Summary Critical CSS stats:\n"
        "total_critical_inlined_size=%d\n"
        "total_original_external_size=%d\n"
        "\n"
        "num_delayed_links=%d\n"
        "num_repeated_style_blocks=%d\n"
        "repeated_style_blocks_size=%d\n"
        "\n"
        "unhandled_import_count=%d\n"
        "unhandled_link_count=%d\n"
        "exception_count=%d\n",
        total_critical_size_,
        total_original_size_,
        num_delayed_links_,
        num_repeated_style_blocks_,
        repeated_style_blocks_size_,
        critical_css_result_->import_count(),
        critical_css_result_->link_count(),
        critical_css_result_->exception_count()));
  }
  if (has_critical_css_match_) {
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
    driver_->server_context()->static_asset_manager()->AddJsToElement(
        kAddStylesScript, script, driver_);
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
    // TODO(slamm): Prioritize critical rules for style blocks too?
    CHECK(element->keyword() == HtmlName::kStyle);
    css_elements_.push_back(current_style_element_);
    current_style_element_ = NULL;
  } else if (has_critical_css_ && element->keyword() == HtmlName::kLink) {
    HtmlElement::Attribute* href;
    const char* media;
    if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
      css_elements_.push_back(new CssElement(driver_, element));
      num_delayed_links_ += 1;

      const CriticalCssResult_LinkRules* link_rules =
          GetLinkRules(href->DecodedValueOrNull());
      if (link_rules != NULL) {
        // Replace link with critical CSS rules.
        // TODO(slamm): Keep stats on CSS added and repeated CSS links.
        //   The latter will help debugging if resources get downloaded twice.
        has_critical_css_match_ = true;
        HtmlElement* style_element =
            driver_->NewElement(element->parent(), HtmlName::kStyle);
        if (driver_->ReplaceNode(element, style_element)) {
          driver_->AppendChild(style_element, driver_->NewCharactersNode(
              element, link_rules->critical_rules()));

          // If the link tag has a media attribute, copy it over to the style.
          if (media != NULL && strcmp(media, "") != 0) {
            driver_->AddEscapedAttribute(
                style_element, HtmlName::kMedia, media);
          }
          if (driver_->DebugMode()) {
            int critical_size = link_rules->critical_rules().length();
            int original_size = link_rules->original_size();
            total_critical_size_ += critical_size;
            total_original_size_ += original_size;
            driver_->InsertComment(StringPrintf(
                "Critical CSS applied:\n"
                "critical_size=%d\n"
                "original_size=%d\n"
                "original_src=%s\n",
                critical_size, original_size, link_rules->link_url().c_str()));
          }
        }
      }
    }
  }
}

const CriticalCssResult_LinkRules* CriticalCssFilter::GetLinkRules(
    StringPiece url) const {
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
  if (link_url.is_valid()) {
    const GoogleString& url = link_url.Spec().as_string();
    UrlIndexes::const_iterator it = url_indexes_.find(url);
    if (it != url_indexes_.end()) {
      // Use "mutable" to get a pointer. A reference does not make sense here.
      return critical_css_result_->mutable_link_rules(it->second);
    } else {
      driver_->InfoHere("Critical CSS rules not found for URL: %s", url.data());
    }
  }
  return NULL;
}

}  // namespace net_instaweb
