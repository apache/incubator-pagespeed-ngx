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

// Author: Mark Cogan (marq@google.com)

// This header should be included (in addition to log_record.h) whenever
// code needs access to the implementations of the logging_info protobuf
// This file needs to be separate from log_record.h because both apache's
// httpd.h and any pb.h #define incompatible |OK| macros.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_LOGGING_PROTO_IMPL_H_
#define NET_INSTAWEB_HTTP_PUBLIC_LOGGING_PROTO_IMPL_H_


#include "net/instaweb/http/logging.pb.h"



#endif  // NET_INSTAWEB_HTTP_PUBLIC_LOGGING_PROTO_IMPL_H_
