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
#include <string>
#include "net/instaweb/util/public/string_util.h"

// The httpd header must be after the
// apache_rewrite_driver_factory.h. Otherwise, the compiler will
// complain "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"

namespace net_instaweb {

// Establishes a context for directory-scoped options, either via .htaccess or
// <Directory>...</Directory>.
class ApacheConfig {
 public:
  explicit ApacheConfig(const StringPiece& dir);
  ~ApacheConfig() {}

  // Human-readable description of what this configuration is for.  This
  // may be a directory, or a string indicating a combination of directives
  // for multiple directories.
  StringPiece description() const { return description_; }

  RewriteOptions* options() { return &options_; }

 private:
  std::string description_;
  RewriteOptions options_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
