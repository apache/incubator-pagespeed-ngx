/*
 * Copyright 2013 Google Inc.
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

// Author: mpalem@google.com (Maya Palem)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_EXTRACTOR_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_EXTRACTOR_H_

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Transformer that collects the URLs in the input and adds them to a
// Stringvector.
class CssUrlExtractor : public CssTagScanner::Transformer {
 public:
  CssUrlExtractor() {}
  virtual ~CssUrlExtractor();

  void ExtractUrl(const StringPiece& in_text, StringVector* urls);

  virtual TransformStatus Transform(GoogleString* str);

 private:
  StringVector* out_urls_;

  DISALLOW_COPY_AND_ASSIGN(CssUrlExtractor);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_EXTRACTOR_H_
