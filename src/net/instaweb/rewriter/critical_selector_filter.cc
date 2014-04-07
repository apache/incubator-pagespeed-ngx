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
#include <cstddef>
#include <set>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
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

// When flush early filter is enabled, critical css rules are flushed early
// as innerHTML of a script element. When the CSS element appears in the
// document, find the previously flushed style data and copy it to the style
// element so it can be applied. This script is used for that.
const char CriticalSelectorFilter::kApplyFlushEarlyCss[] =
    "var applyFlushedCriticalCss = function(script_id, mediaString) {"
    "  var scripts = document.getElementsByTagName('script');"
    "  var styleScript = document.getElementById(script_id);"
    "  if (styleScript == null) {"
    "    return;"
    "  }"
    "  var cssText = styleScript.innerHTML || styleScript.textContent || "
    "                styleScript.data || \"\";"
    "  var styleElem = document.createElement('style');"
    "  styleElem.type = 'text/css';"
    "  if (styleElem.styleSheet) {"
    "    styleElem.styleSheet.cssText = cssText;"
    "  } else {"
    "    styleElem.appendChild(document.createTextNode(cssText));"
    "  }"
    "  if (mediaString) {"
    "    styleElem.setAttribute(\"media\", mediaString);"
    "  }"
    "  var currentScript = scripts[scripts.length-1];"
    "  currentScript.parentNode.insertBefore(styleElem, currentScript);"
    "};";

const char CriticalSelectorFilter::kInvokeFlushEarlyCssTemplate[] =
    "applyFlushedCriticalCss(\"%s\", \"%s\");";

const char CriticalSelectorFilter::kMoveScriptId[] = "psa_flush_style_early";
const char CriticalSelectorFilter::kNoscriptStylesClass[] = "psa_add_styles";

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
    : CssSummarizerBase(driver),
      saw_end_document_(false),
      any_rendered_(false),
      is_flush_script_added_(false) {
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

void CriticalSelectorFilter::RenderSummary(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node,
    bool* is_element_deleted) {
  RememberFullCss(pos, element, char_node);

  const SummaryInfo& summary = GetSummaryForStyle(pos);
  DCHECK_EQ(kSummaryOk, summary.state);

  // If we're inlining an external CSS file, make sure to adjust the URLs
  // inside to the new base.
  const GoogleString* css_to_use = &summary.data;
  GoogleString resolved_css;
  if (summary.is_external) {
    StringWriter writer(&resolved_css);
    GoogleUrl input_css_base(summary.base);
    if (driver()->ResolveCssUrls(
            input_css_base, driver()->base_url().Spec(), summary.data,
            &writer, driver()->message_handler()) == RewriteDriver::kSuccess) {
      css_to_use = &resolved_css;
    }
  }

  // Update the DOM --- either an existing style element, or replace link
  // with style.
  if (char_node != NULL) {
    *char_node->mutable_contents() = *css_to_use;
  } else {
    HtmlElement* style_element = driver()->NewElement(NULL, HtmlName::kStyle);
    driver()->InsertNodeBeforeNode(element, style_element);

    HtmlCharactersNode* content =
        driver()->NewCharactersNode(style_element, *css_to_use);
    driver()->AppendChild(style_element, content);
    *is_element_deleted = driver()->DeleteNode(element);
    element = style_element;
  }

  // Update the media attribute to just the media that's relevant to screen.
  StringVector all_media;
  css_util::VectorizeMediaAttribute(summary.media_from_html, &all_media);

  element->DeleteAttribute(HtmlName::kMedia);
  bool drop_entire_element = false;
  if (css_to_use->empty()) {
    // Don't keep empty blocks around.
    drop_entire_element = true;
  } else if (summary.is_inside_noscript) {
    // Optimize summary version for scriptable environment, since noscript
    // environment will eagerly load the whole CSS anyway at the foot of the
    // page.
    drop_entire_element = true;
  } else if (summary.is_external &&
             CssTagScanner::IsAlternateStylesheet(summary.rel)) {
    // Likewise drop alternate stylesheets, they're non-critical.
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
      driver()->AddAttribute(element, HtmlName::kMedia,
                             css_util::StringifyMediaVector(relevant_media));
    } else {
      // None of the media applied to the screen, so remove the entire element.
      drop_entire_element = true;
    }
  }

  if (drop_entire_element) {
    driver()->DeleteNode(element);
  } else if (char_node == NULL) {
    const GoogleString& url = summary.location;
    if (IsCssFlushedEarly(url)) {
      ApplyCssFlushedEarly(element,
                           driver()->server_context()->hasher()->Hash(url),
                           element->AttributeValue(HtmlName::kMedia));
    } else if (driver()->flushing_early()) {
      // Add an attribute so the flush early filter can flush these
      // elements early.
      driver()->AddAttribute(element, HtmlName::kDataPagespeedFlushStyle,
                             driver()->server_context()->hasher()->Hash(url));
    }
  }

  // We've altered the CSS, so we should generate code to load the entire thing.
  // TODO(morlovich): Check if we actually dropped something?
  any_rendered_ = true;
}

void CriticalSelectorFilter::WillNotRenderSummary(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node,
    bool* is_element_deleted) {
  RememberFullCss(pos, element, char_node);
}

GoogleString CriticalSelectorFilter::CacheKeySuffix() const {
  return cache_key_suffix_;
}

void CriticalSelectorFilter::StartDocumentImpl() {
  CssSummarizerBase::StartDocumentImpl();
  ServerContext* context = driver()->server_context();

  // Read critical selector info from pcache.
  critical_selectors_ =
      context->critical_selector_finder()->GetCriticalSelectors(driver());

  // Compute corresponding cache key suffix
  GoogleString all_selectors = JoinCollection(critical_selectors_, ",");
  cache_key_suffix_ = context->lock_hasher()->Hash(all_selectors);

  // Clear state between re-uses / check to make sure we wrapped up properly.
  DCHECK(css_elements_.empty());
  saw_end_document_ = false;
  any_rendered_ = false;
  is_flush_script_added_ = false;
}

void CriticalSelectorFilter::EndDocument() {
  CssSummarizerBase::EndDocument();

  saw_end_document_ = true;
}

void CriticalSelectorFilter::RenderDone() {
  CssSummarizerBase::RenderDone();

  // Only do this on very last flush window.
  if (!saw_end_document_) {
    return;
  }

  if (!css_elements_.empty() && any_rendered_ && !driver()->flushing_early()) {
    HtmlElement* noscript_element = NULL;
    Compact(&css_elements_);
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
        noscript_element = driver()->NewElement(NULL, HtmlName::kNoscript);
        if (!css_elements_[i]->inside_noscript()) {
          driver()->AddAttribute(noscript_element, HtmlName::kClass,
                                 kNoscriptStylesClass);
        }
        InsertNodeAtBodyEnd(noscript_element);
      }
      css_elements_[i]->AppendTo(noscript_element);
    }

    HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
    InsertNodeAtBodyEnd(script);
    GoogleString js =
        driver()->server_context()->static_asset_manager()->GetAsset(
            StaticAssetManager::kCriticalCssLoaderJs, driver()->options());
    if (!driver()->options()
             ->test_only_prioritize_critical_css_dont_apply_original_css()) {
      StrAppend(&js, "pagespeed.CriticalCssLoader.Run();");
    }
    driver()->server_context()->static_asset_manager()->AddJsToElement(
        js, script, driver());
  }

  STLDeleteElements(&css_elements_);
}

void CriticalSelectorFilter::DetermineEnabled() {
  // We shouldn't do anything if there is no information on critical selectors
  // in the property cache. Unfortunately, we also cannot run safely in case of
  // IE, since we do not understand IE conditional comments well enough to
  // replicate their behavior in the load-everything section.
  const StringSet& critical_selectors = driver()->server_context()
      ->critical_selector_finder()->GetCriticalSelectors(driver());
  bool ua_supports_critical_css =
      driver()->request_properties()->SupportsCriticalCss();
  bool can_run = ua_supports_critical_css && !critical_selectors.empty();
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kPrioritizeCriticalCss),
      (can_run ? RewriterHtmlApplication::ACTIVE
               : (ua_supports_critical_css
                      ? RewriterHtmlApplication::PROPERTY_CACHE_MISS
                      : RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED)));
  set_is_enabled(can_run);
}

void CriticalSelectorFilter::RememberFullCss(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node) {
  // Deep copy[1] into the css_elements_ array the CSS as optimized by all the
  // filters that ran before us and rendered their results, so that we can
  // emit it accurately at end, as a lazy-load sequence.
  // [1] We need a deep copy since some of the DOM data will get freed up at the
  //     end of each flush window.
  if (static_cast<size_t>(pos) >= css_elements_.size()) {
    css_elements_.resize(pos + 1);
  }
  bool noscript = GetSummaryForStyle(pos).is_inside_noscript;
  CssElement* save = NULL;
  if (char_node != NULL) {
    CssStyleElement* save_inline =
        new CssStyleElement(driver(), element, noscript);
    save_inline->AppendCharactersNode(char_node);
    save = save_inline;
  } else {
    save = new CssElement(driver(), element, noscript);
  }
  css_elements_[pos] = save;
}

bool CriticalSelectorFilter::IsCssFlushedEarly(const GoogleString& url) const {
  if (!driver()->flushed_early() ||
      !driver()->options()->enable_flush_early_critical_css() ||
      driver()->flush_early_info() == NULL) {
    return false;
  }

  // If the url is present in the DOM cohort, it is guaranteed to have
  // been flushed early.
  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  // TODO(slamm): Replace with cheaper and more robust solution.
  return (driver()->flush_early_info()->resource_html().find(
      StrCat("\"", escaped_url, "\"")) != GoogleString::npos);
}

void CriticalSelectorFilter::ApplyCssFlushedEarly(
    HtmlElement* element, const GoogleString& style_id, const char* media) {
  // In this case we have already added the CSS rules to the head as
  // part of flushing early. Now, find the rule, remove the disabled tag
  // and move it here.

  // Add the JS function definition that moves and applies the flushed early
  // CSS rules, if it has not already been added.
  if (!is_flush_script_added_) {
    is_flush_script_added_ = true;
    HtmlElement* script =
        driver()->NewElement(element->parent(), HtmlName::kScript);
    // TODO(slamm): Remove this attribute and update webdriver test as needed.
    driver()->AddAttribute(script, HtmlName::kId, kMoveScriptId);
    driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
    driver()->InsertNodeBeforeNode(element, script);
    driver()->server_context()->static_asset_manager()->AddJsToElement(
        kApplyFlushEarlyCss, script, driver());
  }

  HtmlElement* script_element =
      driver()->NewElement(element->parent(), HtmlName::kScript);
  driver()->AddAttribute(script_element, HtmlName::kPagespeedNoDefer, "");
  driver()->ReplaceNode(element, script_element);

  GoogleString js_data = StringPrintf(kInvokeFlushEarlyCssTemplate,
                                      style_id.c_str(),
                                      (media != NULL ? media : ""));
  driver()->server_context()->static_asset_manager()->AddJsToElement(
      js_data, script_element, driver());
}

}  // namespace net_instaweb
