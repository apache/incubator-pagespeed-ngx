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

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/util/simple_random.h"

namespace net_instaweb {

uint32 SimpleRandom::Next() {
  ScopedMutex lock(mutex_.get());
  z_ = 36969 * (z_ & 65535) + (z_ >> 16);
  w_ = 18000 * (w_ & 65535) + (w_ >> 16);
  uint32 pseudo_random_number = (z_ << 16) + w_;
  return pseudo_random_number;
}

GoogleString SimpleRandom::GenerateHighEntropyString(int size) {
  ScopedMutex lock(mutex_.get());
  GoogleString value;
  value.reserve(size);
  for (int i = 0; i < size; ++i) {
    value.push_back(static_cast<char>(Next() & 0xff));
  }
  return value;
}

}  // namespace net_instaweb
