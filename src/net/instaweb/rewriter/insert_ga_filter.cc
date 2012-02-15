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
    "%s"  // %s is the optional snippet to increase site speed tracking.
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "var ga = document.createElement('script'); ga.type = 'text/javascript';"
    "ga.async = true;"
    "ga.src = '%s.google-analytics.com/ga.js';"  // %s is the scheme and www/ssl
    "var s = document.getElementsByTagName('script')[0];"
    "s.parentNode.insertBefore(ga, s);"
    "})();";

extern const char kGASpeedTracking[] =
    "_gaq.push(['_setSiteSpeedSampleRate', 10]);";

InsertGAFilter::InsertGAFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      script_element_(NULL),
      added_snippet_element_(NULL),
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

void InsertGAFilter::EndElementImpl(HtmlElement* element) {
  switch (element->keyword()) {
    case HtmlName::kScript:
      // There shouldn't be any "nested" script elements, but just
      // in case, don't reset things if the elements don't match.
      if (element == script_element_) {
        if (FoundSnippetInBuffer()) {
          found_snippet_ = true;
          // If we'd already added a snippet, delete it now.
          // This will only work if the snippet we found is in the same
          // flush window as <head>.  (In theory, it should be since
          // the GA instructions say to put the snippet in head, but
          // of course I'm sure not everyone listens.)
          if (added_snippet_element_ != NULL) {
            driver_->DeleteElement(added_snippet_element_);
            added_snippet_element_ = NULL;
            inserted_ga_snippets_count_->Add(-1);
          }
        }
        script_element_ = NULL;
        buffer_.clear();
      }
      break;
    case HtmlName::kHead:
      // End of head, insert ga here.
      if (!found_snippet_) {
        added_snippet_element_ = driver_->NewElement(element,
                                                     HtmlName::kScript);
        added_snippet_element_->set_close_style(HtmlElement::EXPLICIT_CLOSE);
        driver_->AddAttribute(added_snippet_element_, HtmlName::kType,
                              "text/javascript");
        const char* kSpeedSnippet = increase_speed_tracking_ ?
            kGASpeedTracking : "";
        const char* kUrlPrefix = driver_->google_url().SchemeIs("https") ?
            "https://ssl" : "http://www";
        GoogleString snippet_text = StringPrintf(
            kGASnippet, ga_id_.c_str(), kSpeedSnippet, kUrlPrefix);
        HtmlNode* snippet =
            driver_->NewCharactersNode(added_snippet_element_, snippet_text);
        driver_->AppendChild(element, added_snippet_element_);
        driver_->AppendChild(added_snippet_element_, snippet);

        inserted_ga_snippets_count_->Add(1);
      }
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

// Clear out the added_snippet_element_ here because we don't want to
// try to remove it if it's been flushed out already.
void InsertGAFilter::Flush() {
  added_snippet_element_ = NULL;
}

}  // namespace net_instaweb
