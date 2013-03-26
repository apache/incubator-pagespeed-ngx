// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_ERRORS_H__
#define NET_BASE_NET_ERRORS_H__

#include <vector>

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "net/base/net_export.h"

namespace net {

// Error domain of the net module's error codes.
NET_EXPORT extern const char kErrorDomain[];

// Error values are negative.
enum Error {
  // No error.
  OK = 0,

#define NET_ERROR(label, value) ERR_ ## label = value,
#include "net/base/net_error_list.h"
#undef NET_ERROR

  // The value of the first certificate error code.
  ERR_CERT_BEGIN = ERR_CERT_COMMON_NAME_INVALID,
};

// Returns a textual representation of the error code for logging purposes.
NET_EXPORT const char* ErrorToString(int error);

// Returns true if |error| is a certificate error code.
inline bool IsCertificateError(int error) {
  // Certificate errors are negative integers from net::ERR_CERT_BEGIN
  // (inclusive) to net::ERR_CERT_END (exclusive) in *decreasing* order.
  return error <= ERR_CERT_BEGIN && error > ERR_CERT_END;
}

// Map system error code to Error.
NET_EXPORT Error MapSystemError(int os_error);

// Returns a list of all the possible net error codes (not counting OK). This
// is intended for use with UMA histograms that are reporting the result of
// an action that is represented as a net error code.
//
// Note that the error codes are all positive (since histograms expect positive
// sample values). Also note that a guard bucket is created after any valid
// error code that is not followed immediately by a valid error code.
std::vector<int> GetAllErrorCodesForUma();

// A convenient function to translate platform file error to net error code.
NET_EXPORT Error PlatformFileErrorToNetError(
    base::PlatformFileError file_error);

}  // namespace net

#endif  // NET_BASE_NET_ERRORS_H__
