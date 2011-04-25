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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

class MessageHandler;
class Statistics;
class Variable;

// Class wrapping up configuration information for javascript
// rewriting, in order to minimize footprint of later changes
// to javascript rewriting.
class JavascriptRewriteConfig {
 public:
  explicit JavascriptRewriteConfig(Statistics* statistics);
  static void Initialize(Statistics* statistics);
  // Whether to minify javascript output (using jsminify).
  // true by default.
  bool minify() const {
    return minify_;
  }
  void set_minify(bool minify) {
    minify_ = minify;
  }
  // whether to redirect external javascript libraries to
  // Google-as-a-CDN
  bool redirect() const {
    return redirect_;
  }
  void set_redirect(bool redirect) {
    redirect_ = redirect;
  }
  void AddBytesSaved(size_t bytes) {
    if (bytes_saved_ != NULL) {
      bytes_saved_->Add(bytes);
      blocks_minified_->Add(1);
    }
  }
  void AddMinificationFailure() {
    if (minification_failures_ != NULL) {
      minification_failures_->Add(1);
    }
  }
  void AddBlock() {
    if (total_blocks_ != NULL) {
      total_blocks_->Add(1);
    }
  }
 private:
  bool minify_;
  bool redirect_;
  Variable* blocks_minified_;
  Variable* bytes_saved_;
  Variable* minification_failures_;
  Variable* total_blocks_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptRewriteConfig);
};

// Object representing a block of Javascript code that might be a
// candidate for rewriting.
// TODO(jmaessen): Does this architecture make sense when we have
// multiple scripts on a page and the ability to move code around
// a bunch?  How do we maintain JS context in that setting?
//
// For now, we're content just being able to pull data in and parse it at all.
class JavascriptCodeBlock {
 public:
  JavascriptCodeBlock(const StringPiece& original_code,
                      JavascriptRewriteConfig* config,
                      const StringPiece& message_id,
                      MessageHandler* handler);

  virtual ~JavascriptCodeBlock();

  // Is it profitable to replace js code with rewritten version?
  bool ProfitableToRewrite() {
    RewriteIfNecessary();
    return (output_code_.size() < original_code_.size());
  }
  // TODO(jmaessen): Other questions we might reasonably ask:
  //   Can this code be floated downwards?

  // Returns the current (maximally-rewritten) contents of the
  // code block.
  const StringPiece Rewritten() {
    RewriteIfNecessary();
    return output_code_;
  }

  // Is the current block a JS library that can be redirected to Google?
  // If so, return the info necessary to do so.  Otherwise returns a
  // block for which .recognized() is false.
  const JavascriptLibraryId ComputeJavascriptLibrary();

 private:
  void RewriteIfNecessary() {
    if (!rewritten_) {
      Rewrite();
      rewritten_ = true;
    }
  }

 protected:
  void Rewrite();

  JavascriptRewriteConfig* config_;
  const GoogleString message_id_;  // ID to stick at begining of message.
  MessageHandler* handler_;
  const GoogleString original_code_;
  // Note that output_code_ points to either original_code_ or
  // to rewritten_code_ depending upon the results of processing
  // (ie it's an indirection to locally-owned data).
  StringPiece output_code_;
  bool rewritten_;
  GoogleString rewritten_code_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptCodeBlock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
