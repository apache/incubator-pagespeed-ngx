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
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/null_message_handler.h"
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

// Function and invocation are separate so we can suppress the invocation when
// running tests while still being able to manually invoke it from the JS
// console if desired.
// See ModPagespeedTestOnlyCriticalSelectorFilterDontApplyOriginalCss.
const char CriticalSelectorFilter::kAddStylesFunction[] =
    "var addAllStyles = function() {"
    "  var n = document.getElementsByTagName(\"noscript\");"
    // Note that this uses separate loops to walk the noscript NodeList and
    // to modify the DOM as modifying the DOM while walking a collection risks
    // turning the walk quadratic.
    "  var r = [];"
    "  for (var i = 0; i < n.length; ++i) {"
    "    var e = n[i];"
    "    if (e.className == \"psa_add_styles\") {"
    "      r.push(e);"
    "    }"
    "  }"
    "  for (var i = 0; i < r.length; ++i) {"
    "    var e = r[i];"
    "    var div = document.createElement(\"div\");"
    "    div.innerHTML = e.textContent;"
    "    document.body.appendChild(div);"
    "  }"
    "};";
// TODO(ksimbili): Move this to appropriate event instead of 'onload'.
const char CriticalSelectorFilter::kAddStylesInvocation[] =
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
  CssElement(HtmlParse* p, HtmlElement* e, bool inside_noscript)
      : html_parse_(p), element_(p->CloneElement(e)),
        inside_noscript_(inside_noscript) {}

  // HtmlParse deletes the element (regardless of whether it is inserted).
  virtual ~CssElement() {}

  virtual void AppendTo(HtmlElement* parent) const {
    html_parse_->AppendChild(parent, element_);
  }

  bool inside_noscript() const { return inside_noscript_; }

 protected:
  HtmlParse* html_parse_;
  HtmlElement* element_;
  bool inside_noscript_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssElement);
};

// Wrap CSS style blocks to move them later in the document.
class CriticalSelectorFilter::CssStyleElement
    : public CriticalSelectorFilter::CssElement {
 public:
  CssStyleElement(HtmlParse* p, HtmlElement* e, bool inside_noscript)
      : CssElement(p, e, inside_noscript) {}
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
    : CssSummarizerBase(driver) {
}

CriticalSelectorFilter::~CriticalSelectorFilter() {
}

bool CriticalSelectorFilter::MustSummarize(HtmlElement* element) const {
  // Don't summarize non-screen-affecting or <noscript> CSS.  Note that we still
  // need to defer it, which is done by the Notify...Css methods below.
  return (noscript_element() == NULL) &&
      css_util::CanMediaAffectScreen(element->AttributeValue(HtmlName::kMedia));
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

void CriticalSelectorFilter::RenderSummary(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node) {
  const SummaryInfo& summary = GetSummaryForStyle(pos);
  DCHECK_EQ(kSummaryOk, summary.state);

  // If we're inlining an external CSS file, make sure to adjust the URLs
  // inside to the new base.
  const GoogleString* css_to_use = &summary.data;
  GoogleString resolved_css;
  if (summary.is_external) {
    StringWriter writer(&resolved_css);
    GoogleUrl input_css_base(summary.base);
    if (driver_->ResolveCssUrls(
            input_css_base, driver_->base_url().Spec(), summary.data,
            &writer, driver_->message_handler()) == RewriteDriver::kSuccess) {
      css_to_use = &resolved_css;
    }
  }

  // Update the DOM --- either an existing style element, or replace link
  // with style.
  if (char_node != NULL) {
    *char_node->mutable_contents() = *css_to_use;
  } else {
    HtmlElement* style_element = driver_->NewElement(NULL, HtmlName::kStyle);
    driver_->InsertElementBeforeElement(element, style_element);

    HtmlCharactersNode* content =
        driver_->NewCharactersNode(style_element, *css_to_use);
    driver_->AppendChild(style_element, content);
    driver_->DeleteElement(element);
    element = style_element;
  }

  // Update the media attribute to just the media that's relevant to screen.
  StringVector all_media;
  css_util::VectorizeMediaAttribute(summary.media_from_html, &all_media);

  element->DeleteAttribute(HtmlName::kMedia);
  bool drop_entire_element = false;
  if (summary.is_inside_noscript) {
    // Optimize summary version for scriptable environment, since noscript
    // environment will eagerly load the whole CSS anyway at the foot of the
    // page.
    drop_entire_element = true;
  } else if (!all_media.empty()) {
    StringVector relevant_media;
    for (int i = 0, n = all_media.size(); i < n; ++i) {
      const GoogleString& medium = all_media[i];
      if (css_util::CanMediaAffectScreen(medium)) {
        relevant_media.push_back(medium);
      }
    }

    if (!relevant_media.empty()) {
      driver_->AddAttribute(element, HtmlName::kMedia,
                            css_util::StringifyMediaVector(relevant_media));
    } else {
      // None of the media applied to the screen, so remove the entire element.
      drop_entire_element = true;
    }
  }

  if (drop_entire_element) {
    driver_->DeleteElement(element);
  }
}

void CriticalSelectorFilter::NotifyInlineCss(HtmlElement* style_element,
                                             HtmlCharactersNode* content) {
  CssStyleElement* save = new CssStyleElement(driver_, style_element,
                                              noscript_element() != NULL);
  save->AppendCharactersNode(content);
  css_elements_.push_back(save);
  // We can't delete non-screen CSS we skipped here, because
  // the close </style> tag hasn't happened yet.
}

void CriticalSelectorFilter::NotifyExternalCss(HtmlElement* link) {
  css_elements_.push_back(new CssElement(driver_, link,
                                         noscript_element() != NULL));
  if (!MustSummarize(link)) {
    // Treat as if we were rendering a link with no critical css, or inside a
    // <noscript> block, which would cause us to delete the element from the
    // page in RenderSummary above.  In this case we didn't Summarize the link
    // at all, so we need to remove it here.
    driver_->DeleteElement(link);
  }
}

GoogleString CriticalSelectorFilter::CacheKeySuffix() const {
  return cache_key_suffix_;
}

void CriticalSelectorFilter::EndElementImpl(HtmlElement* element) {
  CssSummarizerBase::EndElementImpl(element);
  if (element->keyword() == HtmlName::kStyle && !MustSummarize(element)) {
    // Similar to NotifyExternalCss, we must handle inline <style>s that are in
    // <noscript> or don't affect the screen, which we didn't Summarize but need
    // to delete.
    driver_->DeleteElement(element);
  }
}

void CriticalSelectorFilter::StartDocumentImpl() {
  CssSummarizerBase::StartDocumentImpl();

  // Read critical selector info from pcache.
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

  // Clear state between re-uses / check to make sure we wrapped up properly.
  DCHECK(css_elements_.empty());
}

void CriticalSelectorFilter::EndDocument() {
  CssSummarizerBase::EndDocument();

  // TODO(morlovich): There is some risk that we will end up duplicating
  // CSS and not actually reducing anything on a first load.

  // TODO(morlovich): This still doesn't fix IE-conditional stuff,
  // since we're not repeating it.
  if (!css_elements_.empty()) {
    HtmlElement* noscript_element = NULL;
    for (int i = 0, n = css_elements_.size(); i < n; ++i) {
      // Insert the full CSS, but hide all the style, link tags inside noscript
      // blocks so that look-ahead parser cannot find them; and mark the
      // portions that were visible to scripting-aware browser with
      // class = psa_add_styles.
      //
      // If the browser has scripting off, it will therefore read everything,
      // including portions of original CSS that were in noscript block.
      //
      // If the browser has scripting on, the parser will not do anything, but
      // we will add a loader script which will load things with
      // class = psa_add_styles (thus skipping over things that were originally
      // inside noscript).
      if (i == 0 || (css_elements_[i]->inside_noscript() !=
                     css_elements_[i - 1]->inside_noscript())) {
        noscript_element = driver_->NewElement(NULL, HtmlName::kNoscript);
        if (!css_elements_[i]->inside_noscript()) {
          driver_->AddAttribute(noscript_element, HtmlName::kClass,
                                "psa_add_styles");
        }
        driver_->InsertElementBeforeCurrent(noscript_element);
      }
      css_elements_[i]->AppendTo(noscript_element);
    }

    HtmlElement* script = driver_->NewElement(NULL, HtmlName::kScript);
    driver_->InsertElementBeforeCurrent(script);
    if (driver_->options()
        ->test_only_prioritize_critical_css_dont_apply_original_css()) {
      driver_->server_context()->static_asset_manager()->AddJsToElement(
          kAddStylesFunction, script, driver_);
    } else {
      driver_->server_context()->static_asset_manager()->AddJsToElement(
          StrCat(kAddStylesFunction, kAddStylesInvocation), script, driver_);
    }
  }

  STLDeleteElements(&css_elements_);
}

void CriticalSelectorFilter::DetermineEnabled() {
  // We shouldn't do anything if there is no information on critical selectors
  // in the property cache. Unfortunately, we also cannot run safely in case of
  // IE, since we do not understand IE conditional comments well enough to
  // replicate their behavior in the load-everything section.
  // TODO(morlovich): IE10 in strict mode disables the conditional comments
  // feature; but the strict mode is determined by combination of doctype and
  // X-UA-Compatible, which can come in both meta and header flavors. Once we
  // have a good way of detecting this case, we can enable us for strict IE10.
  // Note: the UA logic should be the same in CriticalCssBeaconFilter.
  bool can_run = (driver_->CriticalSelectors() != NULL) &&
                 !driver_->user_agent_matcher()->IsIe(driver_->user_agent());
  set_is_enabled(can_run);
}

}  // namespace net_instaweb
