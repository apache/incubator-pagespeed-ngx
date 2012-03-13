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

#include "net/instaweb/apache/apache_thread_system.h"
#include "net/instaweb/apache/apr_timer.h"

#include "apr_thread_proc.h"

namespace net_instaweb {

ApacheThreadSystem::ApacheThreadSystem() {}

ApacheThreadSystem::~ApacheThreadSystem() {}

void ApacheThreadSystem::BeforeThreadRunHook() {
  // We disable all signals here, since we don't want Apache's use of SIGTERM
  // to cause the 'delete everything' handle to be run everywhere.
  // (this is only needed for prefork, threaded MPMs do it already)
  apr_setup_signal_thread();
}

Timer* ApacheThreadSystem::NewTimer() {
  return new AprTimer;
}

}  // namespace net_instaweb
