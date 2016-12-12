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
// Author: yeputons@google.com (Egor Suvorov)
#include "pagespeed/system/external_server_spec.h"

#include <utility>

namespace net_instaweb {

bool ExternalServerSpec::SetFromString(
    StringPiece value_string, int default_port, GoogleString* error_detail) {
  StringPieceVector host_port;
  SplitStringPieceToVector(value_string, ":", &host_port,
                           false /* omit_empty_strings */);
  if (host_port.size() != 1 && host_port.size() != 2) {
    *error_detail = "Expected single server in format <host>[:<port>]";
    return false;
  }

  GoogleString loc_host = host_port[0].as_string();
  if (loc_host.empty()) {
    *error_detail = "Server host cannot be empty";
    return false;
  }

  int loc_port = default_port;
  if (host_port.size() == 2) {
    if (!StringToInt(host_port[1], &loc_port)) {
      *error_detail =
          StrCat("Port specified is not a valid number: '", host_port[1], "'");
      return false;
    }
    if (!(1 <= loc_port && loc_port <= 65535)) {
      *error_detail = StrCat("Invalid port: ", IntegerToString(loc_port));
      return false;
    }
  }
  host.swap(loc_host);
  port = loc_port;
  return true;
}

bool ExternalClusterSpec::SetFromString(
    StringPiece value_string, int default_port, GoogleString* error_detail) {
  if (value_string.empty()) {
    servers.clear();
    return true;
  }

  StringPieceVector str_servers;
  SplitStringPieceToVector(value_string, ",", &str_servers,
                           false /* omit_empty_strings */);

  *error_detail = "";
  std::vector<ExternalServerSpec> loc_servers;
  loc_servers.reserve(str_servers.size());
  for (StringPiece str : str_servers) {
    GoogleString current_error;
    ExternalServerSpec spec;
    if (spec.SetFromString(str, default_port, &current_error)) {
      loc_servers.emplace_back(std::move(spec));
    } else {
      if (str_servers.size() > 1) {
        if (!error_detail->empty()) {
          *error_detail += ". ";
        }
        StrAppend(error_detail, "In server '", str, "': ");
      }
      *error_detail += current_error;
    }
  }
  if (!error_detail->empty()) {
    return false;
  }
  servers.swap(loc_servers);
  return true;
}

GoogleString ExternalClusterSpec::ToString() const {
  std::vector<GoogleString> str_servers;
  str_servers.reserve(servers.size());
  for (const ExternalServerSpec& s : servers) {
    str_servers.emplace_back(s.ToString());
  }
  return JoinCollection(str_servers, ",");
}

}  // namespace net_instaweb
