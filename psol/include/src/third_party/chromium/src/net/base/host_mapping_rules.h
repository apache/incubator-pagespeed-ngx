// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_MAPPING_RULES_H_
#define NET_BASE_HOST_MAPPING_RULES_H_
#pragma once

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "net/base/net_api.h"

namespace net {

class HostPortPair;

class NET_TEST HostMappingRules {
 public:
  HostMappingRules();
  ~HostMappingRules();

  // Modifies |*host_port| based on the current rules. Returns true if the
  // RequestInfo was modified, false otherwise.
  bool RewriteHost(HostPortPair* host_port) const;

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string);

  // Sets the rules from a comma separated list of rules.
  void SetRulesFromString(const std::string& rules_string);

 private:
  struct MapRule;
  struct ExclusionRule;

  typedef std::vector<MapRule> MapRuleList;
  typedef std::vector<ExclusionRule> ExclusionRuleList;

  MapRuleList map_rules_;
  ExclusionRuleList exclusion_rules_;

  DISALLOW_COPY_AND_ASSIGN(HostMappingRules);
};

}  // namespace net

#endif  // NET_BASE_HOST_MAPPING_RULES_H_
