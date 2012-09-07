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
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/server_context.h"
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

// The javascript tag to insert at the bottom of head.  Formatting args:
//     1. %s : CDATA hack opener or "".
//     2. %s : the custom beacon url, by default "./mod_pagespeed_beacon?ets=".
//     3. %s : kUnloadTag.
//     4. %s : time taken to fetch the headers of the html document.
//     5. %s : experiment id.
//     6. %s : URL of HTML.
//     7. %s : CDATA hack closer or "".
//
//  Then our timing info, e.g. "unload:123", will be appended.
const char kUnloadScriptFormat[] =
    "<script type='text/javascript'>%s"
    "(function(){function g(){"
    "if(window.mod_pagespeed_loaded) {return;}"
    "var ifr=0;"
    "if(window.parent != window){ifr=1}"
    "new Image().src='%s%s'+"
    "(Number(new Date())-window.mod_pagespeed_start)+'&ifr='+ifr+'"
    "%s%s&url='+encodeURIComponent('%s');};"
    "var f=window.addEventListener;if(f){f('beforeunload',g,false);}else{"
    "f=window.attachEvent;if(f){f('onbeforeunload',g);}}"
    "})();%s</script>";

// The javascript tag to insert at the bottom of document.  The same formatting
// args are used for kUnloadScriptFormat.
//
// Then our timing info, e.g. "load:123", will be appended.
const char kTailScriptFormat[] =
    "<script type='text/javascript'>%s"
    "(function(){function g(){var ifr=0;var rpi='';"
    "if(window.mod_pagespeed_prefetch_start){"
    "rpi+='&nrp='+window.mod_pagespeed_num_resources_prefetched;"
    "rpi+='&htmlAt=';"
    "rpi+=(window.mod_pagespeed_start-window.mod_pagespeed_prefetch_start);}"
    "if(window.parent != window){ifr=1}"
    "new Image().src='%s%s'+"
    "(Number(new Date())-window.mod_pagespeed_start)+'&ifr='+ifr+'"
    "'+rpi+'"
    "%s%s&url='+encodeURIComponent('%s');"
    "window.mod_pagespeed_loaded=true;};"
    "var f=window.addEventListener;if(f){f('load',g,false);}else{"
    "f=window.attachEvent;if(f){f('onload',g);}}"
    "})();%s</script>";

// In mod_pagespeed, the output_filter gets run prior to mod_headers,
// so we may not know the correct mimetype at the time we run.
//
// So we should use this hack from
//     http://stackoverflow.com/questions/2375217
//     should-i-use-or-for-closing-a-cdata-section-into-xhtml
//
//   <script type="text/javascript">//<![CDATA[
//     INSTRUMENTATION CODE
//   //]]></script>
//
// The %s format elements after <script> and before </script> are the
// hooks that allow us to insert the cdata hacks.
const char kCdataHackOpen[] = "//<![CDATA[\n";
const char kCdataHackClose[] = "\n//]]>";

}  // namespace

// Timing tag for total page load time.  Also embedded in kTailScriptFormat
// above via the second %s.
const char AddInstrumentationFilter::kLoadTag[] = "load:";
const char AddInstrumentationFilter::kUnloadTag[] = "unload:";

// Counters.
const char AddInstrumentationFilter::kInstrumentationScriptAddedCount[] =
    "instrumentation_filter_script_added_count";
AddInstrumentationFilter::AddInstrumentationFilter(RewriteDriver* driver)
    : driver_(driver),
      found_head_(false),
      use_cdata_hack_(
          !driver_->server_context()->response_headers_finalized()),
      added_tail_script_(false),
      added_unload_script_(false) {
  Statistics* stats = driver->server_context()->statistics();
  instrumentation_script_added_count_ = stats->GetVariable(
      kInstrumentationScriptAddedCount);
}

AddInstrumentationFilter::~AddInstrumentationFilter() {}

void AddInstrumentationFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kInstrumentationScriptAddedCount);
}

void AddInstrumentationFilter::StartDocument() {
  found_head_ = false;
  added_tail_script_ = false;
  added_unload_script_ = false;
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
  if (!added_tail_script_ && element->keyword() == HtmlName::kBody) {
    // We relied on the existence of a <head> element.  This should have been
    // assured by add_head_filter.
    CHECK(found_head_) << "Reached end of document without finding <head>."
        "  Please turn on the add_head filter.";
    AddScriptNode(element, kTailScriptFormat, kLoadTag);
    added_tail_script_ = true;
  } else if (found_head_ && element->keyword() == HtmlName::kHead &&
             driver_->options()->report_unload_time() &&
             !added_unload_script_) {
    AddScriptNode(element, kUnloadScriptFormat, kUnloadTag);
    added_unload_script_ = true;
  }
}

void AddInstrumentationFilter::AddScriptNode(HtmlElement* element,
                                             const GoogleString& script_format,
                                             const GoogleString& tag_name) {
  GoogleString html_url;
  driver_->google_url().Spec().CopyToString(&html_url);
  const RewriteOptions::BeaconUrl& beacons = driver_->options()->beacon_url();
  const GoogleString* beacon_url =
      driver_->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString expt_id_param;
  if (driver_->options()->running_furious()) {
    int furious_state = driver_->options()->furious_id();
    if (furious_state != furious::kFuriousNotSet &&
        furious_state != furious::kFuriousNoExperiment) {
      expt_id_param = StringPrintf("&exptid=%d",
                                   driver_->options()->furious_id());
    }
  }
  GoogleString headers_fetch_time;
  LogRecord* log_record = driver_->log_record();
  if (log_record != NULL && log_record->logging_info()->has_timing_info() &&
      log_record->logging_info()->timing_info().has_header_fetch_ms()) {
    int64 header_fetch_ms =
        log_record->logging_info()->timing_info().header_fetch_ms();
    // If time taken to fetch the http header is not set then the response
    // came from cache.
    headers_fetch_time = StrCat(
        "&hft=",
        Integer64ToString(header_fetch_ms));
  }
  GoogleString tail_script = StringPrintf(
      script_format.c_str(),
      use_cdata_hack_ ? kCdataHackOpen : "",
      beacon_url->c_str(), tag_name.c_str(), headers_fetch_time.c_str(),
      expt_id_param.c_str(),
      html_url.c_str(),
      use_cdata_hack_ ? kCdataHackClose: "");
  if (driver_->MimeTypeXhtmlStatus() == RewriteDriver::kIsXhtml) {
    // We shouldn't escape ampersands if we're using CDATA, but we will only use
    // CDATA if the response headers aren't finalized while we will only have
    // have MimeTypeXhtmlStatus as Xhtml if they are.
    CHECK(!use_cdata_hack_);
    GlobalReplaceSubstring("&", "&amp;", &tail_script);
  }
  HtmlCharactersNode* script =
      driver_->NewCharactersNode(element, tail_script);
  driver_->InsertElementBeforeCurrent(script);
}

}  // namespace net_instaweb
