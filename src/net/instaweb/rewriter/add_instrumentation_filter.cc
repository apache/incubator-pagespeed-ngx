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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
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
    "(Number(new Date())-window.mod_pagespeed_start)+'&amp;ifr='+ifr+'"
    "&amp;url='+encodeURIComponent('%s');};"
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
    "(Number(new Date())-window.mod_pagespeed_start)+'&amp;ifr='+ifr+'"
    "&amp;url='+encodeURIComponent('%s');"
    "window.mod_pagespeed_loaded=true;};"
    "var f=window.addEventListener;if(f){f('load',g,false);}else{"
    "f=window.attachEvent;if(f){f('onload',g);}}"
    "})();</script>";

}  // namespace

// Timing tag for total page load time.  Also embedded in kTailScriptFormat
// above via the second %s.
const char AddInstrumentationFilter::kLoadTag[] = "load:";

const char AddInstrumentationFilter::kUnloadTag[] = "unload:";

// Counters.
const char AddInstrumentationFilter::kInstrumentationScriptAddedCount[] =
    "instrumentation_filter_script_added_count";
AddInstrumentationFilter::AddInstrumentationFilter(
    RewriteDriver* driver, const StringPiece& beacon_url)
    : driver_(driver),
      found_head_(false) {
  beacon_url.CopyToString(&beacon_url_);
  Statistics* stats = driver->resource_manager()->statistics();
  instrumentation_script_added_count_ = stats->GetVariable(
      kInstrumentationScriptAddedCount);
}

AddInstrumentationFilter::~AddInstrumentationFilter() {}

void AddInstrumentationFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kInstrumentationScriptAddedCount);
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

void AddInstrumentationFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody) {
    // We relied on the existence of a <head> element.  This should have been
    // assured by add_head_filter.
    CHECK(found_head_) << "Reached end of document without finding <head>."
        "  Please turn on the add_head filter.";
    AddScriptNode(element, kTailScriptFormat, kLoadTag);
  } else if (found_head_ && element->keyword() == HtmlName::kHead &&
             driver_->options()->report_unload_time()) {
    AddScriptNode(element, kUnloadScriptFormat, kUnloadTag);
  }
}

void AddInstrumentationFilter::AddScriptNode(HtmlElement* element,
                                             const GoogleString& script_format,
                                             const GoogleString& tag_name) {
    GoogleString html_url(driver_->google_url().Spec().as_string());
    GoogleString tail_script = StringPrintf(
        script_format.c_str(), beacon_url_.c_str(), tag_name.c_str(),
        html_url.c_str());
    HtmlCharactersNode* script =
        driver_->NewCharactersNode(element, tail_script);
    driver_->InsertElementBeforeCurrent(script);
}

}  // namespace net_instaweb
