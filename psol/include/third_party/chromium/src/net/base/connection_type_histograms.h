// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_
#define NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_
#pragma once

// The UpdateConnectionTypeHistograms function collects statistics related
// to the number of MD5 certificates that our users are encountering.  The
// information will help us decide when it is fine for browsers to stop
// supporting MD5 certificates, in light of the recent MD5 certificate
// collision attack (see "MD5 considered harmful today: Creating a rogue CA
// certificate" at http://www.win.tue.nl/hashclash/rogue-ca/).

namespace net {

enum ConnectionType {
  CONNECTION_ANY = 0,      // Any connection (SSL, HTTP, SPDY, etc)
  CONNECTION_SSL = 1,      // An SSL connection
  CONNECTION_SSL_MD5 = 2,  // An SSL connection with an MD5 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD2 = 3,  // An SSL connection with an MD2 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD4 = 4,  // An SSL connection with an MD4 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD5_CA = 5,  // An SSL connection with an MD5 CA certificate
                              // in the certificate chain (excluding root)
  CONNECTION_SSL_MD2_CA = 6,  // An SSL connection with an MD2 CA certificate
                              // in the certificate chain (excluding root)
  CONNECTION_HTTP = 7,     // An HTTP connection
  CONNECTION_SPDY = 8,     // A SPDY connection
  CONNECTION_SSL_SSL2 = 9,     // An SSL connection that uses SSL 2.0
  CONNECTION_SSL_SSL3 = 10,    // An SSL connection that uses SSL 3.0
  CONNECTION_SSL_TLS1 = 11,    // An SSL connection that uses TLS 1.0
  CONNECTION_SSL_TLS1_1 = 12,  // An SSL connection that uses TLS 1.1
  CONNECTION_SSL_TLS1_2 = 13,  // An SSL connection that uses TLS 1.2
  NUM_OF_CONNECTION_TYPES
};

// Update the connection type histograms.  |type| is the connection type.
void UpdateConnectionTypeHistograms(ConnectionType type);

}  // namespace net

#endif  // NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_
