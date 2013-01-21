// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/timer.h"
#include "base/win/object_watcher.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierWin : public NetworkChangeNotifier,
                                 public base::win::ObjectWatcher::Delegate {
 public:
  NetworkChangeNotifierWin();

 private:
  virtual ~NetworkChangeNotifierWin();

  // NetworkChangeNotifier methods:
  virtual bool IsCurrentlyOffline() const;

  // ObjectWatcher::Delegate methods:
  virtual void OnObjectSignaled(HANDLE object);

  // Begins listening for a single subsequent address change.
  void WatchForAddressChange();

  // Forwards online state notifications to parent class.
  void NotifyParentOfOnlineStateChange();

  base::win::ObjectWatcher addr_watcher_;
  OVERLAPPED addr_overlapped_;

  base::OneShotTimer<NetworkChangeNotifierWin> timer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierWin);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
