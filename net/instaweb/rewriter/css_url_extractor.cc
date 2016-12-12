// Copyright 2013 Google Inc.
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
//
// Author: mpalem@google.com (Maya Palem)

#include "net/instaweb/rewriter/public/css_url_extractor.h"

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_writer.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

CssUrlExtractor::~CssUrlExtractor() {
}

void CssUrlExtractor::ExtractUrl(const StringPiece& in_text,
                                 StringVector* urls) {
  // We dont care about the output, we just want the url string captured.
  NullWriter out;
  NullMessageHandler handler;
  out_urls_ = urls;
  CssTagScanner::TransformUrls(in_text, &out, this, &handler);
}

CssTagScanner::Transformer::TransformStatus CssUrlExtractor::Transform(
    GoogleString* str) {
  if (!str->empty()) {
    // Push the Url into the output vector
    GoogleString* url = StringVectorAdd(out_urls_);
    *url = *str;
  }
  return kNoChange;
}

}  // namespace net_instaweb
