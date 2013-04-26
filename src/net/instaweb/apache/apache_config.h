// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
#define NET_INSTAWEB_APACHE_APACHE_CONFIG_H_

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ThreadSystem;

// Establishes a context for VirtualHosts and directory-scoped
// options, either via .htaccess or <Directory>...</Directory>.
class ApacheConfig : public SystemRewriteOptions {
 public:
  static void Initialize();
  static void Terminate();

  ApacheConfig(const StringPiece& dir, ThreadSystem* thread_system);
  explicit ApacheConfig(ThreadSystem* thread_system);
  ~ApacheConfig() {}

  // Human-readable description of what this configuration is for.  This
  // may be a directory, or a string indicating a combination of directives
  // for multiple directories.
  StringPiece description() const { return description_; }
  void set_description(const StringPiece& x) { x.CopyToString(&description_); }

  int64 slurp_flush_limit() const {
    return slurp_flush_limit_.value();
  }
  void set_slurp_flush_limit(int64 x) {
    set_option(x, &slurp_flush_limit_);
  }
  bool slurp_read_only() const {
    return slurp_read_only_.value();
  }
  void set_slurp_read_only(bool x) {
    set_option(x, &slurp_read_only_);
  }
  bool rate_limit_background_fetches() const {
    return rate_limit_background_fetches_.value();
  }
  const GoogleString& slurp_directory() const {
    return slurp_directory_.value();
  }
  void set_slurp_directory(GoogleString x) {
    set_option(x, &slurp_directory_);
  }

  // If this is set to true, we'll turn on our fallback proxy-like behavior
  // on non-.pagespeed. URLs without changing the main fetcher from Serf
  // (the way the slurp options would).
  bool test_proxy() const {
    return test_proxy_.value();
  }
  void set_test_proxy(bool x) {
    set_option(x, &test_proxy_);
  }

  // This configures the fetcher we use for fallback handling if test_proxy()
  // is on:
  //  - If this is empty, we use the usual mod_pagespeed fetcher
  //    (e.g. Serf)
  //  - If it's non-empty, the fallback URLs will be fetched from the given
  //    slurp directory. mod_pagespeed resource fetches, however, will still
  //    use the usual fetcher (e.g. Serf).
  GoogleString test_proxy_slurp() const {
    return test_proxy_slurp_.value();
  }

  // Helper functions
  bool slurping_enabled() const {
    return !slurp_directory().empty();
  }

  bool slurping_enabled_read_only() const {
    return slurping_enabled() && slurp_read_only();
  }

  bool experimental_fetch_from_mod_spdy() const {
    return experimental_fetch_from_mod_spdy_.value();
  }

  // Make an identical copy of these options and return it.
  virtual ApacheConfig* Clone() const;

  // Make a new empty set of options.
  virtual ApacheConfig* NewOptions() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const ApacheConfig* DynamicCast(const RewriteOptions* instance);
  static ApacheConfig* DynamicCast(RewriteOptions* instance);

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  static Properties* apache_properties_;

  // Adds an option to apache_properties_.
  template<class RewriteOptionsSubclass, class OptionClass>
  static void AddApacheProperty(typename OptionClass::ValueType default_value,
                                OptionClass RewriteOptionsSubclass::*offset,
                                const char* id,
                                OptionEnum option_enum,
                                const char* help) {
    AddProperty(default_value, offset, id, option_enum,
                RewriteOptions::kServerScope, help,
                apache_properties_);
  }

  void InitializeSignaturesAndDefaults();
  static void AddProperties();
  void Init();

  GoogleString description_;

  Option<GoogleString> slurp_directory_;
  Option<GoogleString> test_proxy_slurp_;

  Option<bool> slurp_read_only_;
  Option<bool> test_proxy_;
  Option<bool> rate_limit_background_fetches_;
  Option<bool> experimental_fetch_from_mod_spdy_;

  Option<int64> slurp_flush_limit_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
