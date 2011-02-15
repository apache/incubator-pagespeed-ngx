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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
// Relatively simple filter that trims redundant information from the left end
// of each url.  In particular, we can drop http: from any url that is on a page
// served via http (etc. for other protocols).  By the same token, a page often
// contains fully-qualified urls that can be made base-relative (especially as
// we may do this as a result of rewriting itself), or root-relative.  We should
// strip the leading portions of these urls.
//
// We actually register the base url of a page.  This in turn registers
// individual trimmings for the protocol, host, and path in that order.  These
// portions of the url are then trimmed off in order by the Trim(...) operation.
//
// TODO(jmaessen): url references in css / outside src= and href= properties
// TODO(jmaessen): do we need a generic filter base class that just finds urls
// and calls a class method?  Or do we need context information for any
// transform other than the sort of thing you see here?
// TODO(jmaessen): Do we care to introduce ../ in order to relativize more urls?
// Do we have a library solution to do so with minimal effort?

class Statistics;
class Variable;

class UrlLeftTrimFilter : public EmptyHtmlFilter {
 public:
  UrlLeftTrimFilter(HtmlParse* html_parse, Statistics* statistics);
  static void Initialize(Statistics* statistics);
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "UrlLeftTrim"; }

 protected:
  friend class UrlLeftTrimFilterTest;
  // url: original url to shorten
  bool Trim(const StringPiece& url, std::string* trimmed_url);

 private:
  void TrimAttribute(HtmlElement::Attribute* attr);
  void ClearBaseUrl();
  // There is only one base_url at a time, so calling the function clears out
  // the previous base_url.
  // SetBaseUrl() should be called at the beginning of the document with the
  // document's url, and whenever we encounter a base tag.
  void SetBaseUrl(const StringPiece& base_url);

  HtmlParse* html_parse_;
  GURL base_url_;              // url we make paths relative to

  // Keep local copies of the folowing so we don't have to re-parse the base_url
  // for every url we're trimming.
  // TODO: look into GURL methods that get these for us
  // without reparsing the url
  std::string scheme_;      // scheme of the base_url_, e.g. http
  std::string origin_;      // origin of the base_url_, e.g. http://www.cnn.com
  std::string path_;        // path of the base_url_, e.g. /b/c/d/

  // Stats on how much trimming we've done.
  Variable* trim_count_;
  Variable* trim_saved_bytes_;

  DISALLOW_COPY_AND_ASSIGN(UrlLeftTrimFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_
