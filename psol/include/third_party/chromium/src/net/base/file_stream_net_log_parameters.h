// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File stream error reporting.

#ifndef NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_
#define NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_

#include <string>

#include "net/base/file_stream_metrics.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

// Creates NetLog parameters when a FileStream has an error.
base::Value* NetLogFileStreamErrorCallback(
    FileErrorSource source,
    int os_error,
    net::Error net_error,
    NetLog::LogLevel log_level);

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_
