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
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// TODO(ksimbili): Move this to appropriate event instead of 'onload'.
const char CriticalCssFilter::kAddStylesScript[] =
    "<script type=\"text/javascript\">"
    "var addAllStyles = function() {"
    "  var stylesCommentedNode = document.getElementById(\"psa_add_styles\");"
    "  var htmlString = stylesCommentedNode.firstChild.textContent;"
    "  var div = document.createElement(\"div\");"
    "  div.innerHTML = htmlString;"
    "  document.body.appendChild(div);"
    "};"
    "if (window.addEventListener) {"
    "  window.addEventListener(\"load\", addAllStyles, false);"
    "} else if(window.attachEvent) {"
    "  window.attachEvent(\"onload\", addAllStyles);"
    "} else {"
    "  window.onload = addAllStyles;"
    "}</script>";

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

  virtual void InsertBeforeCurrent() const {
    html_parse_->InsertElementBeforeCurrent(element_);
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

  virtual void InsertBeforeCurrent() const {
    HtmlElement* element = element_;
    CssElement::InsertBeforeCurrent();
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
      has_critical_css_match_(false) {
}

CriticalCssFilter::~CriticalCssFilter() {
}

void CriticalCssFilter::StartDocument() {
  if (finder_ != NULL) {
    critical_css_map_.reset(finder_->CriticalCssMap(driver_));
  }
  DCHECK(css_elements_.empty());
  current_style_element_ = NULL;
  has_critical_css_match_ = false;
}

void CriticalCssFilter::EndDocument() {
  if (has_critical_css_match_) {
    // Comment all the style, link tags so that look ahead parser cannot find
    // them.
    driver_->InsertElementBeforeCurrent(
        driver_->NewCharactersNode(NULL, "<div id=\"psa_add_styles\"><!--"));
    // Write the full set of CSS elements (critical and non-critical rules).
    for (CssElementVector::iterator it = css_elements_.begin(),
         end = css_elements_.end(); it != end; ++it) {
      (*it)->InsertBeforeCurrent();
    }
    // Note: If a <style> block has a string '-->' then this will break the html
    // parsing. Most likely that is possible only in comments. But, since this
    // filter is ahead of MinifyCss, such strings inside <style> shouldn't be
    // sent to browser. The only problem with this is, if somebody has MinifyCss
    // turned off and that site has '-->' in <style> block we would have
    // problem.
    // TODO(ksimbili): Escape the '-->' inside <style> blocks.
    driver_->InsertElementBeforeCurrent(
        driver_->NewCharactersNode(NULL, "--></div>"));
    driver_->InsertElementBeforeCurrent(
        driver_->NewCharactersNode(NULL, kAddStylesScript));
  }
  if (!css_elements_.empty()) {
    STLDeleteElements(&css_elements_);
  }
}

void CriticalCssFilter::StartElement(HtmlElement* element) {
  if (!critical_css_map_->empty() &&
      element->keyword() == HtmlName::kStyle) {
    // Capture the style block because full CSS will be copied to end
    // of document if critical CSS rules are used.
    current_style_element_ = new CssStyleElement(driver_, element);
  }
}

void CriticalCssFilter::Characters(HtmlCharactersNode* characters_node) {
  if (current_style_element_ != NULL) {
    current_style_element_->AppendCharactersNode(characters_node);
  }
}

void CriticalCssFilter::EndElement(HtmlElement* element) {
  if (current_style_element_ != NULL) {
    // TODO(slamm): Prioritize critical rules for style blocks too?
    CHECK(element->keyword() == HtmlName::kStyle);
    css_elements_.push_back(current_style_element_);
    current_style_element_ = NULL;
  } else if (!critical_css_map_->empty() &&
             element->keyword() == HtmlName::kLink) {
    HtmlElement::Attribute* href;
    const char* media;
    if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
      css_elements_.push_back(new CssElement(driver_, element));

      StringPiece critical_rules(GetRules(href->DecodedValueOrNull()));
      if (critical_rules.data() != NULL) {
        // Replace link with critical CSS rules.
        // TODO(slamm): Keep stats on CSS added and repeated CSS links.
        //   The latter will help debugging if resources get downloaded twice.
        has_critical_css_match_ = true;
        HtmlElement* style_element =
            driver_->NewElement(element->parent(), HtmlName::kStyle);
        if (driver_->ReplaceNode(element, style_element)) {
          driver_->AppendChild(
              style_element,
              driver_->NewCharactersNode(element, critical_rules));

          // If the link tag has a media attribute, copy it over to the style.
          if (media != NULL && strcmp(media, "") != 0) {
            driver_->AddEscapedAttribute(
                style_element, HtmlName::kMedia, media);
          }
        }
      }
    }
  }
}

StringPiece CriticalCssFilter::GetRules(StringPiece url) const {
  StringPiece critical_rules;
  StringVector decoded_urls;
  GoogleUrl gurl(url);
  StringPiece decoded_url = url;
  // Decode the url if it is pagespeed encoded.
  if (driver_->DecodeUrl(gurl, &decoded_urls)) {
    // PrioritizeCriticalCss is ahead of combine_css.
    // So ideally, we should never have combined urls here.
     DCHECK_EQ(decoded_urls.size(), 1U)
         << "Found combined css url " << url << " in "
         << driver_->base_url().Spec();
    decoded_url.set(decoded_urls.at(0).c_str(), decoded_urls.at(0).size());
  }
  if (!critical_css_map_->empty()) {
    GoogleUrl link_url(driver_->base_url(), decoded_url);
    if (link_url.is_valid()) {
      StringStringMap::const_iterator it =
          critical_css_map_->find(link_url.Spec().as_string());
      if (it != critical_css_map_->end()) {
        critical_rules.set(it->second.data(), it->second.size());
      }
    }
  }
  return critical_rules;
}

}  // namespace net_instaweb
