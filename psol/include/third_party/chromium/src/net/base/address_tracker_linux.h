// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_TRACKER_LINUX_H_
#define NET_BASE_ADDRESS_TRACKER_LINUX_H_

#include <sys/socket.h>  // Needed to include netlink.
// Mask superfluous definition of |struct net|. This is fixed in Linux 2.6.38.
#define net net_kernel
#include <linux/rtnetlink.h>
#undef net

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "net/base/net_util.h"

namespace net {
namespace internal {

// Keeps track of network interface addresses using rtnetlink. Used by
// NetworkChangeNotifier to provide signals to registered IPAddressObservers.
class NET_EXPORT_PRIVATE AddressTrackerLinux
    : public MessageLoopForIO::Watcher {
 public:
  typedef std::map<IPAddressNumber, struct ifaddrmsg> AddressMap;

  // Will run |callback| when the AddressMap changes.
  explicit AddressTrackerLinux(const base::Closure& callback);
  virtual ~AddressTrackerLinux();

  // Starts watching system configuration for changes. The current thread must
  // have a MessageLoopForIO.
  void Init();

  AddressMap GetAddressMap() const;

 private:
  friend class AddressTrackerLinuxTest;

  // Returns true if |map_| changed while reading messages from |netlink_fd_|.
  bool ReadMessages();

  // Returns true if |map_| changed while reading the message from |buffer|.
  bool HandleMessage(const char* buffer, size_t length);

  // MessageLoopForIO::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE;

  base::Closure callback_;

  int netlink_fd_;
  MessageLoopForIO::FileDescriptorWatcher watcher_;

  mutable base::Lock lock_;
  AddressMap map_;
};

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_H_
