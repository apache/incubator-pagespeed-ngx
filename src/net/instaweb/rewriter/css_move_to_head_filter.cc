/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/statistics.h"

namespace {

// Names for Statistics variables.
const char kCssElementsMoved[] = "css_elements_moved";

}  // namespace

namespace net_instaweb {

CssMoveToHeadFilter::CssMoveToHeadFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      css_tag_scanner_(driver),
      move_css_to_head_(
          driver->options()->Enabled(RewriteOptions::kMoveCssToHead)),
      move_css_above_scripts_(
          driver->options()->Enabled(RewriteOptions::kMoveCssAboveScripts)) {
  Statistics* stats = driver_->statistics();
  css_elements_moved_ = stats->GetVariable(kCssElementsMoved);
}

CssMoveToHeadFilter::~CssMoveToHeadFilter() {}

void CssMoveToHeadFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCssElementsMoved);
}

void CssMoveToHeadFilter::StartDocumentImpl() {
  move_to_element_ = NULL;
}

void CssMoveToHeadFilter::EndElementImpl(HtmlElement* element) {
  if (move_to_element_ == NULL) {
    // We record the first we see, either </head> or <script>. That will be
    // the anchor for where to move all styles.
    if (move_css_to_head_ &&
        element->keyword() == HtmlName::kHead) {
      move_to_element_ = element;
      element_is_head_ = true;
    } else if (move_css_above_scripts_ &&
               element->keyword() == HtmlName::kScript) {
      move_to_element_ = element;
      element_is_head_ = false;
    }

  // Do not move anything out of a <noscript> element.
  // MoveCurrent* methods will check that that we are allowed to move these
  // elements into the approriate places.
  } else if (noscript_element() == NULL) {
    HtmlElement::Attribute* href;
    const char* media;
    if ((element->keyword() == HtmlName::kStyle) ||
        css_tag_scanner_.ParseCssElement(element, &href, &media)) {
      css_elements_moved_->Add(1);

      if (element_is_head_) {
        // Move styles to end of head.
        driver_->MoveCurrentInto(move_to_element_);
      } else {
        // Move styles directly before that first script.
        driver_->MoveCurrentBefore(move_to_element_);
      }
    }
  }
}

}  // namespace net_instaweb
