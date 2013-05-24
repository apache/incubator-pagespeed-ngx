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

#ifndef PAGESPEED_KERNEL_UTIL_PLATFORM_H_
#define PAGESPEED_KERNEL_UTIL_PLATFORM_H_

namespace net_instaweb {

class ThreadSystem;
class Timer;

// Encapsulates the creation of objects that may have different applications
// across platforms.
class Platform {
 public:
  // Creates an appropriate ThreadSystem for the platform.
  static ThreadSystem* CreateThreadSystem();

  // Creates an appropriate Timer for the platform.
  static Timer* CreateTimer();
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_PLATFORM_H_
