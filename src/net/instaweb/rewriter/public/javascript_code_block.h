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
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/source_map.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace pagespeed { namespace js { struct JsTokenizerPatterns; } }

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
  // Statistics names.
  static const char kBlocksMinified[];
  static const char kLibrariesIdentified[];
  static const char kMinificationFailures[];
  static const char kTotalBytesSaved[];
  static const char kTotalOriginalBytes[];
  static const char kMinifyUses[];
  static const char kNumReducingMinifications[];

  // Those are JS rewrite failure type statistics.
  static const char kJSMinificationDisabled[];
  static const char kJSDidNotShrink[];
  static const char kJSFailedToWrite[];

  JavascriptRewriteConfig(
      Statistics* statistics, bool minify, bool use_experimental_minifier,
      const JavascriptLibraryIdentification* identification,
      const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns);

  static void InitStats(Statistics* statistics);

  // Whether to minify javascript output.
  bool minify() const { return minify_; }
  // Whether to use the new JsTokenizer-based minifier.
  // TODO(sligocki): Once that minifier has been around for a while, we
  // should deprecate this option.
  bool use_experimental_minifier() const { return use_experimental_minifier_; }
  const JavascriptLibraryIdentification* library_identification() const {
    return library_identification_;
  }
  const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns() const {
    return js_tokenizer_patterns_;
  }

  Variable* blocks_minified() { return blocks_minified_; }
  Variable* libraries_identified() { return libraries_identified_; }
  Variable* minification_failures() { return minification_failures_; }
  Variable* total_bytes_saved() { return total_bytes_saved_; }
  Variable* total_original_bytes() { return total_original_bytes_; }
  Variable* num_uses() { return num_uses_; }
  Variable* num_reducing_uses() { return num_reducing_minifications_; }

  Variable* minification_disabled() { return minification_disabled_; }
  Variable* did_not_shrink() { return did_not_shrink_; }
  Variable* failed_to_write() { return failed_to_write_; }

 private:
  bool minify_;
  bool use_experimental_minifier_;
  // Library identifier.  NULL if library identification should be skipped.
  const JavascriptLibraryIdentification* library_identification_;
  const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns_;

  // Statistics
  // # of JS blocks (JS files and <script> blocks) successfully minified:
  // parsed, analyzed and serialized, not necessarily made smaller;
  // num_reducing_minifications_ is counting those.
  Variable* blocks_minified_;
  // # of JS blocks that were identified as redirectable a known URL.
  Variable* libraries_identified_;
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
  // Number of times we have successfully reduced the size of JS block.
  Variable* num_reducing_minifications_;

  // Failure metrics.
  // Number of scripts we didn't rewrite JS because minification was disabled.
  Variable* minification_disabled_;
  // Number of scripts we didn't rewrite since JS didn't shrink.
  Variable* did_not_shrink_;
  // Number of scipts we failed to write out.
  Variable* failed_to_write_;

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
  // If debug_filter and AvoidRenamingIntrospectiveJavascript option are
  // turned on, this comment will be injected right after the introspective
  // Javascript context for debugging.
  static const char kIntrospectionComment[];

  JavascriptCodeBlock(const StringPiece& original_code,
                      JavascriptRewriteConfig* config,
                      const StringPiece& message_id,
                      MessageHandler* handler);

  virtual ~JavascriptCodeBlock();

  // Attempt to rewrite the file. Returns true if we should use the
  // rewritten version. Must be called before successfully_rewritten(),
  // rewritten_code() and ComputeJavascriptLibrary().
  bool Rewrite();

  // Should we use the rewritten version?
  // PRECONDITION: Rewrite() must have been called first.
  bool successfully_rewritten() const {
    DCHECK(rewritten_);
    return successfully_rewritten_;
  }
  // PRECONDITION: Rewrite() must have been called first and
  // successfully_rewritten() must be true.
  StringPiece rewritten_code() const {
    DCHECK(rewritten_);
    DCHECK(successfully_rewritten_);
    return rewritten_code_;
  }

  // Returns the contents of a source map from original to rewritten.
  // PRECONDITION: Rewrite() must have been called first and
  // successfully_rewritten() must be true.
  const source_map::MappingVector& SourceMappings() const {
    DCHECK(rewritten_);
    DCHECK(successfully_rewritten_);
    return source_mappings_;
  }

  // Annotate rewritten_code() with a source map URL.
  //
  // Call this after Rewrite() and before rewritten_code() if you want to
  // append a comment to the minified JS indicating  the URL for the source map.
  // Note: Source map URL may not be appended if url is unsanitary, but
  // this probably shouldn't happen in practice.
  void AppendSourceMapUrl(StringPiece url);

  // Is the current block a JS library that can be redirected to a canonical
  // URL?  If so, return that canonical URL (storage owned by the underlying
  // config object passed in at construction), otherwise return an empty
  // StringPiece.
  //
  // PRECONDITION: Rewrite() must have been called first.
  StringPiece ComputeJavascriptLibrary() const;

  // Swaps rewritten_code_ into *other. Afterward the JavascriptCodeBlock will
  // be cleared and unusable.
  // PRECONDITION: Rewrite() must have been called first and
  // successfully_rewritten() must be true.
  void SwapRewrittenString(GoogleString* other);

  // Determines whether the javascript is brittle and will likely
  // break if we alter its URL.
  static bool UnsafeToRename(const StringPiece& script);

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

  // Get message id passed in at creation time, for external diagnostics.
  const GoogleString& message_id() const { return message_id_; }

 private:
  // Is this URL sanitary to be appended (in a line comment) to the JS doc?
  static bool IsSanitarySourceMapUrl(StringPiece url);

  // Temporary wrapper around calling new or old version of JS minifier.
  bool MinifyJs(StringPiece input, GoogleString* output,
                source_map::MappingVector* source_mappings);

  JavascriptRewriteConfig* config_;
  const GoogleString message_id_;  // ID to stick at begining of message.
  const GoogleString original_code_;
  GoogleString rewritten_code_;
  source_map::MappingVector source_mappings_;

  // Used to make sure we don't rewrite twice and that results aren't looked at
  // before produced.
  bool rewritten_;
  bool successfully_rewritten_;

  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptCodeBlock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_CODE_BLOCK_H_
