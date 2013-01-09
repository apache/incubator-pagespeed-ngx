// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_TEST_SUITE_H_
#define NET_BASE_NET_TEST_SUITE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "net/base/mock_host_resolver.h"

class MessageLoop;

namespace net {
class NetworkChangeNotifier;
}

class NetTestSuite : public base::TestSuite {
 public:
  NetTestSuite(int argc, char** argv);
  virtual ~NetTestSuite();

  virtual void Initialize();

  virtual void Shutdown();

 protected:

  // Called from within Initialize(), but separate so that derived classes
  // can initialize the NetTestSuite instance only and not
  // TestSuite::Initialize().  TestSuite::Initialize() performs some global
  // initialization that can only be done once.
  void InitializeTestThread();

 private:
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

#endif  // NET_BASE_NET_TEST_SUITE_H_
