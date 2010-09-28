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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/javascript_code_block.h"

#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/message_handler.h"
#include "third_party/jsmin/cpp/jsmin.h"

namespace net_instaweb {

JavascriptCodeBlock::JavascriptCodeBlock(
    const StringPiece& original_code, const JavascriptRewriteConfig& config,
    MessageHandler* handler)
    : config_(config),
      handler_(handler),
      original_code_(original_code.data(), original_code.size()),
      output_code_(original_code_),
      rewritten_(false) { }

JavascriptCodeBlock::~JavascriptCodeBlock() { }

void JavascriptCodeBlock::Rewrite() {
  std::string untrimmed;
  // Before attempting library identification, we minify.  However, we only
  // point output_code_ at the minified code if we actually want to serve it to
  // the rest of the universe.
  if ((config_.minify() || config_.redirect())) {
    if (!jsmin::MinifyJs(original_code_, &untrimmed)) {
      handler_->Message(kError, "Minification failed.  Preserving old code.");
      TrimWhitespace(original_code_, &rewritten_code_);
    } else {
      TrimWhitespace(untrimmed, &rewritten_code_);
    }
  } else {
    TrimWhitespace(original_code_, &rewritten_code_);
  }
  if (config_.minify()) {
    output_code_ = rewritten_code_;
  }
}

}  // namespace net_instaweb
