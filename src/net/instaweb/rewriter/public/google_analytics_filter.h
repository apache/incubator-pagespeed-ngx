/**
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

// Author: slamm@google.com (Stephen Lamm)

// Search for synchronous loads of Google Analytics similar to the following:
//
//     <script type="text/javascript">
//         var gaJsHost = (("https:" == document.location.protocol) ?
//             "https://ssl." : "http://www.");
//         document.write(unescape("%3Cscript src='" + gaJsHost +
//             "google-analytics.com/ga.js type='text/javascript'" +
//             "%3E%3C/script%3E"));
//     </script>
//     <script type="text/javascript">
//         try {
//             var pageTracker = _gat._getTracker("UA-XXXXX-X");
//             pageTracker._trackPageview();
//         } catch(err) {}
//     </script>
//
// Replace the document.write with a new snippet that loads ga.js
// asynchronously. Also, insert a replacement for _getTracker that
// converts any calls to the synchronous API to the asynchronous API.
// The _getTracker replacement is a new function that returns a mock
// tracker object. Anytime a synchronous API method is called, the
// mock tracker fowards it to a _gaq.push(...) call.
//
// An alternative approach would been to find all the API calls and
// rewrite them to the asynchronous API. However, to be done properly,
// it would have had the added complication of using a JavaScript
// compiler.
//

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_ANALYTICS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_ANALYTICS_FILTER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class MessageHandler;
class MetaData;
class OutputResource;
class ResourceManager;
class Statistics;
class Variable;


// Edit a substring in a script element.
class ScriptEditor {
 public:
  enum Type {
    kGaJsScriptSrcLoad = 0,
    kGaJsDocWriteLoad,
    kGaJsInit,
  };
  ScriptEditor(HtmlElement* script_element_,
               HtmlCharactersNode* characters_node,
               std::string::size_type pos,
               std::string::size_type len,
               Type editor_type);

  HtmlElement* GetScriptElement() const { return script_element_; }
  HtmlCharactersNode* GetScriptCharactersNode() const {
    return script_characters_node_;
  }
  Type GetType() const { return editor_type_; }

  void NewContents(const StringPiece &replacement,
                   std::string* contents) const;

 private:
  HtmlElement* script_element_;
  HtmlCharactersNode* script_characters_node_;

  std::string::size_type pos_;
  std::string::size_type len_;

  Type editor_type_;
  DISALLOW_COPY_AND_ASSIGN(ScriptEditor);
};


// Filter <script> tags.
// Rewrite qualifying sync loads of Google Analytics as async loads.
class GoogleAnalyticsFilter : public EmptyHtmlFilter {
 public:
  typedef std::vector<StringPiece> MethodVector;

  explicit GoogleAnalyticsFilter(HtmlParse* html_parse,
                                 Statistics* statistics);

  // The filter will take ownership of the method vectors.
  explicit GoogleAnalyticsFilter(HtmlParse* html_parse,
                                 Statistics* statistics,
                                 MethodVector* glue_methods,
                                 MethodVector* unhandled_methods);

  static void Initialize(Statistics* statistics);

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  virtual void Flush();

  // Expected HTML Events in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters_node);

  // Unexpected HTML Events in <script> elements.
  virtual void Comment(HtmlCommentNode* comment);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);

  virtual const char* Name() const { return "GoogleAnalytics"; }

  static const char kPageLoadCount[];
  static const char kRewrittenCount[];

 private:
  void ResetFilter();

  bool MatchSyncLoad(StringPiece contents,
                     std::string::size_type &pos,
                     std::string::size_type &len) const;
  bool MatchSyncInit(StringPiece contents,
                     std::string::size_type start_pos,
                     std::string::size_type &pos,
                     std::string::size_type &len) const;
  bool MatchUnhandledCalls(StringPiece contents,
                           std::string::size_type start_pos) const;
  void FindRewritableScripts();
  void GetSyncToAsyncScript(std::string *buffer) const;
  bool RewriteAsAsync();

  bool is_load_found_;
  bool is_init_found_;
  std::vector<ScriptEditor*> script_editors_;

  scoped_ptr<MethodVector> glue_methods_;  // methods to forward to async api
  scoped_ptr<MethodVector> unhandled_methods_;  // if found, skip rewrite

  HtmlParse* html_parse_;
  HtmlElement* script_element_;  // NULL if not in script element
  HtmlCharactersNode* script_characters_node_;  // NULL if not found in script

  // HTML strings interned into a symbol table.
  Atom s_script_;
  Atom s_src_;
  Atom s_type_;

  Variable* page_load_count_;
  Variable* rewritten_count_;

  DISALLOW_COPY_AND_ASSIGN(GoogleAnalyticsFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_ANALYTICS_FILTER_H_
