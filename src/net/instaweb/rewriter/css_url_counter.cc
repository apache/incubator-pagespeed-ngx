// Copyright 2012 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_url_counter.h"

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_writer.h"

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
