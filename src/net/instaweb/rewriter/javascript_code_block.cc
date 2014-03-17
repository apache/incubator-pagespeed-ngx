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

#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/source_map.h"
#include "pagespeed/kernel/js/js_minify.h"

namespace pagespeed { namespace js { struct JsTokenizerPatterns; } }

namespace net_instaweb {

// Statistics names
const char JavascriptRewriteConfig::kBlocksMinified[] =
    "javascript_blocks_minified";
const char JavascriptRewriteConfig::kLibrariesIdentified[] =
    "javascript_libraries_identified";
const char JavascriptRewriteConfig::kMinificationFailures[] =
    "javascript_minification_failures";
const char JavascriptRewriteConfig::kTotalBytesSaved[] =
    "javascript_total_bytes_saved";
const char JavascriptRewriteConfig::kTotalOriginalBytes[] =
    "javascript_total_original_bytes";
const char JavascriptRewriteConfig::kMinifyUses[] = "javascript_minify_uses";
const char JavascriptRewriteConfig::kNumReducingMinifications[] =
    "javascript_reducing_minifications";


const char JavascriptRewriteConfig::kJSMinificationDisabled[] =
    "javascript_minification_disabled";
const char JavascriptRewriteConfig::kJSDidNotShrink[] =
    "javascript_did_not_shrink";
const char JavascriptRewriteConfig::kJSFailedToWrite[] =
    "javascript_failed_to_write";

JavascriptRewriteConfig::JavascriptRewriteConfig(
    Statistics* stats, bool minify, bool use_experimental_minifier,
    const JavascriptLibraryIdentification* identification,
    const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns)
    : minify_(minify),
      use_experimental_minifier_(use_experimental_minifier),
      library_identification_(identification),
      js_tokenizer_patterns_(js_tokenizer_patterns),
      blocks_minified_(stats->GetVariable(kBlocksMinified)),
      libraries_identified_(stats->GetVariable(kLibrariesIdentified)),
      minification_failures_(stats->GetVariable(kMinificationFailures)),
      total_bytes_saved_(stats->GetVariable(kTotalBytesSaved)),
      total_original_bytes_(stats->GetVariable(kTotalOriginalBytes)),
      num_uses_(stats->GetVariable(kMinifyUses)),
      num_reducing_minifications_(
          stats->GetVariable(kNumReducingMinifications)),
      minification_disabled_(stats->GetVariable(kJSMinificationDisabled)),
      did_not_shrink_(stats->GetVariable(kJSDidNotShrink)),
      failed_to_write_(stats->GetVariable(kJSFailedToWrite)) {
}

void JavascriptRewriteConfig::InitStats(Statistics* statistics) {
  statistics->AddVariable(kBlocksMinified);
  statistics->AddVariable(kLibrariesIdentified);
  statistics->AddVariable(kMinificationFailures);
  statistics->AddVariable(kTotalBytesSaved);
  statistics->AddVariable(kTotalOriginalBytes);
  statistics->AddVariable(kMinifyUses);
  statistics->AddVariable(kNumReducingMinifications);

  statistics->AddVariable(kJSMinificationDisabled);
  statistics->AddVariable(kJSDidNotShrink);
  statistics->AddVariable(kJSFailedToWrite);
}

JavascriptCodeBlock::JavascriptCodeBlock(
    const StringPiece& original_code, JavascriptRewriteConfig* config,
    const StringPiece& message_id, MessageHandler* handler)
    : config_(config),
      message_id_(message_id.data(), message_id.size()),
      original_code_(original_code.data(), original_code.size()),
      rewritten_(false),
      successfully_rewritten_(false),
      handler_(handler) {
}

JavascriptCodeBlock::~JavascriptCodeBlock() { }

// Is this URL sanitary to be appended (in a line comment) to the JS doc?
bool JavascriptCodeBlock::IsSanitarySourceMapUrl(StringPiece url) {
  for (int i = 0, n = url.size(); i < n; ++i) {
    if (!IsNonControlAscii(url[i])) {
      // This is a bit broader than necessary. JS line comments can only be
      // terminated by Unicode line/paragraph separators (Zl/Zp). Instead of
      // searching for all of these, we simply check any non-standard chars.
      // Specifically, we reject any URL with control chars (0x00-0x1F,0x7F)
      // or any non-ASCII UTF-8 chars (Bytes 0x80-0xFF).
      // Because URLs passed in here are .pagespeed. rewritten URLs, we do
      // not expect any of them to be of this form anyway, so this case
      // shouldn't be hit.
      return false;
    }
  }
  return true;
}

void JavascriptCodeBlock::AppendSourceMapUrl(StringPiece url) {
  DCHECK(rewritten_);
  DCHECK(successfully_rewritten_);
  if (!IsSanitarySourceMapUrl(url)) {
    LOG(DFATAL) << "Unsanitary source map URL could not be added to JS " << url;
    return;
  }

  StrAppend(&rewritten_code_, "\n//# sourceMappingURL=", url, "\n");
}

StringPiece JavascriptCodeBlock::ComputeJavascriptLibrary() const {
  // TODO(jmaessen): when we compute minified version and find
  // a match, consider adding the un-minified hash to the library
  // identifier, and then using that to speed up identification
  // in future (at the cost of a double lookup for a miss).  Also
  // consider pruning candidate JS that is simply too small to match
  // a registered library.
  DCHECK(rewritten_);
  StringPiece result;
  if (rewritten_) {
    const JavascriptLibraryIdentification* library_identification =
        config_->library_identification();
    if (library_identification != NULL) {
      result = library_identification->Find(rewritten_code_);
      if (!result.empty()) {
        config_->libraries_identified()->Add(1);
      }
    }
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

bool JavascriptCodeBlock::Rewrite() {
  DCHECK(!rewritten_);
  if (rewritten_) {
    return successfully_rewritten_;
  }

  rewritten_ = true;
  successfully_rewritten_ = false;
  // We minify for two reasons: because the user wants minified js code (in
  // which case output_code_ should point to the minified code when we're
  // done), or because we're trying to identify a javascript library.
  // Bail if we're not doing one of these things.
  if (!config_->minify() && (config_->library_identification() == NULL)) {
    return successfully_rewritten_;
  }

  if (MinifyJs(original_code_, &rewritten_code_, &source_mappings_)) {
    // Minification succeeded. The fact that it succeeded doesn't imply that
    // it actually saved anything; we increment num_reducing_uses when there
    // were actual savings.
    config_->blocks_minified()->Add(1);
    if (config_->minify() && rewritten_code_.size() < original_code_.size()) {
      // Minification will actually be used.
      successfully_rewritten_ = true;
      config_->num_reducing_uses()->Add(1);
      config_->total_original_bytes()->Add(original_code_.size());
      // Note: This unsigned arithmetic is guaranteed not to underflow because
      // of the if statement above.
      config_->total_bytes_saved()->Add(
          original_code_.size() - rewritten_code_.size());
    }
  } else {  // Minification failed.
    handler_->Message(kInfo, "%s: Javascript minification failed.  "
                      "Preserving old code.", message_id_.c_str());
    // Note: Although we set rewritten_code_, we do not consider this a
    // successful rewrite and thus will not minify. This is only used for
    // canonical library identification.
    TrimWhitespace(original_code_, &rewritten_code_);
    // Update stats.
    config_->minification_failures()->Add(1);
  }
  return successfully_rewritten_;
}

void JavascriptCodeBlock::SwapRewrittenString(GoogleString* other) {
  DCHECK(rewritten_);
  DCHECK(successfully_rewritten_);

  other->swap(rewritten_code_);

  rewritten_code_.clear();
  rewritten_ = false;
  successfully_rewritten_ = false;
}

bool JavascriptCodeBlock::MinifyJs(
    StringPiece input, GoogleString* output,
    std::vector<source_map::Mapping>* source_mappings) {
  if (config_->use_experimental_minifier()) {
    return pagespeed::js::MinifyUtf8JsWithSourceMap(
        config_->js_tokenizer_patterns(), input, output, source_mappings);
  } else {
    return pagespeed::js::MinifyJs(input, output);
  }
}

}  // namespace net_instaweb
