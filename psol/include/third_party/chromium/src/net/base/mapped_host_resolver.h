// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MAPPED_HOST_RESOLVER_H_
#define NET_BASE_MAPPED_HOST_RESOLVER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_resolver.h"
#include "net/base/net_api.h"

namespace net {

// This class wraps an existing HostResolver instance, but modifies the
// request before passing it off to |impl|. This is different from
// MockHostResolver which does the remapping at the HostResolverProc
// layer, so it is able to preserve the effectiveness of the cache.
class NET_API MappedHostResolver : public HostResolver {
 public:
  // Creates a MappedHostResolver that forwards all of its requests through
  // |impl|.  It takes ownership of |impl|.
  explicit MappedHostResolver(HostResolver* impl);
  virtual ~MappedHostResolver();

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string) {
    return rules_.AddRuleFromString(rule_string);
  }

  // Takes a comma separated list of rules, and assigns them to this resolver.
  void SetRulesFromString(const std::string& rules_string) {
    rules_.SetRulesFromString(rules_string);
  }

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log);
  virtual void CancelRequest(RequestHandle req);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual HostResolverImpl* GetAsHostResolverImpl();

 private:
  scoped_ptr<HostResolver> impl_;

  HostMappingRules rules_;
};

}  // namespace net

#endif  // NET_BASE_MAPPED_HOST_RESOLVER_H_
