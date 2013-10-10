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
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

namespace {

// The javascript tag to insert in the top of the <head> element.  We want this
// as early as possible in the html.  It must be short and fast.
const char kHeadScript[] =
    "<script type='text/javascript'>"
    "window.mod_pagespeed_start = Number(new Date());"
    "</script>";

}  // namespace

// Timing tag for total page load time.  Also embedded in kTailScriptFormat
// above via the second %s.
// TODO(jud): These values would be better set to "load" and "beforeunload".
const char AddInstrumentationFilter::kLoadTag[] = "load:";
const char AddInstrumentationFilter::kUnloadTag[] = "unload:";

// Counters.
const char AddInstrumentationFilter::kInstrumentationScriptAddedCount[] =
    "instrumentation_filter_script_added_count";
AddInstrumentationFilter::AddInstrumentationFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      found_head_(false),
      added_head_script_(false),
      added_unload_script_(false) {
  Statistics* stats = driver->server_context()->statistics();
  instrumentation_script_added_count_ = stats->GetVariable(
      kInstrumentationScriptAddedCount);
}

AddInstrumentationFilter::~AddInstrumentationFilter() {}

void AddInstrumentationFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kInstrumentationScriptAddedCount);
}

void AddInstrumentationFilter::StartDocumentImpl() {
  found_head_ = false;
  added_head_script_ = false;
  added_unload_script_ = false;
}

void AddInstrumentationFilter::AddHeadScript(HtmlElement* element) {
  // IE doesn't like tags other than title or meta at the start of the
  // head. The MSDN page says:
  //   The X-UA-Compatible header isn't case sensitive; however, it must appear
  //   in the header of the webpage (the HEAD section) before all other elements
  //   except for the title element and other meta elements.
  // Reference: http://msdn.microsoft.com/en-us/library/jj676915(v=vs.85).aspx
  if (element->keyword() != HtmlName::kTitle &&
      element->keyword() != HtmlName::kMeta) {
    added_head_script_ = true;
    // TODO(abliss): add an actual element instead, so other filters can
    // rewrite this JS
    HtmlCharactersNode* script = driver_->NewCharactersNode(NULL, kHeadScript);
    driver_->InsertNodeBeforeCurrent(script);
    instrumentation_script_added_count_->Add(1);
  }
}

void AddInstrumentationFilter::StartElementImpl(HtmlElement* element) {
  if (found_head_ && !added_head_script_) {
    AddHeadScript(element);
  }
  if (!found_head_ && element->keyword() == HtmlName::kHead) {
    found_head_ = true;
  }
}

void AddInstrumentationFilter::EndElementImpl(HtmlElement* element) {
  if (found_head_ && element->keyword() == HtmlName::kHead) {
    if (!added_head_script_) {
      AddHeadScript(element);
    }
    if (driver_->options()->report_unload_time() &&
        !added_unload_script_) {
      GoogleString js = GetScriptJs(kUnloadTag);
      HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
      if (!driver_->defer_instrumentation_script()) {
        driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
      }
      driver_->InsertNodeBeforeCurrent(script);
      driver_->server_context()->static_asset_manager()->AddJsToElement(
          js, script, driver_);
      added_unload_script_ = true;
    }
  }
}

void AddInstrumentationFilter::EndDocument() {
  // We relied on the existence of a <head> element.  This should have been
  // assured by add_head_filter.
  if (!found_head_) {
    LOG(WARNING) << "Reached end of document without finding <head>."
                    "  Please turn on the add_head filter.";
    return;
  }
  GoogleString js = GetScriptJs(kLoadTag);
  HtmlElement* script = driver_->NewElement(NULL, HtmlName::kScript);
  if (!driver_->defer_instrumentation_script()) {
    driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  }
  InsertNodeAtBodyEnd(script);
  driver_->server_context()->static_asset_manager()->AddJsToElement(js, script,
                                                                    driver_);
}

GoogleString AddInstrumentationFilter::GetScriptJs(StringPiece event) {
  GoogleString js;
  StaticAssetManager* static_asset_manager =
      driver_->server_context()->static_asset_manager();
  // Only add the static JS once.
  if (!added_unload_script_) {
    if (driver_->options()->enable_extended_instrumentation()) {
      js = static_asset_manager->GetAsset(
          StaticAssetManager::kExtendedInstrumentationJs, driver_->options());
    }
    StrAppend(&js, static_asset_manager->GetAsset(
        StaticAssetManager::kAddInstrumentationJs, driver_->options()));
  }

  GoogleString js_event = (event == kLoadTag) ? "load" : "beforeunload";

  const RewriteOptions::BeaconUrl& beacons = driver_->options()->beacon_url();
  const GoogleString* beacon_url =
      driver_->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString extra_params;
  if (driver_->options()->running_experiment()) {
    int experiment_state = driver_->options()->experiment_id();
    if (experiment_state != experiment::kExperimentNotSet &&
        experiment_state != experiment::kNoExperiment) {
      StrAppend(&extra_params, "&exptid=",
                IntegerToString(driver_->options()->experiment_id()));
    }
  }

  const RequestContext::TimingInfo& timing_info =
      driver_->request_context()->timing_info();
  int64 header_fetch_ms;
  if (timing_info.GetFetchHeaderLatencyMs(&header_fetch_ms)) {
    // If time taken to fetch the http header is not set, then the response
    // came from cache.
    StrAppend(&extra_params, "&hft=", Integer64ToString(header_fetch_ms));
  }
  int64 fetch_ms;
  if (timing_info.GetFetchLatencyMs(&fetch_ms)) {
    // If time taken to fetch the resource is not set, then the response
    // came from cache.
    StrAppend(&extra_params, "&ft=", Integer64ToString(fetch_ms));
  }
  int64 ttfb_ms;
  if (timing_info.GetTimeToFirstByte(&ttfb_ms)) {
    StrAppend(&extra_params, "&s_ttfb=", Integer64ToString(ttfb_ms));
  }

  // Append the http response code.
  if (driver_->response_headers() != NULL &&
      driver_->response_headers()->status_code() > 0 &&
      driver_->response_headers()->status_code() != HttpStatus::kOK) {
    StrAppend(&extra_params, "&rc=", IntegerToString(
        driver_->response_headers()->status_code()));
  }
  // Append the request id.
  if (driver_->request_context()->request_id() > 0) {
    StrAppend(&extra_params, "&id=", Integer64ToString(
        driver_->request_context()->request_id()));
  }

  GoogleString html_url;
  EscapeToJsStringLiteral(driver_->google_url().Spec(),
                          false, /* no quotes */
                          &html_url);

  StrAppend(&js, "\npagespeed.addInstrumentationInit(");
  StrAppend(&js, "'", *beacon_url, "', ");
  StrAppend(&js, "'", js_event, "', ");
  StrAppend(&js, "'", extra_params, "', ");
  StrAppend(&js, "'", html_url, "');");

  return js;
}

}  // namespace net_instaweb
