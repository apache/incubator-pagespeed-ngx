// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_RELOAD_TIMER_H_
#define NET_BASE_DNS_RELOAD_TIMER_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
namespace net {

// DnsReloadTimerExpired tests the thread local DNS reload timer and, if it has
// expired, returns true and resets the timer. See comments in
// host_resolver_proc.cc for details.
bool DnsReloadTimerHasExpired();

}  // namespace net
#endif

#endif  // NET_BASE_DNS_RELOAD_TIMER_H_
