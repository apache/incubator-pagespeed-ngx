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

#ifndef NGX_REWRITE_OPTIONS_H_
#define NGX_REWRITE_OPTIONS_H_

#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

class NgxRewriteOptions : public RewriteOptions {
 public:
  static const char kClassName[];

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
  const char* ParseAndSetOptions(StringPiece* args, int n_args);

  // Make an identical copy of these options and return it.
  virtual NgxRewriteOptions* Clone() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const NgxRewriteOptions* DynamicCast(const RewriteOptions* instance);
  static NgxRewriteOptions* DynamicCast(RewriteOptions* instance);

  // Name of the actual type of this instance as a poor man's RTTI.
  virtual const char* class_name() const;

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
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

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteOptions);
};

} // namespace net_instaweb

#endif  // NGX_REWRITE_OPTIONS_H_
