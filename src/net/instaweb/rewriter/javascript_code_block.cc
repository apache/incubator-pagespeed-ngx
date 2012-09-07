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
const char JavascriptRewriteConfig::kLibrariesRedirected[] =
    "javascript_libraries_redirected";
const char JavascriptRewriteConfig::kMinificationFailures[] =
    "javascript_minification_failures";
const char JavascriptRewriteConfig::kTotalBytesSaved[] =
    "javascript_total_bytes_saved";
const char JavascriptRewriteConfig::kTotalOriginalBytes[] =
    "javascript_total_original_bytes";
const char JavascriptRewriteConfig::kMinifyUses[] = "javascript_minify_uses";

JavascriptRewriteConfig::JavascriptRewriteConfig(
    Statistics* stats, bool minify,
    const JavascriptLibraryIdentification* identification)
    : minify_(minify),
      library_identification_(identification) {
  blocks_minified_ = stats->GetVariable(kBlocksMinified);
  libraries_redirected_ = stats->GetVariable(kLibrariesRedirected);
  minification_failures_ = stats->GetVariable(kMinificationFailures);
  total_bytes_saved_ = stats->GetVariable(kTotalBytesSaved);
  total_original_bytes_ = stats->GetVariable(kTotalOriginalBytes);
  num_uses_ = stats->GetVariable(kMinifyUses);
}

void JavascriptRewriteConfig::InitStats(Statistics* statistics) {
  statistics->AddVariable(kBlocksMinified);
  statistics->AddVariable(kLibrariesRedirected);
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

StringPiece JavascriptCodeBlock::ComputeJavascriptLibrary() {
  // We always RewriteIfNecessary just to provide a degree of
  // predictability to the rewrite flow.
  // TODO(jmaessen): when we compute minified version and find
  // a match, consider adding the un-minified hash to the library
  // identifier, and then using that to speed up identification
  // in future (at the cost of a double lookup for a miss).  Also
  // consider pruning candidate JS that is simply too small to match
  // a registered library.
  RewriteIfNecessary();
  const JavascriptLibraryIdentification* library_identification =
      config_->library_identification();
  if (library_identification == NULL) {
    return StringPiece(NULL);
  }
  StringPiece result = library_identification->Find(rewritten_code_);
  if (!result.empty()) {
    config_->libraries_redirected()->Add(1);
  }
  return result;
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
  // We minify for two reasons: because the user wants minified js code (in
  // which case output_code_ should point to the minified code when we're done),
  // or because we're trying to identify a javascript library.  Bail if we're
  // not doing one of these things.
  if (!config_->minify() && (config_->library_identification() == NULL)) {
    return;
  }
  if (!pagespeed::js::MinifyJs(original_code_, &rewritten_code_)) {
    handler_->Message(kInfo, "%s: Javascript minification failed.  "
                      "Preserving old code.", message_id_.c_str());
    TrimWhitespace(original_code_, &rewritten_code_);
    // Update stats.
    config_->minification_failures()->Add(1);
    return;
  }
  // Minification succeeded.  Update stats based on whether
  // minified code will be served back to the user or is just being
  // used for library identification.
  config_->blocks_minified()->Add(1);
  if (config_->minify()) {
    config_->total_original_bytes()->Add(original_code_.size());
    size_t savings = original_code_.size() - rewritten_code_.size();
    config_->total_bytes_saved()->Add(savings);
    output_code_ = rewritten_code_;
  }
}

}  // namespace net_instaweb
