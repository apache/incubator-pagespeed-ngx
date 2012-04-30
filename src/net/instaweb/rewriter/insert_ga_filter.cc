/*
 * Copyright 2011 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)
//
// Implements the insert_ga_snippet filter, which inserts the
// Google Analytics tracking snippet into html pages.

#include "net/instaweb/rewriter/public/insert_ga_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// Name for statistics variable.
const char kInsertedGaSnippets[] = "inserted_ga_snippets";

}  // namespace

namespace net_instaweb {

// Google Analytics async snippet.
extern const char kGASnippet[] =
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"  // %s is the GA account number.
    "_gaq.push(['_setDomainName', '%s']);"  // %s is the domain name
    "_gaq.push(['_setAllowLinker', true]);"
    "%s"  // %s is the optional snippet to increase site speed tracking.
    "%s"  // %s is the Furious Snippet for experiments.
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "var ga = document.createElement('script'); ga.type = 'text/javascript';"
    "ga.async = true;"
    "ga.src = '%s.google-analytics.com/ga.js';"  // %s is the scheme and www/ssl
    "var s = document.getElementsByTagName('script')[0];"
    "s.parentNode.insertBefore(ga, s);"
    "})();";

// Set the sample rate to 100%.
// TODO(nforman): Allow this to be configurable through RewriteOptions.
extern const char kGASpeedTracking[] =
    "_gaq.push(['_setSiteSpeedSampleRate', 100]);";

// The %u is for the variable slot (defaults to 1).
// The %s is for the Experiment spec string.
// This defaults to being a page-scoped variable.
const char kFuriousSnippetFmt[] =
    "_gaq.push(['_setCustomVar', %u, 'FuriousState', '%s']);";

InsertGAFilter::InsertGAFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      script_element_(NULL),
      added_snippet_element_(NULL),
      added_furious_element_(NULL),
      ga_id_(rewrite_driver->options()->ga_id()),
      found_snippet_(false),
      increase_speed_tracking_(
          rewrite_driver->options()->increase_speed_tracking()) {
  Statistics* stats = driver_->statistics();
  inserted_ga_snippets_count_ = stats->GetVariable(kInsertedGaSnippets);
  DCHECK(!ga_id_.empty()) << "Enabled ga insertion, but did not provide ga id.";
}

void InsertGAFilter::Initialize(Statistics* stats) {
  stats->AddVariable(kInsertedGaSnippets);
}

InsertGAFilter::~InsertGAFilter() {}

void InsertGAFilter::StartDocumentImpl() {
  found_snippet_ = false;
  script_element_ = NULL;
  added_snippet_element_ = NULL;
  added_furious_element_ = NULL;
  buffer_.clear();
}

// Start looking for ga snippet.
void InsertGAFilter::StartElementImpl(HtmlElement* element) {
  if (!found_snippet_ && element->keyword() == HtmlName::kScript &&
      script_element_ == NULL) {
    script_element_ = element;
    buffer_.clear();
  }
}

// This may not be exact, but should be a pretty good guess.
// TODO(nforman): Find out if there is a canonical way of determining
// if a script is a GA snippet.
bool InsertGAFilter::FoundSnippetInBuffer() const {
  return
      (buffer_.find(ga_id_) != GoogleString::npos &&
       buffer_.find("setAccount") != GoogleString::npos &&
       (buffer_.find(".google-analytics.com/ga.js") != GoogleString::npos ||
        buffer_.find(".google-analytics.com/urchin.js") != GoogleString::npos));
}

// Running furious: add in the information as the slot 1 custom variable.
// TODO(nforman): Change this to be a label on track_timings
// data when the track_timings api goes live (maybe).
GoogleString InsertGAFilter::ConstructFuriousSnippet() const {
  GoogleString furious = "";
  if (driver_->options()->running_furious()) {
    int furious_state = driver_->options()->furious_id();
    if (furious_state != furious::kFuriousNotSet &&
        furious_state != furious::kFuriousNoExperiment) {
      furious = StringPrintf(kFuriousSnippetFmt,
          driver_->options()->furious_ga_slot(),
          driver_->options()->ToExperimentString().c_str());
    }
  }
  return furious;
}

void InsertGAFilter::AddScriptNode(HtmlElement* parent,
                                   const GoogleString& text,
                                   HtmlElement** script_element) const {
  *script_element = driver_->NewElement(parent,
                                        HtmlName::kScript);
  (*script_element)->set_close_style(HtmlElement::EXPLICIT_CLOSE);
  driver_->AddAttribute(*script_element, HtmlName::kType,
                        "text/javascript");
  HtmlNode* snippet =
      driver_->NewCharactersNode(*script_element, text);
  driver_->AppendChild(parent, *script_element);
  driver_->AppendChild(*script_element, snippet);
}

GoogleString InsertGAFilter::MakeFullFuriousSnippet() const {
  GoogleString furious = ConstructFuriousSnippet();
  if (!furious.empty()) {
    // Always increase speed tracking to 100% for Furious.
    StrAppend(&furious, kGASpeedTracking, "_gaq.push(['_trackPageview']);");
  }
  return furious;
}

// Handle the end of a head tag.
//
// If we've already inserted any GA snippet, don't do anything.
//
// If we found a GA snippet in the original page, add a furious
// snippet only (if we're running furious).
//
// If we haven't found anything, and haven't inserted anything yet,
// insert a GA snippet which includes the furious tracking code.
void InsertGAFilter::HandleEndHead(HtmlElement* head) {
  // There is a chance (e.g. if there are two heads), that we have
  // already inserted the snippet.  In that case, don't do it again.
  if (added_snippet_element_ != NULL || added_furious_element_ != NULL) {
    return;
  }

  if (found_snippet_) {
    // We found a snippet, but we now need to set the custom variable.
    // We also need to send a trackPageview request after the variable
    // has been set.
    GoogleString furious = MakeFullFuriousSnippet();
    if (!furious.empty()) {
      AddScriptNode(head, furious, &added_furious_element_);
    }
    return;
  }
  // No snippets have been found, and we haven't added any snippets
  // yet, so add one now.

  // Domain for this html page.
  GoogleString domain = driver_->google_url().Host().as_string();
  // HTTP vs. HTTPS - these are usually determined on the fly by js
  // in the ga snippet, but it's faster to determine it here.
  const char* kUrlPrefix = driver_->google_url().SchemeIs("https") ?
      "https://ssl" : "http://www";

  // This will be empty if we're not running furious.
  GoogleString furious = ConstructFuriousSnippet();

  // Increase the percentage of traffic for which we track page load time.
  GoogleString speed_snippet =
      (!furious.empty() || increase_speed_tracking_) ? kGASpeedTracking : "";

  // Full Snippet
  GoogleString snippet_text = StringPrintf(
      kGASnippet, ga_id_.c_str(), domain.c_str(),
      speed_snippet.c_str(), furious.c_str(), kUrlPrefix);
  AddScriptNode(head, snippet_text, &added_snippet_element_);
  inserted_ga_snippets_count_->Add(1);
  return;
}

// Handle the end of a script tag.
// Look for a GA snippet in the script.
// If we find one, remove any GA snippets we've added already.
// If we're running furious, and if we had to remove a snippet,
// we need to add back in the furious part only.
void InsertGAFilter::HandleEndScript(HtmlElement* script) {
  // There shouldn't be any "nested" script elements, but just
  // in case, don't reset things if the elements don't match.
  // Don't bother to look in here if we already found a snippet.
  // The buffer should also be empty in that case.
  if (script == script_element_ && !found_snippet_) {
    if (FoundSnippetInBuffer()) {
      found_snippet_ = true;
      // If we'd already added a snippet, delete it now.
      // This will only work if the snippet we found is in the same
      // flush window as <head>.  (In theory, it should be since
      // the GA instructions say to put the snippet in head, but
      // of course I'm sure not everyone listens.)
      if (added_snippet_element_ != NULL) {
        if (!driver_->DeleteElement(added_snippet_element_)) {
          LOG(INFO) <<
              "Tried to delete GA element, but it was already flushed.";
        } else {
          added_snippet_element_ = NULL;
          inserted_ga_snippets_count_->Add(-1);
          // If we deleted the snippet, and we're running furious, we now need
          // to add back in the furious bit.
          GoogleString furious = MakeFullFuriousSnippet();
          if (!furious.empty()) {
            AddScriptNode(script->parent(), furious,
                          &added_furious_element_);
          }
        }
      }
    }
    script_element_ = NULL;
    buffer_.clear();
  }
}

void InsertGAFilter::EndElementImpl(HtmlElement* element) {
  switch (element->keyword()) {
    case HtmlName::kScript:
      HandleEndScript(element);
      break;
    case HtmlName::kHead:
      HandleEndHead(element);
      break;
    default:
      break;
  }
}

void InsertGAFilter::Characters(HtmlCharactersNode* characters) {
  if (script_element_ != NULL && !found_snippet_) {
    buffer_ += characters->contents();
  }
}

}  // namespace net_instaweb
