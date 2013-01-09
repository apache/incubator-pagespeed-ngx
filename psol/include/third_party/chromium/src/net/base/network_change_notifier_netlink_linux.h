// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is to hide the netlink implementation details since the netlink.h
// header contains a struct net; which conflicts with the net namespace.  So we
// separate out all the netlink stuff into these files.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_NETLINK_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_NETLINK_LINUX_H_
#pragma once

#include <cstddef>

// Returns the file descriptor if successful.  Otherwise, returns -1.
int InitializeNetlinkSocket();

// Returns true if a network change has been detected, otherwise returns false.
bool HandleNetlinkMessage(char* buf, size_t len);

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_NETLINK_LINUX_H_
