// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_OPENSSL_H_
#define NET_BASE_X509_UTIL_OPENSSL_H_
#pragma once

#include <openssl/asn1.h>
#include <openssl/x509v3.h>

#include <string>
#include <vector>

namespace base {
class Time;
}  // namespace base

namespace net {

// A collection of helper functions to fetch data from OpenSSL X509 certificates
// into more convenient std / base datatypes.
namespace x509_util {

bool ParsePrincipalKeyAndValueByIndex(X509_NAME* name,
                                      int index,
                                      std::string* key,
                                      std::string* value);

bool ParsePrincipalValueByIndex(X509_NAME* name, int index, std::string* value);

bool ParsePrincipalValueByNID(X509_NAME* name, int nid, std::string* value);

bool ParseDate(ASN1_TIME* x509_time, base::Time* time);

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_OPENSSL_H_
