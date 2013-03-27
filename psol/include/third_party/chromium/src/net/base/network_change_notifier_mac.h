// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_

#include <SystemConfiguration/SystemConfiguration.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_mac.h"

namespace net {

class NetworkChangeNotifierMac: public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierMac();
  virtual ~NetworkChangeNotifierMac();

  // NetworkChangeNotifier implementation:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE;

  // Forwarder just exists to keep the NetworkConfigWatcherMac API out of
  // NetworkChangeNotifierMac's public API.
  class Forwarder : public NetworkConfigWatcherMac::Delegate {
   public:
    explicit Forwarder(NetworkChangeNotifierMac* net_config_watcher)
        : net_config_watcher_(net_config_watcher) {}

    // NetworkConfigWatcherMac::Delegate implementation:
    virtual void Init() OVERRIDE;
    virtual void StartReachabilityNotifications() OVERRIDE;
    virtual void SetDynamicStoreNotificationKeys(
        SCDynamicStoreRef store) OVERRIDE;
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) OVERRIDE;

   private:
    NetworkChangeNotifierMac* const net_config_watcher_;
    DISALLOW_COPY_AND_ASSIGN(Forwarder);
  };

 private:
  class DnsConfigServiceThread;

  // Methods directly called by the NetworkConfigWatcherMac::Delegate:
  void StartReachabilityNotifications();
  void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  void OnNetworkConfigChange(CFArrayRef changed_keys);

  void SetInitialConnectionType();

  static void ReachabilityCallback(SCNetworkReachabilityRef target,
                                   SCNetworkConnectionFlags flags,
                                   void* notifier);

  // These must be constructed before config_watcher_ to ensure
  // the lock is in a valid state when Forwarder::Init is called.
  ConnectionType connection_type_;
  bool connection_type_initialized_;
  mutable base::Lock connection_type_lock_;
  mutable base::ConditionVariable initial_connection_type_cv_;
  base::mac::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability_;
  base::mac::ScopedCFTypeRef<CFRunLoopRef> run_loop_;

  Forwarder forwarder_;
  scoped_ptr<const NetworkConfigWatcherMac> config_watcher_;

  scoped_ptr<DnsConfigServiceThread> dns_config_service_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierMac);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
