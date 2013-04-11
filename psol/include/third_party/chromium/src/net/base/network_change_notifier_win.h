// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "base/win/object_watcher.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// NetworkChangeNotifierWin inherits from NonThreadSafe, as all its internal
// notification code must be called on the thread it is created and destroyed
// on.  All the NetworkChangeNotifier methods it implements are threadsafe.
class NET_EXPORT_PRIVATE NetworkChangeNotifierWin
    : public NetworkChangeNotifier,
      public base::win::ObjectWatcher::Delegate,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  NetworkChangeNotifierWin();

  // Begins listening for a single subsequent address change.  If it fails to
  // start watching, it retries on a timer.  Must be called only once, on the
  // thread |this| was created on.  This cannot be called in the constructor, as
  // WatchForAddressChangeInternal is mocked out in unit tests.
  // TODO(mmenke): Consider making this function a part of the
  //               NetworkChangeNotifier interface, so other subclasses can be
  //               unit tested in similar fashion, as needed.
  void WatchForAddressChange();

 protected:
  virtual ~NetworkChangeNotifierWin();

  // For unit tests only.
  bool is_watching() { return is_watching_; }
  void set_is_watching(bool is_watching) { is_watching_ = is_watching; }
  int sequential_failures() { return sequential_failures_; }

 private:
  class DnsConfigServiceThread;
  friend class NetworkChangeNotifierWinTest;

  // NetworkChangeNotifier methods:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE;

  // ObjectWatcher::Delegate methods:
  // Must only be called on the thread |this| was created on.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // Notifies IP address change observers of a change immediately, and notifies
  // network state change observers on a delay.  Must only be called on the
  // thread |this| was created on.
  void NotifyObservers();

  // Forwards connection type notifications to parent class.
  void NotifyParentOfConnectionTypeChange();

  // Tries to start listening for a single subsequent address change.  Returns
  // false on failure.  The caller is responsible for updating |is_watching_|.
  // Virtual for unit tests.  Must only be called on the thread |this| was
  // created on.
  virtual bool WatchForAddressChangeInternal();

  // All member variables may only be accessed on the thread |this| was created
  // on.

  // False when not currently watching for network change events.  This only
  // happens on initialization and when WatchForAddressChangeInternal fails and
  // there is a pending task to try again.  Needed for safe cleanup.
  bool is_watching_;

  base::win::ObjectWatcher addr_watcher_;
  OVERLAPPED addr_overlapped_;

  base::OneShotTimer<NetworkChangeNotifierWin> timer_;

  // Number of times WatchForAddressChange has failed in a row.
  int sequential_failures_;

  // Used for calling WatchForAddressChange again on failure.
  base::WeakPtrFactory<NetworkChangeNotifierWin> weak_factory_;

  // Thread on which we can run DnsConfigService.
  scoped_ptr<DnsConfigServiceThread> dns_config_service_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierWin);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
