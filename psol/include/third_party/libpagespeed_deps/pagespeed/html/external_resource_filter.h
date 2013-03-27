// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_HTML_EXTERNAL_RESOURCE_FILTER_H_
#define PAGESPEED_HTML_EXTERNAL_RESOURCE_FILTER_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace pagespeed {

class DomDocument;

namespace html {

// Filter that finds external resource URLs (e.g. CSS, JS) declared in
// HTML.
class ExternalResourceFilter : public net_instaweb::EmptyHtmlFilter {
 public:
  explicit ExternalResourceFilter(net_instaweb::HtmlParse* html_parse);
  virtual ~ExternalResourceFilter();

  virtual void StartDocument();
  virtual void StartElement(net_instaweb::HtmlElement* element);
  virtual const char* Name() const { return "ExternalResource"; }

  // Get the URLs of resources referenced by the parsed HTML.
  bool GetExternalResourceUrls(std::vector<std::string>* out,
                               const DomDocument* document,
                               const std::string& document_url) const;

 private:
  std::vector<std::string> external_resource_urls_;

  DISALLOW_COPY_AND_ASSIGN(ExternalResourceFilter);
};

}  // namespace html

}  // namespace pagespeed

#endif  // PAGESPEED_HTML_EXTERNAL_RESOURCE_FILTER_H_
