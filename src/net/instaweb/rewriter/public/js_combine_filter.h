/*
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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class MessageHandler;
class OutputResource;
class Resource;
class ResourceManager;
class Variable;
class Writer;

class JsCombineFilter : public RewriteFilter {
 public:
  static const char kJsFileCountReduction[];  // statistics variable name

  JsCombineFilter(RewriteDriver* rewrite_driver, const char* path_prefix);
  virtual ~JsCombineFilter();

  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Flush();
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual const char* Name() const { return "JsCombine"; }
  virtual bool Fetch(OutputResource* resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

 private:
  class JsCombiner;
  friend class JsCombineFilterTest;

  void ConsiderJsForCombination(HtmlElement* element,
                                HtmlElement::Attribute* src);

  // Return true if the current outermost <script> element exists and
  // is included  inside the set we're accumulating in combiner_.
  bool IsCurrentScriptInCombination() const;

  // Returns variable where code for given URL should be stored.
  std::string VarName(const std::string& url) const;

  ScriptTagScanner script_scanner_;
  scoped_ptr<JsCombiner> combiner_;
  int script_depth_;  // how many script elements we are inside
  // current outermost <script> not with JavaScript we are inside, or NULL
  HtmlElement* current_js_script_;

  DISALLOW_COPY_AND_ASSIGN(JsCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_
