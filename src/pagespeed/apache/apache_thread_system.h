/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// A wrapper around SystemThreadSystem that substitutes an AprTimer.

#ifndef PAGESPEED_APACHE_APACHE_THREAD_SYSTEM_H_
#define PAGESPEED_APACHE_APACHE_THREAD_SYSTEM_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/system/system_thread_system.h"

namespace net_instaweb {

class Timer;

class ApacheThreadSystem : public SystemThreadSystem {
 public:
  ApacheThreadSystem() {}
  virtual ~ApacheThreadSystem() {}
  virtual Timer* NewTimer();

 private:
  DISALLOW_COPY_AND_ASSIGN(ApacheThreadSystem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_THREAD_SYSTEM_H_
