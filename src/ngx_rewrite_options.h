/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

// Manage configuration for pagespeed.  Compare to ApacheConfig.
//
// TODO(jefftk): Much of the code here is copied from ApacheConfig, and is very
// similar.  It may be worth it to create an OriginRewriteOptions that both
// NgxRewriteOptions and ApacheConfig inherit from.

#ifndef NGX_REWRITE_OPTIONS_H_
#define NGX_REWRITE_OPTIONS_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

class NgxRewriteOptions : public RewriteOptions {
 public:
  // See rewrite_options::Initialize and ::Terminate
  static void Initialize();
  static void Terminate();

  NgxRewriteOptions();
  virtual ~NgxRewriteOptions() { }

  // args is an array of n_args StringPieces together representing a directive.
  // For example:
  //   ["RewriteLevel", "PassThrough"]
  // or
  //   ["EnableFilters", "combine_css,extend_cache,rewrite_images"]
  // or
  //   ["ShardDomain", "example.com", "s1.example.com,s2.example.com"]
  // Apply the directive, returning NGX_CONF_OK on success or an error message
  // on failure.
  //
  // pool is a memory pool for allocating error strings.
  const char* ParseAndSetOptions(
      StringPiece* args, int n_args, ngx_pool_t* pool, MessageHandler* handler);

  // Make an identical copy of these options and return it.
  virtual NgxRewriteOptions* Clone() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const NgxRewriteOptions* DynamicCast(const RewriteOptions* instance);
  static NgxRewriteOptions* DynamicCast(RewriteOptions* instance);

  // Name of the actual type of this instance as a poor man's RTTI.
  virtual const char* class_name() const;

  // TODO(jefftk): All these caching-related getters and setters could move to
  // an OriginRewriteOptions.
  const GoogleString& file_cache_path() const {
    return file_cache_path_.value();
  }
  void set_file_cache_path(GoogleString x) {
    set_option(x, &file_cache_path_);
  }
  int64 file_cache_clean_interval_ms() const {
    return file_cache_clean_interval_ms_.value();
  }
  void set_file_cache_clean_interval_ms(int64 x) {
    set_option(x, &file_cache_clean_interval_ms_);
  }
  int64 file_cache_clean_size_kb() const {
    return file_cache_clean_size_kb_.value();
  }
  void set_file_cache_clean_size_kb(int64 x) {
    set_option(x, &file_cache_clean_size_kb_);
  }
  int64 file_cache_clean_inode_limit() const {
    return file_cache_clean_inode_limit_.value();
  }
  void set_file_cache_clean_inode_limit(int64 x) {
    set_option(x, &file_cache_clean_inode_limit_);
  }
  int64 lru_cache_byte_limit() const {
    return lru_cache_byte_limit_.value();
  }
  void set_lru_cache_byte_limit(int64 x) {
    set_option(x, &lru_cache_byte_limit_);
  }
  int64 lru_cache_kb_per_process() const {
    return lru_cache_kb_per_process_.value();
  }
  void set_lru_cache_kb_per_process(int64 x) {
    set_option(x, &lru_cache_kb_per_process_);
  }

 private:
  // Used by class_name() and DynamicCast() to provide error checking.
  static const char kClassName[];

  // Helper methods for ParseAndSetOptions().  Each can:
  //  - return kOptionNameUnknown and not set msg:
  //    - directive not handled; continue on with other possible
  //      interpretations.
  //  - return kOptionOk and not set msg:
  //    - directive handled, all's well.
  //  - return kOptionValueInvalid and set msg:
  //    - directive handled with an error; return the error to the user.
  //
  // msg will be shown to the user on kOptionValueInvalid.  While it would be
  // nice to always use msg and never use the MessageHandler, some option
  // parsing code in RewriteOptions expects to write to a MessageHandler.  If
  // that happens we put a summary on msg so the user sees something, and the
  // detailed message goes to their log via handler.
  OptionSettingResult ParseAndSetOptions0(
      StringPiece directive, GoogleString* msg, MessageHandler* handler);
  OptionSettingResult ParseAndSetOptions1(
      StringPiece directive, StringPiece arg,
      GoogleString* msg, MessageHandler* handler);
  OptionSettingResult ParseAndSetOptions2(
      StringPiece directive, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler);
  OptionSettingResult ParseAndSetOptions3(
      StringPiece directive, StringPiece arg1, StringPiece arg2, 
      StringPiece arg3, GoogleString* msg, MessageHandler* handler);

  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  //
  // RewriteOptions uses static initialization to reduce memory usage and
  // construction time.  All NgxRewriteOptions instances will have the same
  // Properties, so we can build the list when we initialize the first one.
  static Properties* ngx_properties_;
  static void AddProperties();
  void Init();
  void InitializeSignaturesAndDefaults();

  // Add an option to ngx_properties_
  template<class RewriteOptionsSubclass, class OptionClass>
  static void add_ngx_option(typename OptionClass::ValueType default_value,
                             OptionClass RewriteOptionsSubclass::*offset,
                             const char* id,
                             OptionEnum option_enum) {
    AddProperty(default_value, offset, id, option_enum, ngx_properties_);
  }

  // Helper for ParseAndSetOptions.  Returns whether the two directives equal,
  // ignoring case.
  bool IsDirective(StringPiece config_directive, StringPiece compare_directive);

  Option<GoogleString> file_cache_path_;
  Option<int64> file_cache_clean_inode_limit_;
  Option<int64> file_cache_clean_interval_ms_;
  Option<int64> file_cache_clean_size_kb_;
  Option<int64> lru_cache_byte_limit_;
  Option<int64> lru_cache_kb_per_process_;

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteOptions);
};

} // namespace net_instaweb

#endif  // NGX_REWRITE_OPTIONS_H_
