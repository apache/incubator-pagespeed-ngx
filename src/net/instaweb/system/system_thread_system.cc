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

#include "net/instaweb/system/public/system_thread_system.h"

#include "base/logging.h"

#include "apr_thread_proc.h"  // NOLINT

namespace net_instaweb {

SystemThreadSystem::SystemThreadSystem() : may_start_threads_(false) {}
SystemThreadSystem::~SystemThreadSystem() {}

void SystemThreadSystem::PermitThreadStarting() {
  DCHECK(!may_start_threads_);
  may_start_threads_ = true;
}

void SystemThreadSystem::BeforeThreadRunHook() {
  // We disable all signals here, since the server we're hooking into is
  // probably using signals for something else and we don't want to get in the
  // way.  For example, we don't want Apache's use of SIGTERM to cause the
  // 'delete everything' handler to be run everywhere.  (This is only needed for
  // prefork, threaded MPMs do it already.)
  apr_setup_signal_thread();

  // If this fails you can get a backtrace from gdb by setting a breakpoint on
  // "pthread_create".
  DCHECK(may_start_threads_);
}

}  // namespace net_instaweb
