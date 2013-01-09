// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_
#define NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_

#include <SystemConfiguration/SCDynamicStore.h>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Thread;
}

namespace net {

// Base class for watching the Mac OS system network settings.
class NetworkConfigWatcherMac {
 public:
  // NOTE: The lifetime of Delegate is expected to exceed the lifetime of
  // NetworkConfigWatcherMac.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called to register the notification keys on |store|.
    // Implementors are expected to call SCDynamicStoreSetNotificationKeys().
    // Will be called on the notifier thread.
    virtual void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store) = 0;

    // Called when one of the notification keys has changed.
    // Will be called on the notifier thread.
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) = 0;
  };

  explicit NetworkConfigWatcherMac(Delegate* delegate);
  virtual ~NetworkConfigWatcherMac();

 private:
  // The thread used to listen for notifications.  This relays the notification
  // to the registered observers without posting back to the thread the object
  // was created on.
  scoped_ptr<base::Thread> notifier_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigWatcherMac);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_
