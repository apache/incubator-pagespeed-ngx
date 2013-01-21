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
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NET_INSTAWEB_APACHE_MOD_INSTAWEB_H_
#define NET_INSTAWEB_APACHE_MOD_INSTAWEB_H_

#include "http_config.h"
#include "httpd.h"

extern "C" {
extern module AP_MODULE_DECLARE_DATA pagespeed_module;
}

#endif  // NET_INSTAWEB_APACHE_MOD_INSTAWEB_H_
