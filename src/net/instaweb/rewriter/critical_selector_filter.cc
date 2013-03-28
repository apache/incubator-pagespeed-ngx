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

// Author: slamm@google.com (Stephen Lamm),
//         morlovich@google.com (Maksim Orlovich)
// See the header for overview.

#include "net/instaweb/rewriter/public/critical_selector_filter.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/selector.h"

namespace net_instaweb {

namespace {

// Helper that takes a std::vector-like collection, and compacts
// any null holes in it.
template<typename VectorType> void Compact(VectorType* cl) {
  typename VectorType::iterator new_end =
      std::remove(cl->begin(), cl->end(),
                  static_cast<typename VectorType::value_type>(NULL));
  cl->erase(new_end, cl->end());
}

}  // namespace

const char CriticalSelectorFilter::kSummarizedCssProperty[] =
    "selector_summarized_css";

// TODO(ksimbili): Move this to appropriate event instead of 'onload'.
const char CriticalSelectorFilter::kAddStylesScript[] =
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

// TODO(morlovich): Check charset like CssInlineFilter::ShouldInline().

// Wrap CSS elements to move them later in the document.
// A simple list of elements is insufficient because link tags and style tags
// are inserted different.
class CriticalSelectorFilter::CssElement {
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
class CriticalSelectorFilter::CssStyleElement
    : public CriticalSelectorFilter::CssElement {
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
CriticalSelectorFilter::CriticalSelectorFilter(RewriteDriver* driver)
    : CssSummarizerBase(driver),
      style_element_to_delete_(NULL),
      inserted_critical_css_(false) {
}

CriticalSelectorFilter::~CriticalSelectorFilter() {
}

void CriticalSelectorFilter::Summarize(Css::Stylesheet* stylesheet,
                                       GoogleString* out) const {
  for (int ruleset_index = 0, num_rulesets = stylesheet->rulesets().size();
       ruleset_index < num_rulesets; ++ruleset_index) {
    Css::Ruleset* r = stylesheet->mutable_rulesets().at(ruleset_index);
    if (r->type() == Css::Ruleset::UNPARSED_REGION) {
      // Couldn't parse this as a rule, leave unaltered. Hopefully it's not
      // too big..
      continue;
    }

    // TODO(morlovich): This does a lot of repeated work as the same media
    // entries are repeated for tons of rulesets.
    // TODO(morlovich): It's silly to serialize this, we should work directly
    // off AST once we have decision procedure on that.

    bool any_media_apply = r->media_queries().empty();
    for (int mediaquery_index = 0, num_mediaquery = r->media_queries().size();
         mediaquery_index < num_mediaquery; ++mediaquery_index) {
      Css::MediaQuery* mq = r->mutable_media_queries().at(mediaquery_index);
      if (css_util::CanMediaAffectScreen(mq->ToString())) {
        any_media_apply = true;
      } else {
        delete mq;
        r->mutable_media_queries()[mediaquery_index] = NULL;
      }
    }

    bool any_selectors_apply = false;
    if (any_media_apply) {
      // See which of the selectors for given declaration apply.
      // Note that in some partial parse errors we will get 0 selectors here,
      // in which case we retain things to be conservative.
      any_selectors_apply = r->selectors().empty();
      for (int selector_index = 0, num_selectors = r->selectors().size();
          selector_index < num_selectors; ++selector_index) {
        Css::Selector* s = r->mutable_selectors().at(selector_index);
        GoogleString portion_to_compare = css_util::JsDetectableSelector(*s);
        if (portion_to_compare.empty() ||
            critical_selectors_.find(portion_to_compare)
                != critical_selectors_.end()) {
          any_selectors_apply = true;
        } else {
          delete s;
          r->mutable_selectors()[selector_index] = NULL;
        }
      }
    }

    if (any_selectors_apply && any_media_apply) {
      // Just remove the irrelevant selectors & media
      Compact(&r->mutable_selectors());
      Compact(&r->mutable_media_queries());
    } else {
      // Remove the entire production
      delete r;
      stylesheet->mutable_rulesets()[ruleset_index] = NULL;
    }
  }
  Compact(&stylesheet->mutable_rulesets());

  // Serialize out the remaining subset.
  StringWriter writer(out);
  NullMessageHandler handler;
  CssMinify::Stylesheet(*stylesheet, &writer, &handler);
}

void CriticalSelectorFilter::SummariesDone() {
  // TODO(morlovich): How do we decide to invalidate things here?
  bool all_ok = true;
  CriticalSelectorSummarizedCss summary;
  for (int i = 0; i < NumStyles(); ++i) {
    const SummaryInfo& fragment = GetSummaryForStyle(i);
    if (fragment.state == kSummaryOk) {
      StrAppend(summary.mutable_content(), fragment.data);
    } else {
      all_ok = false;
    }
  }
  if (all_ok) {
    // We don't write cohort here since RewriteDriver takes care of committing
    // the DOM cohort.
    bool write_cohort = false;
    UpdateInPropertyCache(summary, driver_, RewriteDriver::kDomCohort,
                          kSummarizedCssProperty, write_cohort);
    driver_->set_write_property_cache_dom_cohort(true);
  }
}

void CriticalSelectorFilter::NotifyInlineCss(HtmlElement* style_element,
                                             HtmlCharactersNode* content) {
  if (critical_css_.get() != NULL) {
    CssStyleElement* save = new CssStyleElement(driver_, style_element);
    save->AppendCharactersNode(content);
    css_elements_.push_back(save);
    // We remove both the style element and content in case we get a flush
    // window in the middle, which would render the element itself uneditable.
    // (Though we also need to wait till </style> to delete the element.
    // TODO(morlovich): Testcase of flush thing.
    driver_->DeleteElement(content);
    style_element_to_delete_ = style_element;
    InsertCriticalCssIfNeeded(style_element);
  }
}

void CriticalSelectorFilter::NotifyExternalCss(HtmlElement* link) {
  if (critical_css_.get() != NULL) {
    css_elements_.push_back(new CssElement(driver_, link));
    if (!driver_->DeleteElement(link)) {
      driver_->WarningHere("Trouble removing non-critical CSS link?");
    }
    InsertCriticalCssIfNeeded(link);
  }
}

GoogleString CriticalSelectorFilter::CacheKeySuffix() const {
  return cache_key_suffix_;
}

void CriticalSelectorFilter::StartDocumentImpl() {
  CssSummarizerBase::StartDocumentImpl();

  // Read critical selector info from pcache.
  // TODO(morlovich): Distinguish the case where this is not available!
  critical_selectors_.clear();
  CriticalSelectorSet* pcache_selectors = driver_->CriticalSelectors();
  if (pcache_selectors != NULL) {
    for (int i = 0; i < pcache_selectors->critical_selectors_size(); ++i) {
      critical_selectors_.insert(pcache_selectors->critical_selectors(i));
    }
  }

  GoogleString all_selectors = JoinCollection(critical_selectors_, ",");
  cache_key_suffix_ =
      driver_->server_context()->lock_hasher()->Hash(all_selectors);

  // Read critical css info from pcache.
  PropertyCacheDecodeResult status;
  critical_css_.reset(DecodeFromPropertyCache<CriticalSelectorSummarizedCss>(
      driver_, RewriteDriver::kDomCohort, kSummarizedCssProperty,
      driver_->options()->finder_properties_cache_expiration_time_ms(),
      &status));

  // Clear state between re-uses / check to make sure we wrapped up properly.
  DCHECK(css_elements_.empty());
  style_element_to_delete_ = NULL;
  inserted_critical_css_ = false;
}

void CriticalSelectorFilter::EndDocument() {
  CssSummarizerBase::EndDocument();

  // In case we didn't spot a nice spot for insertion of critical CSS, put it
  // out now.
  InsertCriticalCssIfNeeded(NULL);
  if (critical_css_.get() != NULL && !css_elements_.empty()) {
    // Insert the full CSS, but comment all the style, link tags so that
    // look-ahead parser cannot find them.
    HtmlElement* noscript_element =
        driver_->NewElement(NULL, HtmlName::kNoscript);
    driver_->AddAttribute(noscript_element, HtmlName::kId, "psa_add_styles");
    driver_->InsertElementBeforeCurrent(noscript_element);
    // Write the full original CSS elements.
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
  critical_css_.reset(NULL);
}

void CriticalSelectorFilter::EndElementImpl(HtmlElement* element) {
  CssSummarizerBase::EndElementImpl(element);
  if (element == style_element_to_delete_) {
    InsertCriticalCssIfNeeded(style_element_to_delete_);
    driver_->DeleteElement(element);
    style_element_to_delete_ = NULL;
  }

  if (element->keyword() == HtmlName::kHead) {
    // An explicit </head> may be a good spot for injection.
    InsertCriticalCssIfNeeded(element);
  }
}

void CriticalSelectorFilter::InsertCriticalCssIfNeeded(
    HtmlElement* insert_before) {
  if (critical_css_.get() == NULL || inserted_critical_css_) {
    return;
  }

  if (insert_before != NULL && !driver_->IsRewritable(insert_before)) {
    return;
  }

  HtmlElement* style_element = driver_->NewElement(NULL, HtmlName::kStyle);
  // TODO(morlovich): This is unsafe inside <noscript>. Actually, what should
  // the summarizer do with <noscript>?
  if (insert_before != NULL) {
    driver_->InsertElementBeforeElement(insert_before, style_element);
  } else {
    driver_->InsertElementBeforeCurrent(style_element);
  }
  driver_->AppendChild(
      style_element, driver_->NewCharactersNode(style_element,
                                                critical_css_->content()));
  inserted_critical_css_ = true;
}

}  // namespace net_instaweb
