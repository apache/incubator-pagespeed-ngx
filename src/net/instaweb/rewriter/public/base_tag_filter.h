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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

// Add this filter into the HtmlParse chain to add a base
// tag into the head section of an HTML document.
//
// e.g.
//
// HtmlParser* parser = ...
// ...
// BaseTagFilter* base_tag_filter = new BaseTagFilter(parser);
// parser->AddFilter(base_tag_filter);
// base_tag_filter->set_base("http://my_new_base.com");
// ...
// parser->StartParse()...
class BaseTagFilter : public EmptyHtmlFilter {
 public:
  explicit BaseTagFilter(HtmlParse* parser);

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "BaseTag"; }

  void set_base_url(const StringPiece& url) { url.CopyToString(&base_url_); }

 private:
  Atom s_head_;
  Atom s_base_;
  Atom s_href_;
  bool found_head_;
  std::string base_url_;
  HtmlParse* html_parse_;

  DISALLOW_COPY_AND_ASSIGN(BaseTagFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BASE_TAG_FILTER_H_
