// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_DEFAULTS_H_
#define NET_BASE_SSL_CONFIG_SERVICE_DEFAULTS_H_
#pragma once

#include "net/base/net_api.h"
#include "net/base/ssl_config_service.h"

namespace net {

// This SSLConfigService always returns the default SSLConfig settings.  It is
// mainly useful for unittests, or for platforms that do not have a native
// implementation of SSLConfigService yet.
class NET_API SSLConfigServiceDefaults : public SSLConfigService {
 public:
  SSLConfigServiceDefaults();

  // Store default SSL config settings in |config|.
  virtual void GetSSLConfig(SSLConfig* config);

 private:
  virtual ~SSLConfigServiceDefaults();

  // Default value of prefs.
  const SSLConfig default_config_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceDefaults);
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_DEFAULTS_H_
