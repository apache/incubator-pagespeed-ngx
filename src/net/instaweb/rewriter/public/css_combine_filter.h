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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/combine_filter_base.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class OutputResource;
class Resource;
class ResourceManager;
class Variable;

class CssCombineFilter : public CombineFilterBase {
 public:
  CssCombineFilter(RewriteDriver* rewrite_driver, const char* path_prefix);
  virtual ~CssCombineFilter();

  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual const char* Name() const { return "CssCombine"; }

 private:
  // Try to combine all the CSS files we have seen so far.
  // Insert the combined resource where the first original CSS link was.
  void TryCombineAccumulated();

  virtual bool WritePiece(Resource* input, OutputResource* combination,
                          Writer* writer, MessageHandler* handler);

  Atom s_type_;
  Atom s_link_;
  Atom s_href_;
  Atom s_rel_;
  Atom s_media_;
  Atom s_style_;

  class Partnership;

  scoped_ptr<Partnership> partnership_;
  HtmlParse* html_parse_;
  CssTagScanner css_tag_scanner_;
  Variable* css_file_count_reduction_;

  DISALLOW_COPY_AND_ASSIGN(CssCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_
