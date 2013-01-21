// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CERT_REQUEST_INFO_H_
#define NET_BASE_SSL_CERT_REQUEST_INFO_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/net_api.h"

namespace net {

class X509Certificate;

// The SSLCertRequestInfo class contains the info that allows a user to
// select a certificate to send to the SSL server for client authentication.
class NET_API SSLCertRequestInfo
    : public base::RefCountedThreadSafe<SSLCertRequestInfo> {
 public:
  SSLCertRequestInfo();

  void Reset();

  // The host and port of the SSL server that requested client authentication.
  std::string host_and_port;

  // A list of client certificates that match the server's criteria in the
  // SSL CertificateRequest message.  In TLS 1.0, the CertificateRequest
  // message is defined as:
  //   enum {
  //     rsa_sign(1), dss_sign(2), rsa_fixed_dh(3), dss_fixed_dh(4),
  //     (255)
  //   } ClientCertificateType;
  //
  //   opaque DistinguishedName<1..2^16-1>;
  //
  //   struct {
  //       ClientCertificateType certificate_types<1..2^8-1>;
  //       DistinguishedName certificate_authorities<3..2^16-1>;
  //   } CertificateRequest;
  std::vector<scoped_refptr<X509Certificate> > client_certs;

 private:
  friend class base::RefCountedThreadSafe<SSLCertRequestInfo>;

  ~SSLCertRequestInfo();
};

}  // namespace net

#endif  // NET_BASE_SSL_CERT_REQUEST_INFO_H_
