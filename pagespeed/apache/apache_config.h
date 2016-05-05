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

#ifndef PAGESPEED_APACHE_APACHE_CONFIG_H_
#define PAGESPEED_APACHE_APACHE_CONFIG_H_

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace net_instaweb {

// Establishes a context for VirtualHosts and directory-scoped
// options, either via .htaccess or <Directory>...</Directory>.
class ApacheConfig : public SystemRewriteOptions {
 public:
  static void Initialize();
  static void Terminate();

  ApacheConfig(const StringPiece& description, ThreadSystem* thread_system);
  explicit ApacheConfig(ThreadSystem* thread_system);
  virtual ~ApacheConfig();

  // Make an identical copy of these options and return it.
  virtual ApacheConfig* Clone() const;

  // Make a new empty set of options.
  virtual ApacheConfig* NewOptions() const;

  // Gets proxy authentication settings from the config file.  Returns true
  // if any settings were found, populating *name. *value and *redirect will
  // be non-empty if specified in the config file.
  bool GetProxyAuth(StringPiece* name, StringPiece* value,
                    StringPiece* redirect) const;

  void set_proxy_auth(StringPiece p) {
    set_option(p.as_string(), &proxy_auth_);
  }
  const GoogleString& proxy_auth() const { return proxy_auth_.value(); }

  bool force_buffering() const { return force_buffering_.value(); }
  void set_force_buffering(bool x) { set_option(x, &force_buffering_); }

  bool proxy_all_requests_mode() const {
    return proxy_all_requests_mode_.value();
  }

  bool measurement_proxy_mode() const {
    return !measurement_proxy_root().empty() &&
           !measurement_proxy_password().empty();
  }

  const GoogleString& measurement_proxy_root() const {
    return measurement_proxy_root_.value();
  }

  const GoogleString& measurement_proxy_password() const {
    return measurement_proxy_password_.value();
  }

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const ApacheConfig* DynamicCast(const RewriteOptions* instance);
  static ApacheConfig* DynamicCast(RewriteOptions* instance);

  void Merge(const RewriteOptions& src) override;

  OptionSettingResult ParseAndSetOptionFromName2(
      StringPiece name, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler) override;

  GoogleString SubclassSignatureLockHeld() override;

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  static Properties* apache_properties_;

  // Adds an option to apache_properties_.
  template<class OptionClass>
  static void AddApacheProperty(typename OptionClass::ValueType default_value,
                                OptionClass ApacheConfig::*offset,
                                const char* id,
                                StringPiece option_name,
                                const char* help,
                                bool safe_to_print) {
    AddProperty(default_value, offset, id, option_name,
                RewriteOptions::kServerScope, help, safe_to_print,
                apache_properties_);
  }

  static void AddProperties();
  void Init();

  Option<bool> fetch_from_mod_spdy_;
  Option<bool> force_buffering_;
  Option<bool> proxy_all_requests_mode_;
  Option<GoogleString> proxy_auth_;  // CookieName[=Value][:RedirectUrl]
  Option<GoogleString> measurement_proxy_root_;
  Option<GoogleString> measurement_proxy_password_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_CONFIG_H_
