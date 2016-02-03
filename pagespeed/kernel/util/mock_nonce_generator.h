// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef PAGESPEED_KERNEL_UTIL_MOCK_NONCE_GENERATOR_H_
#define PAGESPEED_KERNEL_UTIL_MOCK_NONCE_GENERATOR_H_

#include "pagespeed/kernel/util/nonce_generator.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// A nonce generator that simply yields successive integers starting from 0.
class MockNonceGenerator : public NonceGenerator {
 public:
  explicit MockNonceGenerator(AbstractMutex* mutex)
      : NonceGenerator(mutex), counter_(0) { }
  virtual ~MockNonceGenerator();

 protected:
  virtual uint64 NewNonceImpl();

 private:
  uint64 counter_;

  DISALLOW_COPY_AND_ASSIGN(MockNonceGenerator);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_MOCK_NONCE_GENERATOR_H_
