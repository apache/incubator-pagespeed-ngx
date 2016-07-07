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

// Authors: nforman@google.com (Naomi Forman)
//          jefftk@google.com (Jeff Kaufman)
//
// Implements the insert_ga_snippet filter, which inserts the Google Analytics
// tracking snippet into html pages.  When experiments are enabled, also inserts
// snippets to report experiment status back.

#include "net/instaweb/rewriter/public/insert_ga_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

namespace {

// Name for statistics variable.
const char kInsertedGaSnippets[] = "inserted_ga_snippets";

}  // namespace

namespace net_instaweb {

// This filter primarily exists to support PageSpeed experiments that report
// back to Google Analytics for reporting.  You can also use it just to insert
// the Google Analytics tracking snippet, though.
//
// GA had a rewrite recently, switching from ga.js to analytics.js with a new
// API.  They also released support for content experiments.  The older style of
// reporting is to use a custom variable.  This filter can report to a content
// experiment with either ga.js or analytics.js; with ga.js reporting to a
// custom variable is still supported.
//
// If no GA snippet is present on the page then PageSpeed will insert one.
// Additionally, if you're running an experiment then PageSpeed will insert the
// JS necessary to report details back to GA.  This can look like any of these
// three things:
//
// ga.js + custom variables:
//   <script>kGAExperimentSnippet
//           kGAJsSnippet</script> [ possibly existing ]
//
// ga.js + content experiments:
//   <script src="kContentExperimentsJsClientUrl"></script>
//   <script>kContentExperimentsSetChosenVariationSnippet
//           kGAJsSnippet</script> [ possibly existing ]
//
// analytics.js + content experiments:
//   <script>kAnalyticsJsSnippet</script> [ possibly existing ]
//   kContentExperimentsSetExpAndVariantSnippet goes inside the analytics js
//   snippet, just before the ga(send, pageview) call.

// Google Analytics snippet for setting experiment related variables.  Use with
// old ga.js and custom variable experiment reporting. Arguments are:
//   %s: Optional snippet to increase site speed tracking.
//   %u: Which ga.js custom variable to support to.
//   %s: Experiment spec string, shown in the GA UI.
extern const char kGAExperimentSnippet[] =
    "var _gaq = _gaq || [];"
    "%s"
    "_gaq.push(['_setCustomVar', %u, 'ExperimentState', '%s'"
    "]);";

// Google Analytics async snippet along with the _trackPageView call.
extern const char kGAJsSnippet[] =
    "if (window.parent == window) {"
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"  // %s is the GA account number.
    "_gaq.push(['_setDomainName', '%s']);"  // %s is the domain name
    "_gaq.push(['_setAllowLinker', true]);"
    "%s"  // Optional snippet to increase site speed tracking.
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "var ga = document.createElement('script'); ga.type = 'text/javascript';"
    "ga.async = true;"
    "ga.src = 'https://ssl.google-analytics.com/ga.js';"
    "var s = document.getElementsByTagName('script')[0];"
    "s.parentNode.insertBefore(ga, s);"
    "})();"
    "}";

// Google Universal analytics snippet.  First argument is the GA account number,
// second is kContentExperimentsSetExpAndVariantSnippet or nothing.
extern const char kAnalyticsJsSnippet[] =
    "if (window.parent == window) {"
    "(function(i,s,o,g,r,a,m){"
    "i['GoogleAnalyticsObject']=r;"
    "i[r]=i[r]||function(){"
    "(i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();"
    "a=s.createElement(o), m=s.getElementsByTagName(o)[0];"
    "a.async=1;a.src=g;m.parentNode.insertBefore(a,m)"
    "})(window,document,'script',"
    "'//www.google-analytics.com/analytics.js','ga');"
    "ga('create', '%s', 'auto'%s);"
    "%s"
    "ga('send', 'pageview');"
    "}";

// Increase site speed tracking to 100% when using analytics.js
// Use the first one if we're inserting the snippet, or if the site we're
// modifying isn't already using a fields object with ga('create'), the second
// one if there is an existing snippet with a fields object.
extern const char kAnalyticsJsIncreaseSiteSpeedTracking[] =
    ", {'siteSpeedSampleRate': 100}";
extern const char kAnalyticsJsIncreaseSiteSpeedTrackingMinimal[] =
    "'siteSpeedSampleRate': 100,";

// When using content experiments with ga.js you need to do a sychronous load
// of /cx/api.js first.
extern const char kContentExperimentsJsClientUrl[] =
    "//www.google-analytics.com/cx/api.js";

// When using content experiments with ga.js, after /cx/api.js has loaded and
// before ga.js loads you need to call this.  The first argument is the
// variant id, the second is the experiment id.
extern const char kContentExperimentsSetChosenVariationSnippet[] =
    "cxApi.setChosenVariation(%d, '%s');";

// When using content experiments with ga.js, the variant ID must be numeric.
// If the user requests a non-numeric variant with ga.js, we inject this
// comment. The string is bracketed with newlines because otherwise it's
// invisible in a wall of JavaScript.
extern const char kContentExperimentsNonNumericVariantComment[] =
    "\n/* mod_pagespeed cannot inject experiment variant '%s' "
    "because it's not a number */\n";

// When using content experiments with analytics.js, after ga('create', ..._)
// and before ga('[...].send', 'pageview'), we need to insert:
extern const char kContentExperimentsSetExpAndVariantSnippet[] =
    "ga('set', 'expId', '%s');"
    "ga('set', 'expVar', '%s');";

// Set the sample rate to 100%.
// TODO(nforman): Allow this to be configurable through RewriteOptions.
extern const char kGASpeedTracking[] =
    "_gaq.push(['_setSiteSpeedSampleRate', 100]);";

InsertGAFilter::InsertGAFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      script_element_(NULL),
      added_analytics_js_(false),
      added_experiment_snippet_(false),
      ga_id_(rewrite_driver->options()->ga_id()),
      found_snippet_(false),
      increase_speed_tracking_(
          rewrite_driver->options()->increase_speed_tracking()),
      seen_sync_ga_js_(false) {
  Statistics* stats = driver()->statistics();
  inserted_ga_snippets_count_ = stats->GetVariable(kInsertedGaSnippets);
  DCHECK(!ga_id_.empty()) << "Enabled ga insertion, but did not provide ga id.";
}

void InsertGAFilter::InitStats(Statistics* stats) {
  stats->AddVariable(kInsertedGaSnippets);
}

InsertGAFilter::~InsertGAFilter() {}

bool InsertGAFilter::StringLiteralMatches(StringPiece literal,
                                          StringPiece desired) {
  // Literal includes the beginning and ending quotes; need to exclude them.
  return literal.substr(1, literal.size() - 2) == desired;
}

bool InsertGAFilter::StringLiteralEndsWith(StringPiece literal,
                                           StringPiece desired) {
  // Literal includes the beginning and ending quotes; need to exclude them.
  return literal.substr(1, literal.size() - 2).ends_with(desired);
}

void InsertGAFilter::StartDocumentImpl() {
  found_snippet_ = false;
  script_element_ = NULL;
  added_analytics_js_ = false;
  added_experiment_snippet_ = false;
  if (driver()->options()->running_experiment()) {
    driver()->message_handler()->Message(
        kInfo, "run_experiment: %s",
        driver()->options()->ToExperimentDebugString().c_str());
  }
}

// Start looking for ga snippet.
void InsertGAFilter::StartElementImpl(HtmlElement* element) {
  if (!ga_id_.empty() &&
      !found_snippet_ &&
      element->keyword() == HtmlName::kScript &&
      script_element_ == NULL) {
    script_element_ = element;
  }
}

// This isn't perfect but matches all the cases we've found.  It's ok if it has
// some false positives; the later check is more thorough.
InsertGAFilter::AnalyticsStatus InsertGAFilter::FindSnippetInScript(
    const GoogleString& s) {
  // dc.js is a synonym for old-style ga.js
  if (!seen_sync_ga_js_ &&
      (s.find("google-analytics.com/ga.js") != GoogleString::npos ||
       s.find("stats.g.doubleclick.net/dc.js") != GoogleString::npos)) {
    // The synchronous snippet has two parts: first one with
    // [google-analytics.com/ga.js] (no initial dot) and then a later one with
    // ga_id, _getTracker, and _trackPageview.  Track that we've seen what is
    // probably the first snippet, and then if we later get what could be the
    // second snippet we'll check below.
    seen_sync_ga_js_ = true;
  }
  if (s.find(StrCat("'", ga_id_, "'")) == GoogleString::npos &&
      s.find(StrCat("\"", ga_id_, "\"")) == GoogleString::npos) {
    return kNoSnippetFound;
  }
  if (s.find(".google-analytics.com/urchin.js") != GoogleString::npos) {
    return kUnusableSnippetFound;  // urchin.js is too old.
  } else if (s.find(".google-analytics.com/ga.js") != GoogleString::npos ||
             s.find("stats.g.doubleclick.net/dc.js") != GoogleString::npos) {
    // With the async snippet there is one part that first loads ga.js
    // (using [.google-analytics.com/ga.js], with initial dot) and then has the
    // ga_id (which we checked for above).
    return kGaJs;  // Asynchronous ga.js
  } else if (seen_sync_ga_js_ &&
             s.find("_getTracker") != GoogleString::npos &&
             s.find("_trackPageview") != GoogleString::npos) {
    // Synchronous ga.js was split over two script tags: first one to do the
    // loading then one to do the initialization and page tracking.  We want to
    // process the second one.
    return kGaJs;  // Syncronous ga.js
  } else if (s.find(".google-analytics.com/analytics.js")) {
    return kAnalyticsJs;
  }
  return kUnusableSnippetFound;
}

GoogleString InsertGAFilter::AnalyticsJsExperimentSnippet() const {
  return StringPrintf(
      kContentExperimentsSetExpAndVariantSnippet,
      driver()->options()->content_experiment_id().c_str(),
      driver()->options()->content_experiment_variant_id().c_str());
}

GoogleString InsertGAFilter::GaJsExperimentSnippet() const {
  // ga.js requires a numeric variant id. Attempt to convert the string
  // variant ID to int and use that.
  const char* variant_id =
      driver()->options()->content_experiment_variant_id().c_str();
  int numeric_variant_id;
  if (StringToInt(variant_id, &numeric_variant_id)) {
    return StringPrintf(
        kContentExperimentsSetChosenVariationSnippet, numeric_variant_id,
        driver()->options()->content_experiment_id().c_str());
  } else {
    // Variant ID was non-numeric, so inject a warning.
    return StringPrintf(kContentExperimentsNonNumericVariantComment,
                        variant_id);
  }
}

// * If we've already inserted any GA snippet or if we found a GA snippet in the
//   original page, don't do anything.
// * If we haven't found anything, and haven't inserted anything yet, insert the
//   GA js snippet.
//
// Caveat: The snippet should ideally be placed in <head> for accurate
// collection of data (e.g. pageviews etc.). We place it at the end of the
// document so that we won't add duplicate analytics js code for any page.
//
// For pages which don't already have analytics js, this might result in some
// data being lost.
void InsertGAFilter::EndDocument() {
  if (found_snippet_ || added_analytics_js_ || ga_id_.empty()) {
    return;
  }

  // No snippets have been found, and we haven't added any snippets yet, so add
  // one now.  Include experiment setup if experiments are on.

  GoogleString js_text;
  GoogleString experiment_snippet;
  const char* speed_tracking = "";
  if (driver()->options()->use_analytics_js()) {
    if (increase_speed_tracking_) {
      speed_tracking = kAnalyticsJsIncreaseSiteSpeedTracking;
    }
    if (ShouldInsertExperimentTracking(true /* analytics.js */)) {
      experiment_snippet = AnalyticsJsExperimentSnippet();
    }
    js_text = StringPrintf(
        kAnalyticsJsSnippet,
        ga_id_.c_str(),
        speed_tracking,
        experiment_snippet.c_str());
  } else {
    if (ShouldInsertExperimentTracking(false /* ga.js */)) {
      if (driver()->options()->is_content_experiment()) {
        HtmlElement* cxapi = driver()->NewElement(NULL, HtmlName::kScript);
        driver()->AddAttribute(
            cxapi, HtmlName::kSrc, kContentExperimentsJsClientUrl);
        InsertNodeAtBodyEnd(cxapi);
        experiment_snippet = GaJsExperimentSnippet();
      } else {
        experiment_snippet = StringPrintf(
            kGAExperimentSnippet,
            "" /* don't change speed tracking here, we add it below */,
            driver()->options()->experiment_ga_slot(),
            driver()->options()->ToExperimentString().c_str());
      }
    }

    // Domain for this html page.
    GoogleString domain = driver()->google_url().Host().as_string();
    if (increase_speed_tracking_) {
      speed_tracking = kGASpeedTracking;
    }
    js_text = StrCat(experiment_snippet,
                     StringPrintf(kGAJsSnippet,
                                  ga_id_.c_str(),
                                  domain.c_str(),
                                  speed_tracking));
  }

  HtmlElement* script_element = driver()->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script_element);
  HtmlNode* snippet = driver()->NewCharactersNode(script_element, js_text);
  driver()->AppendChild(script_element, snippet);

  added_analytics_js_ = true;
  inserted_ga_snippets_count_->Add(1);
}

bool InsertGAFilter::ShouldInsertExperimentTracking(bool is_analytics_js) {
  if (driver()->options()->running_experiment()) {
    if (is_analytics_js && !driver()->options()->is_content_experiment()) {
      driver()->WarningHere("Experiment framework requires a content experiment"
                            " when used with analytics.js.");
      return false;
    }

    int experiment_state = driver()->options()->experiment_id();
    if (experiment_state != experiment::kExperimentNotSet &&
        experiment_state != experiment::kNoExperiment) {
      return true;
    }
  }
  return false;
}

void InsertGAFilter::RewriteInlineScript(HtmlCharactersNode* characters) {
  AnalyticsStatus analytics_status =
      FindSnippetInScript(characters->contents());
  if (analytics_status == kNoSnippetFound) {
    return;  // This inline script isn't for GA; nothing to change.
  }

  found_snippet_ = true;

  if (!ShouldInsertExperimentTracking(analytics_status == kAnalyticsJs)) {
    return;  // GA script found, but we don't need to change it.
  }

  if (analytics_status == kUnusableSnippetFound) {
    driver()->InfoHere("Page contains unusual Google Analytics snippet that"
                       " we're not able to modify to add experiment tracking.");
    return;
  }

  if (analytics_status == kAnalyticsJs) {
    GoogleString rewritten;
    StringPiece token;
    pagespeed::JsKeywords::Type token_type;
    pagespeed::js::JsTokenizer tokenizer(
        server_context()->js_tokenizer_patterns(), characters->contents());
    ParseState state = kInitial;

    // Go through the tokens, appending them to rewritten.  We need to find the
    // ga(create) call so we can increase speed tracking.  Then we need to find
    // the ga(send pageview) call so we can insert our experiment snippet.

    // When we find a ga(send pageview) call it won't be obvious what we've
    // found until we're several tokens along.  So save the offset of each ga
    // function call when we find it so we can later insert before if need be.
    int ga_send_pageview_offset = -1;
    bool inserted_speed_tracking = false;

    while ((token_type = tokenizer.NextToken(&token)) !=
           pagespeed::JsKeywords::kEndOfInput) {
      if (token_type == pagespeed::JsKeywords::kError) {
        driver()->InfoHere("Got invalid js in Google Analytics snippet");
        return;
      }
      if (token_type == pagespeed::JsKeywords::kComment ||
          token_type == pagespeed::JsKeywords::kWhitespace ||
          token_type == pagespeed::JsKeywords::kLineSeparator) {
        // All states allow these, so stay in the same state.  kLineSeparator is
        // specifically for newlines that don't trigger semicolon insertion.
      } else if (state == kInitial &&
                 token_type == pagespeed::JsKeywords::kIdentifier &&
                 token == "ga") {
        ga_send_pageview_offset = rewritten.size();
        state = kGotGa;
      } else if (state == kGotGa &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 token == "(") {
        state = kGotGaFuncCall;
      } else if (state == kGotGaFuncCall &&
                 token_type == pagespeed::JsKeywords::kStringLiteral &&
                 StringLiteralMatches(token, "create")) {
        state = kGotGaCreate;
      } else if (state == kGotGaFuncCall &&
                 token_type == pagespeed::JsKeywords::kStringLiteral &&
                 (StringLiteralMatches(token, "send") ||
                  StringLiteralEndsWith(token, ".send"))) {
        state = kGotGaSend;
      } else if (state == kGotGaCreate &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 token == ",") {
        state = kGotGaCreateComma;
      } else if (state == kGotGaCreate &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 token == ")") {
        // Saw end of function call without any fields object.  Insert
        // standard speed tracking here.
        if (increase_speed_tracking_) {
          rewritten.append(kAnalyticsJsIncreaseSiteSpeedTracking);
          inserted_speed_tracking = true;
        }
        state = kInitial;
      } else if (state == kGotGaCreateComma &&
                 token_type == pagespeed::JsKeywords::kStringLiteral) {
        // Ignore any string arguments after create, just let them pass.
        state = kGotGaCreate;
      } else if (state == kGotGaCreateComma &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 token == "{") {
        state = kGotFieldsObject;
      } else if (state == kGotFieldsObject) {
        // Add our field setting before any of the others.
        if (increase_speed_tracking_) {
          rewritten.append(kAnalyticsJsIncreaseSiteSpeedTrackingMinimal);
          inserted_speed_tracking = true;
        }
        state = kInitial;
      } else if (state == kGotGaSend &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 token == ",") {
        state = kGotGaSendComma;
      } else if (state == kGotGaSendComma &&
                 token_type == pagespeed::JsKeywords::kStringLiteral &&
                 StringLiteralMatches(token, "pageview")) {
        state = kGotGaSendPageview;
      } else if (state == kGotGaSendPageview &&
                 token_type == pagespeed::JsKeywords::kOperator &&
                 (token == "," || token == ")")) {
        CHECK(ga_send_pageview_offset != -1);
        rewritten.insert(ga_send_pageview_offset,
                         AnalyticsJsExperimentSnippet());
        state = kSuccess;
      } else if (state == kSuccess) {
        // Pass the remaining tokens through, we already made our changes.
      } else {
        // Any token we weren't expecting puts us back into looking for "ga".
        state = kInitial;
      }

      rewritten.append(token.as_string());
    }
    if (state == kSuccess) {
      (*characters->mutable_contents()) = rewritten;
      added_experiment_snippet_ = true;

      if (increase_speed_tracking_ && !inserted_speed_tracking) {
        driver()->InfoHere("Failed to increase site speed tracking.");
      }
    } else {
      driver()->InfoHere(
          "Failed to add experiment tracking to existing snippet.");
    }
  } else {
    DCHECK(analytics_status == kGaJs);

    if (driver()->options()->is_content_experiment()) {
      // The API for content experiments with ga.js unfortunately requires a
      // synchronous script load first.  Ideally people would switch to
      // analytics.js, which doesn't have this problem, but we need to support
      // people who haven't switched as well.
      //
      // We can't do InsertBeforeCurrent here, because we could be in the
      // horrible case where "<script>" has been flushed and now we're
      // rewriting the script body.  So the best we can do is:
      // * Blank out this script.
      // * Append the blocking external script load.
      // * Append the edited body of the original script tag as a new
      //   inline script.
      postponed_script_body_ = characters->contents();
      characters->mutable_contents()->clear();
    } else {
      const char* speed_tracking =
          increase_speed_tracking_ ? kGASpeedTracking : "";
      GoogleString snippet_text = StringPrintf(
          kGAExperimentSnippet,
          speed_tracking,
          driver()->options()->experiment_ga_slot(),
          driver()->options()->ToExperimentString().c_str());
      GoogleString* script = characters->mutable_contents();
      // Prepend snippet_text to the script block.
      script->insert(0, snippet_text);
      added_experiment_snippet_ = true;
    }
  }
}

// If RewriteInlineScript decided to insert any new script nodes, do that
// insertion here.
void InsertGAFilter::HandleEndScript(HtmlElement* script) {
  if (!postponed_script_body_.empty()) {
    DCHECK(script == script_element_);
    driver()->InsertScriptAfterCurrent(
        kContentExperimentsJsClientUrl, true /* external */);
    driver()->InsertScriptAfterCurrent(
        StrCat(GaJsExperimentSnippet(), postponed_script_body_),
        false /* inline */);
    added_experiment_snippet_ = true;
    postponed_script_body_.clear();
  }
  script_element_ = NULL;
}

void InsertGAFilter::EndElementImpl(HtmlElement* element) {
  if (ga_id_.empty()) {
    // We only DCHECK that it's non-empty above, but there's nothing useful we
    // can do if it hasn't been set.  Checking here means we'll make no changes.
    return;
  }
  if (element->keyword() == HtmlName::kScript) {
    HandleEndScript(element);
  }
}

void InsertGAFilter::Characters(HtmlCharactersNode* characters) {
  if (script_element_ != NULL && !found_snippet_ &&
      !added_experiment_snippet_) {
    RewriteInlineScript(characters);
  }
}

}  // namespace net_instaweb
