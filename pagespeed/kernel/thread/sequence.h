/*
 * Copyright 2016 Google Inc.
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
//
// Abstraction for a sequence of tasks.

#ifndef PAGESPEED_KERNEL_THREAD_SEQUENCE_H_
#define PAGESPEED_KERNEL_THREAD_SEQUENCE_H_

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class Function;

// Interface for a holding and adding to a sequence of tasks.
// The mechanism for executing the tasks must be defined by
// implementations of this interface.
class Sequence {
 public:
  Sequence();
  virtual ~Sequence();

  // Adds 'function' to a sequence.  Note that this can occur at any time
  // the sequence is live -- you can add functions to a sequence that has
  // already started processing.  The caller is expected to ensure Function
  // will be cleaned up after Run or Cancel.
  //
  // 'function' can be called any time after Add(), and may in fact be
  // called before Add() returns.  It's OK for the function to call Add
  // again.
  //
  // If the sequence is destructed after Add, but before the function has
  // been run, function->Cancel() will be called when the Sequence is destroyed.
  virtual void Add(Function* function) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_SEQUENCE_H_
