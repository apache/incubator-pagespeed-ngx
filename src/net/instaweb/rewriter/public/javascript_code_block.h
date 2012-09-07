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

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class JavascriptLibraryIdentification;
class MessageHandler;
class Statistics;
class Variable;

// Class wrapping up configuration information for javascript
// rewriting, in order to minimize footprint of later changes
// to javascript rewriting.
class JavascriptRewriteConfig {
 public:
  JavascriptRewriteConfig(
      Statistics* statistics, bool minify,
      const JavascriptLibraryIdentification* identification);

  static void InitStats(Statistics* statistics);

  // Whether to minify javascript output (using jsminify).
  // true by default.
  bool minify() const { return minify_; }
  const JavascriptLibraryIdentification* library_identification() const {
    return library_identification_;
  }

  Variable* blocks_minified() { return blocks_minified_; }
  Variable* libraries_redirected() { return libraries_redirected_; }
  Variable* minification_failures() { return minification_failures_; }
  Variable* total_bytes_saved() { return total_bytes_saved_; }
  Variable* total_original_bytes() { return total_original_bytes_; }
  Variable* num_uses() { return num_uses_; }

  // Statistics names.
  static const char kBlocksMinified[];
  static const char kLibrariesRedirected[];
  static const char kMinificationFailures[];
  static const char kTotalBytesSaved[];
  static const char kTotalOriginalBytes[];
  static const char kMinifyUses[];

 private:
  bool minify_;
  // Library identifier.  NULL if library identification should be skipped.
  const JavascriptLibraryIdentification* library_identification_;

  // Statistics
  // # of JS blocks (JS files and <script> blocks) successfully minified.
  Variable* blocks_minified_;
  // # of JS blocks that were redirected to a known URL.
  Variable* libraries_redirected_;
  // # of JS blocks we failed to minify.
  Variable* minification_failures_;
  // Sum of all bytes saved from minifying JS.
  Variable* total_bytes_saved_;
  // Sum of original bytes of all successfully minified JS blocks.
  // total_bytes_saved_ / total_original_bytes_ should be the average
  // percentage reduction of JS block size.
  Variable* total_original_bytes_;
  // # of uses of the minified JS (updating <script> src= attributes or
  // contents).
  Variable* num_uses_;

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

  // Determines whether the javascript is brittle and will likely
  // break if we alter its URL.
  static bool UnsafeToRename(const StringPiece& script);

  // Rewrites the javascript code and returns whether that
  // successfully made it smaller.
  bool ProfitableToRewrite() {
    RewriteIfNecessary();
    return (output_code_.size() < original_code_.size());
  }

  // Returns the current (maximally-rewritten) contents of the
  // code block.
  const StringPiece Rewritten() {
    RewriteIfNecessary();
    return output_code_;
  }

  // Returns the rewritten contents as a mutable GoogleString* suitable for
  // swap() (but owned by the code block).  This should only be used if
  // ProfitableToRewrite() holds.
  GoogleString* RewrittenString() {
    RewriteIfNecessary();
    DCHECK(rewritten_code_.size() < original_code_.size());
    return &rewritten_code_;
  }

  // Is the current block a JS library that can be redirected to a canonical
  // URL?  If so, return that canonical URL (storage owned by the underlying
  // config object passed in at construction), otherwise return an empty
  // StringPiece.
  StringPiece ComputeJavascriptLibrary();

  // Converts a regular string to what can be used in Javascript directly. Note
  // that output also contains starting and ending quotes, to facilitate
  // embedding.
  static void ToJsStringLiteral(const StringPiece& original,
                                GoogleString* escaped) {
    EscapeToJsStringLiteral(original, true /*add quotes*/, escaped);
  }

  // Generates a hash of a URL escaped to be safe to use in a Javascript
  // identifier, so that variable names can be safely created that won't
  // collide with other local Javascript.
  static GoogleString JsUrlHash(const GoogleString &url, Hasher *hasher) {
    GoogleString url_hash = hasher->Hash(GoogleUrl(url).PathAndLeaf());
    // Hashes may contain '-', which isn't valid in a JavaScript name, so
    // replace every '-' with '$'.
    size_t pos = 0;
    while ((pos = url_hash.find_first_of('-', pos)) != GoogleString::npos) {
      url_hash[pos] = '$';
    }
    return url_hash;
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

 private:
  void RewriteIfNecessary() {
    if (!rewritten_) {
      Rewrite();
      rewritten_ = true;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(JavascriptCodeBlock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
