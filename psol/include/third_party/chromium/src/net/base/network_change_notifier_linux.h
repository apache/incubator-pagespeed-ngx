// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace dbus {
class Bus;
}

namespace net {

class NET_EXPORT_PRIVATE NetworkChangeNotifierLinux
    : public NetworkChangeNotifier {
 public:
  static NetworkChangeNotifierLinux* Create();

  // Unittests inject a mock bus.
  static NetworkChangeNotifierLinux* CreateForTest(dbus::Bus* bus);

 private:
  class Thread;

  explicit NetworkChangeNotifierLinux(dbus::Bus* bus);
  virtual ~NetworkChangeNotifierLinux();

  // NetworkChangeNotifier:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE;

  virtual const internal::AddressTrackerLinux*
      GetAddressTrackerInternal() const OVERRIDE;

  // The thread used to listen for notifications.  This relays the notification
  // to the registered observers without posting back to the thread the object
  // was created on.
  // Also used for DnsConfigService which requires TYPE_IO message loop.
  scoped_ptr<Thread> notifier_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierLinux);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
