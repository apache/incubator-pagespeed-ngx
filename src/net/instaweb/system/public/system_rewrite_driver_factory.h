// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class NonceGenerator;
class Statistics;
class ThreadSystem;

// A server context with features specific to a psol port on a unix system.
class SystemRewriteDriverFactory : public RewriteDriverFactory {
 public:
  // Takes ownership of thread_system.
  explicit SystemRewriteDriverFactory(ThreadSystem* thread_system);

  // Initializes all the statistics objects created transitively by
  // SystemRewriteDriverFactory.
  static void InitStats(Statistics* statistics);

  // Creates a HashedNonceGenerator initialized with data from /dev/random.
  NonceGenerator* DefaultNonceGenerator();

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_
