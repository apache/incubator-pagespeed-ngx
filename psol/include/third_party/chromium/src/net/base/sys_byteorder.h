// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a convenience header to pull in the platform-specific
// headers that define functions for byte-order conversion,
// particularly: ntohs(), htons(), ntohl(), htonl(). Prefer including
// this file instead of directly writing the #if / #else, since it
// avoids duplicating the platform-specific selections.

#ifndef NET_BASE_SYS_BYTEORDER_H_
#define NET_BASE_SYS_BYTEORDER_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#endif  // NET_BASE_SYS_BYTEORDER_H_
