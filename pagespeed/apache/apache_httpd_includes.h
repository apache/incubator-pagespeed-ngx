// Copyright 2016 Google Inc.
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
// Author: cheesy@google.com (Steve Hill)
//
// Include Apache's httpd.h without conflicting with grpc OK enum.

#ifndef PAGESPEED_APACHE_APACHE_HTTPD_INCLUDES_H_
#define PAGESPEED_APACHE_APACHE_HTTPD_INCLUDES_H_

#include "httpd.h"

// Apache defines "OK" which conflicts with a gGRPC status code of the same
// name. Expand the macro out into APACHE_OK and then undefine it.
enum { APACHE_OK = OK };
#undef OK

#endif  // PAGESPEED_APACHE_APACHE_HTTPD_INCLUDES_H_
