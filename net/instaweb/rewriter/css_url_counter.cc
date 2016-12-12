// Copyright 2012 Google Inc.
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
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_url_counter.h"

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_writer.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

CssUrlCounter::~CssUrlCounter() {
}

bool CssUrlCounter::Count(const StringPiece& in_text) {
  // Output is meaningless, we are simply counting occurrences of URLs.
  NullWriter out;
  return CssTagScanner::TransformUrls(in_text, &out, this, handler_);
}

CssTagScanner::Transformer::TransformStatus CssUrlCounter::Transform(
    GoogleString* str) {
  TransformStatus ret = kNoChange;

  // Note: we do not mess with empty URLs at all.
  if (!str->empty()) {
    GoogleUrl url(*base_url_, *str);
    if (!url.IsWebOrDataValid()) {
      handler_->Message(kInfo, "Invalid URL in CSS %s expands to %s",
                        str->c_str(), url.spec_c_str());
      ret = kFailure;
    } else {
      // Count occurrences of each URL.
      GoogleString url_string;
      url.Spec().CopyToString(&url_string);
      ++url_counts_[url_string];
    }
  }

  return ret;
}

}  // namespace net_instaweb
