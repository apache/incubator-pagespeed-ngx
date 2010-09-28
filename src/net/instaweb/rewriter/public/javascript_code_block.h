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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// Class wrapping up configuration information for javascript
// rewriting, in order to minimize footprint of later changes
// to javascript rewriting.
class JavascriptRewriteConfig {
 public:
  JavascriptRewriteConfig()
      : minify_(true),
        redirect_(true) { }
  // Whether to minify javascript output (using jsmin).
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
 private:
  bool minify_;
  bool redirect_;

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
                      const JavascriptRewriteConfig& config,
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
  const JavascriptLibraryId ComputeJavascriptLibrary() {
    // We always RewriteIfNecessary just to provide a degree of
    // predictability to the rewrite flow.
    RewriteIfNecessary();
    if (!config_.redirect()) {
      return JavascriptLibraryId();
    }
    return JavascriptLibraryId::Find(rewritten_code_);
  }

 private:
  void RewriteIfNecessary() {
    if (!rewritten_) {
      Rewrite();
      rewritten_ = true;
    }
  }

 protected:
  void Rewrite();

  const JavascriptRewriteConfig& config_;
  MessageHandler* handler_;
  const std::string original_code_;
  StringPiece output_code_;
  bool rewritten_;
  std::string rewritten_code_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptCodeBlock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
