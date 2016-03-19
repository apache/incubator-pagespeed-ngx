/*
 * Copyright 2016 Google Inc.
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

#ifndef PAGESPEED_KERNEL_HTML_AMP_DOCUMENT_FILTER_H_
#define PAGESPEED_KERNEL_HTML_AMP_DOCUMENT_FILTER_H_

#include <memory>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

class HtmlElement;
class HtmlParse;

// This filter is designed to run immediately while lexing HTML into the system
// as an event listener, rather than in the streaming filter chain.  It is used
// to monitor the HTML and try to figure out whether the document is an AMP
// document.  See https://www.ampproject.org/ .
//
// If the AMP-ness of a document is claimed in an incorrect manner (e.g. there
// was intervening tag or non-whitespace characters before the <html amp> tag,
// the filter adds a comment saying so.  This may help users debug why PageSpeed
// makes a page amp-invalid.
class AmpDocumentFilter : public EmptyHtmlFilter {
 public:
  static const char kUtf8LightningBolt[];
  static const char kInvalidAmpDirectiveComment[];

  typedef Callback1<bool> BoolCallback;

  // When the filter discovers whether a document is AMP-compatible, it will
  // call discovered->Run(is_amp).  The callback will be called exactly
  // once for every HTML document passing through the filter.  It must
  // be allocated with NewPermanentCallback.  Ownership is tranferred to
  // the filter.
  AmpDocumentFilter(HtmlParse* html_parse, BoolCallback* discovered);
  virtual ~AmpDocumentFilter();

  void StartDocument() override;
  void EndDocument() override;
  void StartElement(HtmlElement* element) override;
  void Characters(HtmlCharactersNode* chars) override;

  const char* Name() const override { return "AmpDocumentFilter"; }

 private:
  HtmlParse* html_parse_;
  bool is_known_;
  bool saw_doctype_;
  std::unique_ptr<BoolCallback> discovered_;

  DISALLOW_COPY_AND_ASSIGN(AmpDocumentFilter);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_AMP_DOCUMENT_FILTER_H_
