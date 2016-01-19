/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_UTIL_SIMPLE_RANDOM_H_
#define PAGESPEED_KERNEL_UTIL_SIMPLE_RANDOM_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Extremely simplistic pseudo-random number generator from
// http://www.codeproject.com/Articles/25172/Simple-Random-Number-Generation
//
// Do not use this for cryptographic applications.  This is intended
// for generating high-entropy data that will not compress easily.  This class
// is thread safe.
class SimpleRandom {
 public:
  // Mutex should be created specifically for this instance, this class takes
  // ownership.
  explicit SimpleRandom(AbstractMutex* mutex)
      : z_(10), w_(25), mutex_(mutex) {}
  ~SimpleRandom() {}
  uint32 Next();
  inline uint32 NextLockHeld();

  GoogleString GenerateHighEntropyString(int size);

 private:
  uint32 z_;
  uint32 w_;
  scoped_ptr<AbstractMutex> mutex_;
  // Copy & assign are OK.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_SIMPLE_RANDOM_H_
