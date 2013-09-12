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

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/system/public/system_rewrite_options.h"

namespace net_instaweb {

class NgxRewriteDriverFactory;

class NgxRewriteOptions : public SystemRewriteOptions {
 public:
  // See rewrite_options::Initialize and ::Terminate
  static void Initialize();
  static void Terminate();

  NgxRewriteOptions(const StringPiece& description, ThreadSystem* thread_system);
  explicit NgxRewriteOptions(ThreadSystem* thread_system);
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
      StringPiece* args, int n_args, ngx_pool_t* pool, MessageHandler* handler,
      NgxRewriteDriverFactory* driver_factory);

  // Make an identical copy of these options and return it.
  virtual NgxRewriteOptions* Clone() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const NgxRewriteOptions* DynamicCast(const RewriteOptions* instance);
  static NgxRewriteOptions* DynamicCast(RewriteOptions* instance);


 private:
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

  virtual OptionSettingResult ParseAndSetOptionFromName1(
      StringPiece name, StringPiece arg,
      GoogleString* msg, MessageHandler* handler);

  // We may want to override 2- and 3-argument versions as well in the future,
  // but they are not needed yet.

  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  //
  // RewriteOptions uses static initialization to reduce memory usage and
  // construction time.  All NgxRewriteOptions instances will have the same
  // Properties, so we can build the list when we initialize the first one.
  static Properties* ngx_properties_;
  static void AddProperties();
  void Init();

  // Add an option to ngx_properties_
  template<class OptionClass>
  static void add_ngx_option(typename OptionClass::ValueType default_value,
                             OptionClass NgxRewriteOptions::*offset,
                             const char* id,
                             StringPiece option_name) {
    AddProperty(default_value, offset, id, option_name, ngx_properties_);
  }

  // Helper for ParseAndSetOptions.  Returns whether the two directives equal,
  // ignoring case.
  bool IsDirective(StringPiece config_directive, StringPiece compare_directive);

  // TODO(jefftk): support fetch proxy in server and location blocks.

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteOptions);
};

}  // namespace net_instaweb

#endif  // NGX_REWRITE_OPTIONS_H_
