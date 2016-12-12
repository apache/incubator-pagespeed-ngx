/*
 * Copyright 2016 Google Inc.
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
// Author: yeputons@google.com (Egor Suvorov)
#ifndef PAGESPEED_SYSTEM_EXTERNAL_SERVER_SPEC_H_
#define PAGESPEED_SYSTEM_EXTERNAL_SERVER_SPEC_H_

#include <vector>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct ExternalServerSpec {
  ExternalServerSpec() : host(), port(0) {}
  ExternalServerSpec(GoogleString host_, int port_)
      : host(host_), port(port_) {}
  bool SetFromString(StringPiece value_string, int default_port,
                     GoogleString* error_detail);

  bool empty() const { return host.empty() && port == 0; }
  GoogleString ToString() const {
    // Should be 1:1 representation of value held, used to generate signature.
    return empty() ? "" : StrCat(host, ":", IntegerToString(port));
  }

  GoogleString host;
  int port;
};

struct ExternalClusterSpec {
  bool SetFromString(StringPiece value_string, int default_port,
                     GoogleString* error_detail);

  bool empty() const { return servers.empty(); }
  GoogleString ToString() const;

  std::vector<ExternalServerSpec> servers;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_EXTERNAL_SERVER_SPEC_H_
