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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/javascript_code_block.h"

#include <cstddef>

#include "net/instaweb/js/public/js_minify.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Statistics names
const char JavascriptRewriteConfig::kBlocksMinified[] =
    "javascript_blocks_minified";
const char JavascriptRewriteConfig::kMinificationFailures[] =
    "javascript_minification_failures";
const char JavascriptRewriteConfig::kTotalBytesSaved[] =
    "javascript_total_bytes_saved";
const char JavascriptRewriteConfig::kTotalOriginalBytes[] =
    "javascript_total_original_bytes";
const char JavascriptRewriteConfig::kMinifyUses[] = "javascript_minify_uses";

JavascriptRewriteConfig::JavascriptRewriteConfig(Statistics* stats)
    : minify_(true),
      redirect_(true) {
  blocks_minified_ = stats->GetVariable(kBlocksMinified);
  minification_failures_ = stats->GetVariable(kMinificationFailures);
  total_bytes_saved_ = stats->GetVariable(kTotalBytesSaved);
  total_original_bytes_ = stats->GetVariable(kTotalOriginalBytes);
  num_uses_ = stats->GetVariable(kMinifyUses);
}

void JavascriptRewriteConfig::Initialize(Statistics* statistics) {
  statistics->AddVariable(kBlocksMinified);
  statistics->AddVariable(kMinificationFailures);
  statistics->AddVariable(kTotalBytesSaved);
  statistics->AddVariable(kTotalOriginalBytes);
  statistics->AddVariable(kMinifyUses);
}

JavascriptCodeBlock::JavascriptCodeBlock(
    const StringPiece& original_code, JavascriptRewriteConfig* config,
    const StringPiece& message_id, MessageHandler* handler)
    : config_(config),
      message_id_(message_id.data(), message_id.size()),
      handler_(handler),
      original_code_(original_code.data(), original_code.size()),
      output_code_(original_code_),
      rewritten_(false) { }

JavascriptCodeBlock::~JavascriptCodeBlock() { }

const JavascriptLibraryId JavascriptCodeBlock::ComputeJavascriptLibrary() {
  // We always RewriteIfNecessary just to provide a degree of
  // predictability to the rewrite flow.
  RewriteIfNecessary();
  if (!config_->redirect()) {
    return JavascriptLibraryId();
  }
  return JavascriptLibraryId::Find(rewritten_code_);
}

bool JavascriptCodeBlock::UnsafeToRename(const StringPiece& script) {
  // If you're pulling out script elements it's probably because
  // you're trying to do a kind of reflection that would break if we
  // minified the code and mutated its url.
  return script.find("document.getElementsByTagName('script')")
           != StringPiece::npos ||
         script.find("document.getElementsByTagName(\"script\")")
           != StringPiece::npos ||
         script.find("$('script')")  // jquery version
           != StringPiece::npos ||
         script.find("$(\"script\")")
           != StringPiece::npos;
}

void JavascriptCodeBlock::Rewrite() {
  // Before attempting library identification, we minify.  However, we only
  // point output_code_ at the minified code if we actually want to serve it to
  // the rest of the universe.
  if ((config_->minify())) {
    if (!pagespeed::js::MinifyJs(original_code_, &rewritten_code_)) {
      handler_->Message(kInfo, "%s: Javascript minification failed.  "
                        "Preserving old code.", message_id_.c_str());
      TrimWhitespace(original_code_, &rewritten_code_);
      // Update stats.
      config_->minification_failures()->Add(1);
    } else {
      // Update stats.
      config_->blocks_minified()->Add(1);
      config_->total_original_bytes()->Add(original_code_.size());
      size_t savings = original_code_.size() - rewritten_code_.size();
      config_->total_bytes_saved()->Add(savings);
    }
    output_code_ = rewritten_code_;
  }
}

}  // namespace net_instaweb
