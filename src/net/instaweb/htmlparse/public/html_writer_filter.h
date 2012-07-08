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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_WRITER_FILTER_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_WRITER_FILTER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_filter.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class HtmlName;
class Writer;

class HtmlWriterFilter : public HtmlFilter {
 public:
  explicit HtmlWriterFilter(HtmlParse* html_parse);

  void set_writer(Writer* writer) { writer_ = writer; }
  virtual ~HtmlWriterFilter();

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void Comment(HtmlCommentNode* comment);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Directive(HtmlDirectiveNode* directive);
  virtual void Flush();

  void set_max_column(int max_column) { max_column_ = max_column; }
  void set_case_fold(bool case_fold) { case_fold_ = case_fold; }

  virtual const char* Name() const { return "HtmlWriter"; }

 protected:
  // Clear various variables for rewriting a new html file.
  virtual void Clear();

 private:
  void EmitBytes(const StringPiece& str);

  // Emits an HTML name, possibly case-folded depending on the
  // caller-specified option.
  void EmitName(const HtmlName& name);

  HtmlElement::CloseStyle GetCloseStyle(HtmlElement* element);

  // Escapes arbitrary text as HTML, e.g. turning & into &amp;.  If quoteChar
  // is non-zero, e.g. '"', then it would escape " as well.
  void EncodeBytes(const GoogleString& val, int quoteChar);

  HtmlParse* html_parse_;
  Writer* writer_;

  // Helps writer exploit shortcuts like <img .../> rather than writing
  // <img ...></img>.  At the end of StartElement, we defer writing the ">"
  // until we see what's coming next.  If it's the matching end_tag, then
  // we can emit />.  If something else comes first, then we have to
  // first emit the delayed ">" before continuing.
  HtmlElement* lazy_close_element_;

  int column_;
  int max_column_;
  int write_errors_;
  bool case_fold_;
  GoogleString case_fold_buffer_;

  DISALLOW_COPY_AND_ASSIGN(HtmlWriterFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_WRITER_FILTER_H_
