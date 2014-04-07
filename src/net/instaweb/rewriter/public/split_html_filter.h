/*
 * Copyright 2012 Google Inc.
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

// Author: rahulbansal@google.com (Rahul Bansal)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/suppress_prehead_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/json_writer.h"
#include "net/instaweb/util/public/null_writer.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class RewriteOptions;
class SplitHtmlConfig;
class SplitHtmlState;
class StaticAssetManager;
class Writer;

// Splits the incoming html content into above the fold html and below the
// fold json based on critical line specification stored in property cache.

// This filter will stream above the fold html and send below the fold json at
// EndDocument. It directly writes to the http request.
class SplitHtmlFilter : public SuppressPreheadFilter {
 public:
  static const char kSplitSuffixJsFormatString[];
  static const char kSplitTwoChunkSuffixJsFormatString[];
  static const char kLoadHiResImages[];
  static const char kMetaReferer[];

  explicit SplitHtmlFilter(RewriteDriver* rewrite_driver);
  virtual ~SplitHtmlFilter();

  virtual void DetermineEnabled();

  virtual void StartDocument();
  virtual void EndDocument();

  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  static const GoogleString& GetBlinkJsUrl(
      const RewriteOptions* options,
      const StaticAssetManager* static_asset_manager);

  virtual const char* Name() const { return "SplitHtmlFilter"; }

 private:
  const SplitHtmlConfig* config() const {
    return driver()->split_html_config();
  }
  const StaticAssetManager* static_asset_manager() const {
    return driver()->server_context()->static_asset_manager();
  }

  void ServeNonCriticalPanelContents(const Json::Value& json);

  // Sets panel-id attribute to the element. This is not used by client-side
  // binding now.
  void MarkElementWithPanelId(HtmlElement* element,
                              const GoogleString& panel_id);

  // Reads the panel-id attribute and returns the value
  GoogleString GetPanelIdForInstance(HtmlElement* element);

  // Returns a string representation of the critical line config.
  GoogleString GenerateCriticalLineConfigString();

  // Pops the json from top of the stack and merges with parent panel which is
  // one below it.
  void EndPanelInstance();

  // Pushes new Json to the top of the stack corresponding to element.
  void StartPanelInstance(HtmlElement* element);

  // Inserts <!-- GooglePanel begin --> and <!-- GooglePanel end --> stubs.
  void InsertPanelStub(HtmlElement* element, const GoogleString& panel_id);

  // Appends dict to the dictionary array
  void AppendJsonData(Json::Value* dictionary, const Json::Value& dict);

  void WriteString(const StringPiece& str);

  // Inserts lazy load and other scripts needed for split initialization into
  // the head element. If no head tag in the page, it inserts one before
  // body tag.
  void InsertSplitInitScripts(HtmlElement* element);

  void InvokeBaseHtmlFilterStartDocument();

  void InvokeBaseHtmlFilterStartElement(HtmlElement* element);

  void InvokeBaseHtmlFilterEndElement(HtmlElement* element);

  void InvokeBaseHtmlFilterEndDocument();

  // Returns true, if the cross-origin is allowed by looking it up in
  // RewriteOptions::access_control_allow_origins()
  // Note: The cross-origin must match exactly inclusing the protocol.
  // The only wildcard supported is '*' which means allow all domains.
  bool IsAllowedCrossDomainRequest(StringPiece cross_origin);

  scoped_ptr<SplitHtmlState> state_;
  const RewriteOptions* options_;
  std::vector<ElementJsonPair> element_json_stack_;
  Json::FastWriter fast_writer_;
  scoped_ptr<JsonWriter> json_writer_;
  Writer* original_writer_;
  NullWriter null_writer_;
  StringPiece url_;
  bool script_written_;
  bool flush_head_enabled_;
  bool disable_filter_;
  bool inside_pagespeed_no_defer_script_;
  bool serve_response_in_two_chunks_;
  int last_script_index_before_panel_stub_;
  bool panel_seen_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_FILTER_H_
