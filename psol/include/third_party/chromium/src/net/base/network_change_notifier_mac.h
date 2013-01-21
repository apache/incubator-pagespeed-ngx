// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#pragma once

#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCNetworkReachability.h>

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/synchronization/lock.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_mac.h"

namespace net {

class NetworkChangeNotifierMac: public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierMac();
  virtual ~NetworkChangeNotifierMac();

  // NetworkChangeNotifier implementation:
  virtual bool IsCurrentlyOffline() const;

 private:
  // Forwarder just exists to keep the NetworkConfigWatcherMac API out of
  // NetworkChangeNotifierMac's public API.
  class Forwarder : public NetworkConfigWatcherMac::Delegate {
   public:
    explicit Forwarder(NetworkChangeNotifierMac* net_config_watcher)
        : net_config_watcher_(net_config_watcher) {}

    // NetworkConfigWatcherMac::Delegate implementation:
    virtual void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store) {
      net_config_watcher_->SetDynamicStoreNotificationKeys(store);
    }
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) {
      net_config_watcher_->OnNetworkConfigChange(changed_keys);
    }

   private:
    NetworkChangeNotifierMac* const net_config_watcher_;
    DISALLOW_COPY_AND_ASSIGN(Forwarder);
  };

  // NetworkConfigWatcherMac::Delegate implementation:
  void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  void OnNetworkConfigChange(CFArrayRef changed_keys);

  static void ReachabilityCallback(SCNetworkReachabilityRef target,
                                   SCNetworkConnectionFlags flags,
                                   void* notifier);

  Forwarder forwarder_;
  const NetworkConfigWatcherMac config_watcher_;
  base::mac::ScopedCFTypeRef<CFRunLoopRef> run_loop_;
  base::mac::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability_;
  bool network_reachable_;
  mutable base::Lock network_reachable_lock_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierMac);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
