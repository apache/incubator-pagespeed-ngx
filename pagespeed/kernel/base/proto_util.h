// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_BASE_PROTO_UTIL_H_
#define PAGESPEED_KERNEL_BASE_PROTO_UTIL_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"


#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/text_format.h"

namespace net_instaweb {

// TODO(sligocki): Get rid of these special cases.
typedef google::protobuf::io::StringOutputStream StringOutputStream;
typedef google::protobuf::io::ArrayInputStream ArrayInputStream;

namespace protobuf {

// Pulls all google::protobuf namespace into net_instaweb::protobuf namespace.
using namespace google::protobuf;  // NOLINT

}  // namespace protobuf

inline bool ParseProtoFromStringPiece(
    StringPiece sp, protobuf::MessageLite* proto) {
  return proto->ParseFromArray(sp.data(), sp.size());
}

inline bool ParseTextFormatProtoFromString(const GoogleString& s,
                                           protobuf::Message* proto) {
  return google::protobuf::TextFormat::ParseFromString(s, proto);
}

}  // namespace net_instaweb


#endif  // PAGESPEED_KERNEL_BASE_PROTO_UTIL_H_
