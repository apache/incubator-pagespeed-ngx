/**
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

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {
class HtmlParse;
class MessageHandler;
class ResponseHeaders;
class OutputResource;
class Resource;
class ResourceManager;
class Statistics;
class Writer;

/**
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
class JavascriptFilter : public RewriteSingleResourceFilter {
 public:
  JavascriptFilter(RewriteDriver* rewrite_driver,
                   const StringPiece& path_prefix);
  virtual ~JavascriptFilter();
  static void Initialize(Statistics* statistics);

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();
  virtual void IEDirective(HtmlIEDirectiveNode* directive);

  // Configuration settings for javascript filtering:
  // Set whether to minify javascript code blocks encountered.
  void set_minify(bool minify)  {
    config_.set_minify(minify);
  }

  virtual const char* Name() const { return "Javascript"; }

 protected:
  virtual bool RewriteLoadedResource(const Resource* input_resource,
                                     OutputResource* output_resource);

 private:
  inline void CompleteScriptInProgress();
  inline void RewriteInlineScript();
  inline void RewriteExternalScript();
  inline Resource* ScriptAtUrl(const StringPiece& script_url);
  const StringPiece FlattenBuffer(std::string* script_buffer);
  bool WriteExternalScriptTo(const Resource* script_resource,
                             const StringPiece& script_out,
                             OutputResource* script_dest);

  std::vector<HtmlCharactersNode*> buffer_;
  HtmlParse* html_parse_;
  HtmlElement* script_in_progress_;
  HtmlElement::Attribute* script_src_;
  ResourceManager* resource_manager_;
  // some_missing_scripts indicates that we stopped processing a script and
  // therefore can't assume we know all of the Javascript on a page.
  bool some_missing_scripts_;
  JavascriptRewriteConfig config_;
  const Atom s_script_;
  const Atom s_src_;
  const Atom s_type_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_
