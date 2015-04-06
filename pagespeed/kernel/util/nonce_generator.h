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

#ifndef PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_H_
#define PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

// Abstract base class for a generator of cryptographic nonce values (ie a
// cryptographic random number generator).
class NonceGenerator {
 public:
  virtual ~NonceGenerator();
  // Generate a fresh, ideally cryptographic, nonce.  Thread-safe.
  uint64 NewNonce();

 protected:
  // Takes ownership of mutex.
  explicit NonceGenerator(AbstractMutex* mutex) : mutex_(mutex) { }

  // Subclasses must implement this method.  Locking is already handled.
  virtual uint64 NewNonceImpl() = 0;

 private:
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(NonceGenerator);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_H_
