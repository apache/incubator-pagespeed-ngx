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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlIEDirectiveNode;
class JavascriptRewriteConfig;
class RewriteContext;
class RewriteDriver;
class Statistics;

/*
 * Find Javascript elements (either inline scripts or imported js files) and
 * rewrite them.  This can involve any combination of minifaction,
 * concatenation, renaming, reordering, and incrementalization that accomplishes
 * our goals.
 *
 * For the moment we keep it simple and just minify any scripts that we find.
 *
 * Challenges:
 *  * Identifying everywhere js is invoked, in particular event handlers on
 *    elements that might be found in css or in variously-randomly-named
 *    html properties.
 *  * Analysis of eval() contexts.  Actually less hard than the last, assuming
 *    constant strings.  Otherwise hard.
 *  * Figuring out where to re-inject code after analysis.
 *
 * We will probably need to do an end run around the need for js analysis by
 * instrumenting and incrementally loading code, then probably using dynamic
 * feedback to change the runtime instrumentation in future pages as we serve
 * them.
 */
class JavascriptFilter : public RewriteFilter {
 public:
  explicit JavascriptFilter(RewriteDriver* rewrite_driver);
  virtual ~JavascriptFilter();
  static void Initialize(Statistics* statistics);

  virtual void StartDocumentImpl() { InitializeConfigIfNecessary(); }
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();
  virtual void IEDirective(HtmlIEDirectiveNode* directive);

  virtual const char* Name() const { return "Javascript"; }
  virtual const char* id() const { return RewriteOptions::kJavascriptMinId; }
  virtual RewriteContext* MakeRewriteContext();

 protected:
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

 private:
  class Context;

  inline void CompleteScriptInProgress();
  inline void RewriteInlineScript();
  inline void RewriteExternalScript();
  // Lazily initialize config_ if it wasn't already.
  void InitializeConfigIfNecessary() {
    if (config_.get() != NULL) {
      return;
    }
    InitializeConfig();
  }
  void InitializeConfig();

  HtmlCharactersNode* body_node_;
  HtmlElement* script_in_progress_;
  HtmlElement::Attribute* script_src_;
  // some_missing_scripts indicates that we stopped processing a script and
  // therefore can't assume we know all of the Javascript on a page.
  bool some_missing_scripts_;
  scoped_ptr<JavascriptRewriteConfig> config_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_
