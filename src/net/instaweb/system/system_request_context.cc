/*
 * Copyright 2013 Google Inc.
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

#include "net/instaweb/system/public/system_request_context.h"

#include <cstddef>                     // for size_t
#include "base/logging.h"
#include "third_party/domain_registry_provider/src/domain_registry/domain_registry.h"

namespace net_instaweb {

namespace {

GoogleString BracketIpv6(StringPiece local_ip) {
  // See http://www.ietf.org/rfc/rfc2732.txt
  // We assume the IP address is either IPv4 aa.bb.cc.dd or IPv6 with or without
  // brackets.  We add brackets if we see a : indicating an IPv6 address.
  GoogleString result;
  if (!local_ip.starts_with("[") && local_ip.find(':') != StringPiece::npos) {
    StrAppend(&result, "[", local_ip, "]");
  } else {
    local_ip.CopyToString(&result);
  }
  return result;
}

}  // namespace

SystemRequestContext::SystemRequestContext(
    AbstractMutex* logging_mutex, Timer* timer,
    StringPiece hostname_for_cache_fragmentation,
    int local_port, StringPiece local_ip)
    : RequestContext(logging_mutex, timer),
      local_port_(local_port),
      local_ip_(BracketIpv6(local_ip)) {
  set_minimal_private_suffix(MinimalPrivateSuffix(
      hostname_for_cache_fragmentation));
}

SystemRequestContext* SystemRequestContext::DynamicCast(RequestContext* rc) {
  if (rc == NULL) {
    return NULL;
  }
  SystemRequestContext* out = dynamic_cast<SystemRequestContext*>(rc);
  DCHECK(out != NULL) << "Invalid request conversion. Do not rely on RTTI for "
                      << "functional behavior. System handling flows must use "
                      << "SystemRequestContexts or a subclass.";
  return out;
}

StringPiece SystemRequestContext::MinimalPrivateSuffix(StringPiece hostname) {
  if (hostname.empty()) {
    return "";
  }

  size_t length_of_public_suffix =
      GetRegistryLength(hostname.as_string().c_str());
  if (length_of_public_suffix == 0) {
    // Unrecognized top level domain.  We don't know what kind of multi-level
    // public suffixes they might have created, so be on the safe side and
    // treat the entire hostname as a private suffix.
    return hostname;
  }

  stringpiece_ssize_type last_dot_before_private_suffix =
      hostname.rfind('.', hostname.size() - length_of_public_suffix
                     - 1 /* don't include the dot */
                     - 1 /* pos is inclusive */);
  if (last_dot_before_private_suffix == StringPiece::npos) {
    // Hostname is already a minimal private suffix.
    last_dot_before_private_suffix = 0;
  } else {
    last_dot_before_private_suffix++;  // Don't include the dot.
  }
  return hostname.substr(last_dot_before_private_suffix);
}

}  // namespace net_instaweb
