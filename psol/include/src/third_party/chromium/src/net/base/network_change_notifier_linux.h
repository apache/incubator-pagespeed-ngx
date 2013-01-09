// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierLinux : public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierLinux();

 private:
  class Thread;

  virtual ~NetworkChangeNotifierLinux();

  // NetworkChangeNotifier:
  virtual bool IsCurrentlyOffline() const;

  // The thread used to listen for notifications.  This relays the notification
  // to the registered observers without posting back to the thread the object
  // was created on.
  scoped_ptr<Thread> notifier_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierLinux);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
