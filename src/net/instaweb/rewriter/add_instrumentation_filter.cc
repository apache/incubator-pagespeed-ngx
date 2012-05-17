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

// Author: abliss@google.com (Adam Bliss)

#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"  // for ContentType
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// The javascript tag to insert in the top of the <head> element.  We want this
// as early as possible in the html.  It must be short and fast.
const char kHeadScript[] =
    "<script type='text/javascript'>"
    "window.mod_pagespeed_start = Number(new Date());"
    "</script>";

// The javascript tag to insert at the bottom of head.  The first %s will
// be replaced with the custom beacon url, by default
// "./mod_pagespeed_beacon?ets=".  The second %s will be replaced by
// kUnoadTag.
//
//  Then our timing info, e.g. "unload:123", will be appended.
const char kUnloadScriptFormat[] =
    "<script type='text/javascript'>"
    "(function(){function g(){"
    "if(window.mod_pagespeed_loaded) {return;}"
    "var ifr=0;"
    "if(window.parent != window){ifr=1}"
    "new Image().src='%s%s'+"
    "(Number(new Date())-window.mod_pagespeed_start)+'&ifr='+ifr+'"
    "&url='+encodeURIComponent('%s');};"
    "var f=window.addEventListener;if(f){f('beforeunload',g,false);}else{"
    "f=window.attachEvent;if(f){f('onbeforeunload',g);}}"
    "})();</script>";

// The javascript tag to insert at the bottom of document.  The first %s will
// be replaced with the custom beacon url, by default
// "./mod_pagespeed_beacon?ets=".  The second %s will be replaced by kLoadTag.
//
// Then our timing info, e.g. "load:123", will be appended.
const char kTailScriptFormat[] =
    "<script type='text/javascript'>"
    "(function(){function g(){var ifr=0;"
    "if(window.parent != window){ifr=1}"
    "new Image().src='%s%s'+"
    "(Number(new Date())-window.mod_pagespeed_start)+'&ifr='+ifr+'"
    "&url='+encodeURIComponent('%s');"
    "window.mod_pagespeed_loaded=true;};"
    "var f=window.addEventListener;if(f){f('load',g,false);}else{"
    "f=window.attachEvent;if(f){f('onload',g);}}"
    "})();</script>";

}  // namespace

// Timing tag for total page load time.  Also embedded in kTailScriptFormat
// above via the second %s.
const char AddInstrumentationFilter::kLoadTag[] = "load:";
const char AddInstrumentationFilter::kUnloadTag[] = "unload:";
GoogleString* AddInstrumentationFilter::kTailScriptFormatXhtml = NULL;
GoogleString* AddInstrumentationFilter::kUnloadScriptFormatXhtml = NULL;

// Counters.
const char AddInstrumentationFilter::kInstrumentationScriptAddedCount[] =
    "instrumentation_filter_script_added_count";
AddInstrumentationFilter::AddInstrumentationFilter(
    RewriteDriver* driver, const StringPiece& beacon_url)
    : driver_(driver),
      found_head_(false) {
  beacon_url.CopyToString(&beacon_url_);
  beacon_url.CopyToString(&xhtml_beacon_url_);
  GlobalReplaceSubstring("&", "&amp;", &xhtml_beacon_url_);
  Statistics* stats = driver->resource_manager()->statistics();
  instrumentation_script_added_count_ = stats->GetVariable(
      kInstrumentationScriptAddedCount);
}

AddInstrumentationFilter::~AddInstrumentationFilter() {}

void AddInstrumentationFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kInstrumentationScriptAddedCount);
  if (kTailScriptFormatXhtml == NULL) {
    kTailScriptFormatXhtml = new GoogleString(kTailScriptFormat);
    GlobalReplaceSubstring("&", "&amp;", kTailScriptFormatXhtml);
    kUnloadScriptFormatXhtml = new GoogleString(kUnloadScriptFormat);
    GlobalReplaceSubstring("&", "&amp;", kUnloadScriptFormatXhtml);
  }
}

void AddInstrumentationFilter::Terminate() {
  delete kTailScriptFormatXhtml;
  kTailScriptFormatXhtml = NULL;
  delete kUnloadScriptFormatXhtml;
  kUnloadScriptFormatXhtml = NULL;
}

void AddInstrumentationFilter::StartDocument() {
  found_head_ = false;
}

void AddInstrumentationFilter::StartElement(HtmlElement* element) {
  if (!found_head_) {
    if (element->keyword() == HtmlName::kHead) {
      found_head_ = true;
      // TODO(abliss): add an actual element instead, so other filters can
      // rewrite this JS
      HtmlCharactersNode* script =
          driver_->NewCharactersNode(element, kHeadScript);
      driver_->InsertElementAfterCurrent(script);
      instrumentation_script_added_count_->Add(1);
    }
  }
}

// TODO(jmarantz): In mod_pagespeed, the output_filter gets run prior to
// mod_headers, so we may not know the correct mimetype at the time we run.
//
// When run in this mode, we should use this hack from
//     http://stackoverflow.com/questions/2375217
//     should-i-use-or-for-closing-a-cdata-section-into-xhtml

//  <script type="text/javascript">//<![CDATA[
//     INSTRUMENTATION CODE
//  //]]></script>
//
// This will be done in a follow-up.
bool AddInstrumentationFilter::IsXhtml() {
  bool is_xhtml = false;
  const ResponseHeaders* headers = driver_->response_headers();
  if (headers != NULL) {
    const ContentType* content_type = headers->DetermineContentType();
    if (content_type != NULL) {
      is_xhtml = content_type->IsXmlLike();
    }
  }
  return is_xhtml;
}

void AddInstrumentationFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody) {
    // We relied on the existence of a <head> element.  This should have been
    // assured by add_head_filter.
    CHECK(found_head_) << "Reached end of document without finding <head>."
        "  Please turn on the add_head filter.";
    bool is_xhtml = IsXhtml();
    const char* script = is_xhtml
        ? kTailScriptFormatXhtml->c_str() : kTailScriptFormat;
    AddScriptNode(element, script, kLoadTag, is_xhtml);
  } else if (found_head_ && element->keyword() == HtmlName::kHead &&
             driver_->options()->report_unload_time()) {
    bool is_xhtml = IsXhtml();
    const char* script = is_xhtml
        ? kUnloadScriptFormatXhtml->c_str() : kUnloadScriptFormat;
    AddScriptNode(element, script, kUnloadTag, is_xhtml);
  }
}

void AddInstrumentationFilter::AddScriptNode(HtmlElement* element,
                                             const GoogleString& script_format,
                                             const GoogleString& tag_name,
                                             bool is_xhtml) {
  GoogleString html_url(driver_->google_url().Spec().as_string());
  GoogleString tail_script = StringPrintf(
      script_format.c_str(),
      is_xhtml ? xhtml_beacon_url_.c_str() : beacon_url_.c_str(),
      tag_name.c_str(), html_url.c_str());
  HtmlCharactersNode* script =
      driver_->NewCharactersNode(element, tail_script);
  driver_->InsertElementBeforeCurrent(script);
}

}  // namespace net_instaweb
