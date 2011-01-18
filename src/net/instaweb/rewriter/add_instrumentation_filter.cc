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

#include "public/add_instrumentation_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// The javascript tag to insert in the top of the <head> element.  We want this
// as early as possible in the html.  It must be short and fast.
const char kHeadScript[] =
    "<script type='text/javascript'>"
    "window.mod_pagespeed_start = Number(new Date());"
    "</script>";

// The javascript tag to insert at the bottom of document.  The %s will be
// replaced with the custom beacon url, by default
// "./mod_pagespeed_beacon?ets=".  Then our timing info, e.g. "load:123", will
// be appended.
const char kTailScript[] =
    "<script type='text/javascript'>"
    "function g(){new Image().src='%sload:'+"
    "(Number(new Date())-window.mod_pagespeed_start);};"
    "var f=window.addEventListener;if(f){f('load',g,false);}else{"
    "f=window.attachEvent;if(f){f('onload',g);}}"
    "</script>";

// Timing tag for total page load time.  Also embedded in kTailScript above!
const char kLoadTag[] = "load:";

// Variables for the beacon to increment.  These are currently handled in
// mod_pagespeed_handler on apache.  The average load time in milliseconds is
// total_page_load_ms / page_load_count.  Note that these are not updated
// together atomically, so you might get a slightly bogus value.
const char kTotalPageLoadMs[] = "total_page_load_ms";
const char kPageLoadCount[] = "page_load_count";

}  // namespace

AddInstrumentationFilter::AddInstrumentationFilter(
    HtmlParse* html_parse, const StringPiece& beacon_url, Statistics* stats)
    : html_parse_(html_parse),
      found_head_(false),
      s_head_(html_parse->Intern("head")),
      s_body_(html_parse->Intern("body")),
      total_page_load_ms_((stats == NULL) ? NULL :
                          stats->GetVariable(kTotalPageLoadMs)),
      page_load_count_((stats == NULL) ? NULL :
                       stats->GetVariable(kPageLoadCount)) {
  beacon_url.CopyToString(&beacon_url_);
}

void AddInstrumentationFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kTotalPageLoadMs);
  statistics->AddVariable(kPageLoadCount);
}

void AddInstrumentationFilter::StartDocument() {
  found_head_ = false;
}

void AddInstrumentationFilter::StartElement(HtmlElement* element) {
  if (!found_head_) {
    if (element->tag() == s_head_) {
      found_head_ = true;
      // TODO(abliss): add an actual element instead, so other filters can
      // rewrite this JS
      HtmlCharactersNode* script =
          html_parse_->NewCharactersNode(element, kHeadScript);
      html_parse_->InsertElementAfterCurrent(script);
    }
  }
}

void AddInstrumentationFilter::EndElement(HtmlElement* element) {
  if (element->tag() == s_body_) {
    // We relied on the existence of a <head> element.  This should have been
    // assured by add_head_filter.
    CHECK(found_head_) << "Reached end of document without finding <head>."
        "  Please turn on the add_head filter.";
    std::string tailScript = StringPrintf(kTailScript, beacon_url_.c_str());
    HtmlCharactersNode* script =
        html_parse_->NewCharactersNode(element, tailScript);
    html_parse_->InsertElementBeforeCurrent(script);
  }
}

bool AddInstrumentationFilter::HandleBeacon(const StringPiece& unparsed_url) {
  if ((total_page_load_ms_ == NULL) || (page_load_count_ == NULL)) {
    return false;
  }
  std::string url = unparsed_url.as_string();
  // TODO(abliss): proper query parsing
  size_t index = url.find(kLoadTag);
  if (index == std::string::npos) {
    return false;
  }
  url = url.substr(index + strlen(kLoadTag));
  int value = 0;
  if (!StringToInt(url, &value)) {
    return false;
  }
  total_page_load_ms_->Add(value);
  page_load_count_->Add(1);
  return true;
}

}  // namespace net_instaweb
