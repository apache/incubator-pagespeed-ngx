// Copyright 2012 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)
//
// Makes sure we include Apache's http_log.h without conflicting with
// Google LOG() macros, and with proper per-module logging support in
// Apache 2.4

#ifndef NET_INSTAWEB_APACHE_APACHE_LOGGING_INCLUDES_H_
#define NET_INSTAWEB_APACHE_APACHE_LOGGING_INCLUDES_H_

// When HAVE_SYSLOG is defined, apache http_log.h will include syslog.h, which
// #defines LOG_* as numbers. This conflicts with definitions of the LOG(x)
// macros in Chromium base.
#undef HAVE_SYSLOG
#include "http_log.h"

// Apache >= 2.4 expect us to use the APLOG_USE_MODULE macro in order to
// permit per-module log-level configuration.
#ifdef APLOG_USE_MODULE
extern "C" {
APLOG_USE_MODULE(pagespeed);
}
#endif

#endif  // NET_INSTAWEB_APACHE_APACHE_LOGGING_INCLUDES_H_
