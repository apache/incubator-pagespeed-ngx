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
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ThreadSystem;

// Establishes a context for VirtualHosts and directory-scoped
// options, either via .htaccess or <Directory>...</Directory>.
class ApacheConfig : public SystemRewriteOptions {
 public:
  static void Initialize();
  static void Terminate();

  ApacheConfig(const StringPiece& description, ThreadSystem* thread_system);
  explicit ApacheConfig(ThreadSystem* thread_system);
  ~ApacheConfig() {}

  bool fetch_from_mod_spdy() const {
    return fetch_from_mod_spdy_.value();
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
  template<class OptionClass>
  static void AddApacheProperty(typename OptionClass::ValueType default_value,
                                OptionClass ApacheConfig::*offset,
                                const char* id,
                                StringPiece option_name,
                                const char* help) {
    AddProperty(default_value, offset, id, option_name,
                RewriteOptions::kServerScope, help,
                apache_properties_);
  }

  static void AddProperties();
  void Init();

  Option<bool> fetch_from_mod_spdy_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
