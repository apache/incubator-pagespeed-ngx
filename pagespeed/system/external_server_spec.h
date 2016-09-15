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
