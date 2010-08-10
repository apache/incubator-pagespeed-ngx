// Copyright 2010 Google Inc.
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

#include "net/instaweb/apache/html_rewriter.h"

#include "net/instaweb/apache/html_rewriter_imp.h"

namespace html_rewriter {

HtmlRewriter::HtmlRewriter(PageSpeedServerContext* context,
                           ContentEncoding encoding,
                           const std::string& base_url,
                           const std::string& url, std::string* output)
    : html_rewriter_imp_(new HtmlRewriterImp(
        context, encoding, base_url, url, output)) {
}

HtmlRewriter::~HtmlRewriter() {
  delete html_rewriter_imp_;
}

void HtmlRewriter::Finish() {
  html_rewriter_imp_->Finish();
}

void HtmlRewriter::Flush() {
  html_rewriter_imp_->Flush();
}

void HtmlRewriter::Rewrite(const char* input, int size) {
  html_rewriter_imp_->Rewrite(input, size);
}

}  // namespace html_rewriter
