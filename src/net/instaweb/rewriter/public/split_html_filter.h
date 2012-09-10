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

#include <map>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/suppress_prehead_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/json_writer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

struct XpathUnit {
  GoogleString tag_name;
  GoogleString attribute_value;
  int child_number;
};

class HtmlElement;
class RewriteDriver;
class RewriteOptions;
class Writer;

typedef std::map<GoogleString, const Panel*> PanelIdToSpecMap;
typedef std::vector<XpathUnit> XpathUnits;
// Map of xpath to XpathUnits.
typedef std::map<GoogleString, XpathUnits*> XpathMap;

// Splits the incoming html content into above the fold html and below the
// fold json based on critical line specification stored in property cache.

// This filter will stream above the fold html and send below the fold json at
// EndDocument. It directly writes to the http request.
class SplitHtmlFilter : public SuppressPreheadFilter {
 public:
  static const char kRenderCohort[];
  static const char kCriticalLineInfoPropertyName[];

  explicit SplitHtmlFilter(RewriteDriver* rewrite_driver);
  virtual ~SplitHtmlFilter();

  virtual void StartDocument();
  virtual void EndDocument();

  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  virtual const char* Name() const { return "SplitHtmlFilter"; }

 private:
  void ServeNonCriticalPanelContents(const Json::Value& json);

  // Sets panel-id attribute to the element. This is not used by client-side
  // binding now.
  void MarkElementWithPanelId(HtmlElement* element,
                              const GoogleString& panel_id);

  // Reads the panel-id attribute and returns the value
  GoogleString GetPanelIdForInstance(HtmlElement* element);

  // Returns the panel id of the panel whose xpath matched with element.
  GoogleString MatchPanelIdForElement(HtmlElement* element);

  // Returns true if element is sibling of the current start element on top of
  // stack.
  bool IsElementSiblingOfCurrentPanel(HtmlElement* element);

  // Returns true if element is the parent of current panel
  bool IsElementParentOfCurrentPanel(HtmlElement* element);

  // Pops the json from top of the stack and merges with parent panel which is
  // one below it.
  void EndPanelInstance();

  // Pushes new Json to the top of the stack corresponding to element.
  void StartPanelInstance(HtmlElement* element);

  // Inserts <!-- GooglePanel begin --> and <!-- GooglePanel end --> stubs.
  void InsertPanelStub(HtmlElement* element, const GoogleString& panel_id);

  // Returns true if element matches with the end_marker for panel corresponding
  // to panel_id
  bool IsEndMarkerForCurrentPanel(HtmlElement* element);

  // Reads critical line config from property cache.
  void ReadCriticalLineConfig();

  // Populates the xpath map for all panels.
  void PopulateXpathMap();

  // Populates the xpath map for a particular xpath string.
  void PopulateXpathMap(const GoogleString& xpath);

  // Appends dict to the dictionary array
  void AppendJsonData(Json::Value* dictionary, const Json::Value& dict);

  void WriteString(const StringPiece& str);

  void Cleanup();

  // Inserts blink js and defer js init code into the head element. If no head
  // tag in the page, it inserts one before body tag.
  void InsertBlinkJavascript(HtmlElement* element);

  bool ElementMatchesXpath(const HtmlElement* element,
                           const std::vector<XpathUnit>& xpath_units);

  bool ParseXpath(const GoogleString& xpath,
                  std::vector<XpathUnit>* xpath_units);

  void ComputePanels(const CriticalLineInfo& critical_line_info,
                     PanelIdToSpecMap* panel_id_to_spec);

  RewriteDriver* rewrite_driver_;
  const RewriteOptions* options_;
  PanelIdToSpecMap panel_id_to_spec_;
  XpathMap xpath_map_;
  std::vector<ElementJsonPair> element_json_stack_;
  scoped_ptr<JsonWriter> json_writer_;

  StringPiece url_;
  bool script_written_;
  std::vector<std::vector<XpathUnit> > xpath_units_;
  std::vector<int> num_children_stack_;
  CriticalLineInfo critical_line_info_;
  Json::FastWriter fast_writer_;
  Writer* original_writer_;
  bool flush_head_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_FILTER_H_
