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

#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "third_party/jsmin/cpp/jsmin.h"

namespace net_instaweb {

namespace {

// Statistics names
const char kJavascriptBlocksMinified[] = "javascript_blocks_minified";
const char kJavascriptBytesSaved[] = "javascript_bytes_saved";
const char kJavascriptMinificationFailures[] =
    "javascript_minification_failures";
const char kJavascriptTotalBlocks[] = "javascript_total_blocks";

// If there's an SGML comment surrounding the string, remove it and return
// true; otherwise, return false.  Also trim whitespace.
bool TrimWrappingSgmlComment(const StringPiece& input, std::string* output) {
  TrimWhitespace(input, output);
  StringPiece trimmed = *output;
  if (trimmed.starts_with("<!--") && trimmed.ends_with("-->")) {
    output->erase(output->size() - 3, 3); // Erase "-->" from end
    output->erase(0, 4); // Erase "<!--" start
    return true;
  } else {
    return false;
  }
}

}  // namespace

JavascriptRewriteConfig::JavascriptRewriteConfig(Statistics* stats)
    : minify_(true),
      redirect_(true),
      blocks_minified_(NULL),
      bytes_saved_(NULL),
      minification_failures_(NULL),
      total_blocks_(NULL) {
  if (stats != NULL) {
    blocks_minified_ = stats->GetVariable(kJavascriptBlocksMinified);
    bytes_saved_ = stats->GetVariable(kJavascriptBytesSaved);
    minification_failures_ =
        stats->GetVariable(kJavascriptMinificationFailures);
    total_blocks_ = stats->GetVariable(kJavascriptTotalBlocks);
  }
}

void JavascriptRewriteConfig::Initialize(Statistics* statistics) {
  statistics->AddVariable(kJavascriptBlocksMinified);
  statistics->AddVariable(kJavascriptBytesSaved);
  statistics->AddVariable(kJavascriptMinificationFailures);
  statistics->AddVariable(kJavascriptTotalBlocks);
}

JavascriptCodeBlock::JavascriptCodeBlock(
    const StringPiece& original_code, JavascriptRewriteConfig* config,
    MessageHandler* handler)
    : config_(config),
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

void JavascriptCodeBlock::Rewrite() {
  // Before attempting library identification, we minify.  However, we only
  // point output_code_ at the minified code if we actually want to serve it to
  // the rest of the universe.
  config_->AddBlock();
  if ((config_->minify() || config_->redirect())) {
    // If the script is wrapped in an SGML comment, remove it before running
    // JSMin, and then put it back later.
    // TODO(mdsteele): JSMin should really be dealing with this for us, but it
    // doesn't.  Someday soon, we should replace it with something better.
    std::string sans_sgml_comment;
    bool had_an_sgml_comment =
        TrimWrappingSgmlComment(original_code_, &sans_sgml_comment);

    std::string untrimmed;
    if (!jsmin::MinifyJs(sans_sgml_comment, &untrimmed)) {
      handler_->Message(kError, "Minification failed.  Preserving old code.");
      config_->AddMinificationFailure();
      TrimWhitespace(sans_sgml_comment, &rewritten_code_);
    } else {
      TrimWhitespace(untrimmed, &rewritten_code_);
    }

    // If we removed an SGML comment before, put it back now.
    if (had_an_sgml_comment) {
      rewritten_code_.insert(0, "<!--\n");
      rewritten_code_.append("\n//-->");
    }
  } else {
    TrimWhitespace(original_code_, &rewritten_code_);
  }

  if (config_->minify()) {
    output_code_ = rewritten_code_;
    if (rewritten_code_.size() < original_code_.size()) {
      size_t savings = original_code_.size() - rewritten_code_.size();
      config_->AddBytesSaved(savings);
    }
  }
}

}  // namespace net_instaweb
